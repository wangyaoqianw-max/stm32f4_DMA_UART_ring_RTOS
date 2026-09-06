# ST7789 + Minimal Graphics Phase 1 Implementation Plan

> **Execution record:** 按 `superpowers:executing-plans` 逐项执行。ST7789 主线由主代理统一决策；Graphics 作为独立文件集并行实现，最终由主代理审查、修正并集成。
>
> 文档类型：Completed Implementation Record
> 状态：COMPLETE / HOST + KEIL VERIFIED
> 日期：2026-09-06
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

**Goal:** 在现有 Platform SPI/GPIO/Time 基础上，实现正式 ST7789T3 Platform concrete driver、板级构造与最小 ASCII 8x16 Graphics/Text，并通过 focused Host、full Host regression 与 Keil rebuild。

**Architecture:** ST7789 对象拥有 SPI Device descriptor 和 CS/DC/RST/BL GPIO descriptor；SPI Bus 是 shared non-owning dependency。Graphics 直接依赖具体 `platform_st7789_t`，本阶段不增加 Display Service、generic display abstraction、Task、Queue 或业务显示逻辑。

**Tech Stack:** C11 Host tests（GCC `-Wall -Wextra -Werror`）、Platform SPI/GPIO/Time、STM32F411 + CMSIS-RTOS2、Keil MDK ARM Compiler 5 工程。

**Spec:** `00_Doc/02_架构设计/ST7789_Graphics_Phase1设计.md`

## Global Constraints

- 分层固定为 `APP -> Service -> Platform -> Impl -> Vendor/HAL/RTOS/Hardware`；Platform public source/header 不得 include HAL、`main.h`、`spi.h`、`hspi1` 或 Vendor LCD header。
- ST7789 是 Platform concrete device driver；Graphics/Text 是轻量 Platform 绘图能力；不增加 Display Service 或 generic display/surface/backend ops。
- BSP construct 只做静态绑定和配置，不配置 GPIO、不延时、不发送 SPI、不打开背光。
- `platform_st7789_init()` 仅允许 Task Context；所有延时只调用 `platform_time_delay_ms()`，不修改 Time 实现。
- Init 使用 explicit hardware reset + table-driven sequence；`0x11` transaction 结束后延时 120 ms，`0x29` 为最后一条 init command，init 不发送 `0x2C`。
- 每次 region write 在一个 transaction 内完成 `CASET -> RASET -> RAMWR -> pixel chunks`；成功 begin 后必定 best-effort end，root operation error 优先于 cleanup error。
- 逻辑尺寸 `240 x 280`，内部 offset `X=0/Y=20`；严格边界，不 clipping。
- 公共像素为 `uint16_t RGB565`，SPI wire high byte first；禁止将 little-endian `uint16_t *` cast 为字节流。
- ST7789 对象使用固定 `256-byte` TX scratch buffer（128 pixels/chunk）；无 framebuffer、无 runtime malloc/free。
- Graphics 仅支持 printable ASCII `0x20..0x7E`、8x16、opaque foreground/background；每 glyph 展开为 128 个 RGB565 像素，并只调用一次 `platform_st7789_write_rgb565()`。
- Vendor `05_Vendors/lcd/lcdfont.h` 仅用于提取 `ascii_1608`；正式模块不 include Vendor header。
- 本阶段不写临时目标板测试代码；Formal ST7789 Target Verification 记录为 `DEFERRED / MERGED INTO RTOS DISPLAY INTEGRATION`。
- 本次按用户明确授权在干净的 `main` 直接施工，不创建 worktree；不自动创建 Git commit。

## Repository Facts Applied to This Plan

- LCD GPIO 的 STM32 物理资源绑定沿用现有 `04_Impl/impl_bsp/impl_platform_bsp_gpio.c`；ST7789 Platform/BSP 文件不接触 `main.h`。
- 面板逻辑尺寸、offset、MADCTL 与已验证 SPI 最大时钟集中在 `00_Config/project_config.h`；GPIO 物理映射仍归 BSP/Impl，协议命令和复位时序仍为 Driver private constants。
- 当前 SPI Bus/Device API 已完成，单次 STM32 Impl write 上限 `0xFFFF` bytes；ST7789 的 256-byte chunks 自然低于该上限。
- 屏幕规格未提供一个独立确认的最大 SCK；BSP `maxClockHz` 使用已经目标板验证的 `12500000U`，不声称或猜测更高器件上限。
- Vendor `ascii_1608` 每个字节对应一行，横向 bit 顺序为 bit0 -> bit7（LSB first）；该位序只属于字体解释，不改变 SPI MSB-first 配置。
- 现有 `app_system.c` 和四任务模型不在本阶段修改；正式显示初始化入口留给下一阶段 RTOS Display Integration Design。

---

### Task 1: ST7789 Public Object、BSP Static Construct 与 LCD GPIO Binding

**Files:**
- Create: `03_Platform/platform_bsp/st7789/platform_st7789.h`
- Create: `03_Platform/platform_bsp/st7789/platform_bsp_st7789.h`
- Create: `03_Platform/platform_bsp/st7789/platform_bsp_st7789.c`
- Modify: `03_Platform/platform_bsp/platform_bsp_gpio.h`
- Modify: `04_Impl/impl_bsp/impl_platform_bsp_gpio.c`
- Modify: `Tests/platform_bsp_gpio/main.h`
- Modify: `Tests/platform_bsp_gpio/test_platform_bsp_gpio.c`
- Create: `Tests/platform_st7789/test_platform_st7789.c`

**Interfaces:**
- Produces `platform_st7789_t`, `PLATFORM_ST7789_INITIALIZER`, geometry/color constants and all frozen public API declarations.
- Produces `platform_bsp_st7789_construct_display(platform_st7789_t *display)`.
- Produces four BSP GPIO constructors for LCD CS/DC/RST/BL, backed by PA4/PA6/PB10/PA1 in Impl BSP.

- [x] **Step 1: Write failing BSP GPIO and ST7789 static-construct tests**

  Add literal assertions that LCD constructors bind the four exact fake Port/Pin pairs and that ST7789 construct yields:

  ```text
  width=240, height=280, xOffset=0, yOffset=20, madctl=0x00
  mode=MODE_3, bitOrder=MSB_FIRST, dataBits=8, maxClockHz=12500000
  csActive=LOW, resetActive=LOW, backlightOn=HIGH
  initialized=FALSE
  zero GPIO configure/write/deinit, zero SPI transaction, zero delay
  ```

- [x] **Step 2: Verify RED**

  ```powershell
  gcc -std=c11 -Wall -Wextra -Werror -I Tests/platform_bsp_gpio -I 04_Impl/impl_mcu -I 04_Impl/impl_board -I 03_Platform/platform_bsp -I 03_Platform/platform_mcu/gpio -I 03_Platform/platform_common Tests/platform_bsp_gpio/test_platform_bsp_gpio.c 04_Impl/impl_bsp/impl_platform_bsp_gpio.c -o ..\.superpowers\sdd\st7789_phase1\test_platform_bsp_gpio.exe
  ```

  Expected: compile fails because the four LCD BSP constructors are not declared/defined.

- [x] **Step 3: Implement minimal public object, GPIO bindings and BSP construct**

  The public API is frozen as:

  ```c
  platform_error_t platform_st7789_init(platform_st7789_t *display,
                                        platform_spi_bus_t *spiBus);
  platform_error_t platform_st7789_deinit(platform_st7789_t *display);
  platform_error_t platform_st7789_backlight_on(platform_st7789_t *display);
  platform_error_t platform_st7789_backlight_off(platform_st7789_t *display);
  platform_error_t platform_st7789_draw_pixel(platform_st7789_t *display,
                                              uint16_t x, uint16_t y,
                                              uint16_t color);
  platform_error_t platform_st7789_fill(platform_st7789_t *display,
                                        uint16_t color);
  platform_error_t platform_st7789_fill_rect(platform_st7789_t *display,
                                             uint16_t x, uint16_t y,
                                             uint16_t width, uint16_t height,
                                             uint16_t color);
  platform_error_t platform_st7789_write_rgb565(platform_st7789_t *display,
                                                uint16_t x, uint16_t y,
                                                uint16_t width, uint16_t height,
                                                const uint16_t *pixels,
                                                platform_size_t pixelCount);
  ```

  `platform_st7789_t` stores `platform_spi_device_t`, four `platform_gpio_t`, SPI/panel config, `uint8_t scratchBuffer[256]`, and `initialized`.

- [x] **Step 4: Run Task 1 GREEN tests**

  Compile/run `platform_bsp_gpio` and the new `platform_st7789` test binary. Expected: exit code 0; constructor has no hardware side effects.

- [x] **Step 5: Inspect diff checkpoint**

  ```powershell
  git diff --check
  git status --short
  ```

---

### Task 2: GPIO + SPI Device Lifecycle、Backlight 与 Hardware Reset

**Files:**
- Create: `03_Platform/platform_bsp/st7789/platform_st7789.c`
- Modify: `Tests/platform_st7789/test_platform_st7789.c`

**Consumes:** Task 1 public object and existing Platform SPI/GPIO/Time APIs.
**Produces:** init/deinit/backlight lifecycle, reset helper and rollback foundation.

- [x] **Step 1: Add failing lifecycle tests**

  Cover NULL/unconstructed/duplicate init; safe GPIO output config (`CS/DC/RST=HIGH`, `BL=LOW`); SPI Device init; backlight remains OFF; backlight on/off level mapping; reset event sequence `RST LOW -> delay 100 -> RST HIGH -> delay 100 -> delay 100`; deinit never stops/deinits SPI Bus.

- [x] **Step 2: Verify RED**

  Compile the focused ST7789 test with `platform_st7789.c`, `platform_bsp_st7789.c`, Platform SPI/GPIO and Platform common sources. Expected: link/test failure because lifecycle functions are not implemented.

- [x] **Step 3: Implement minimal lifecycle and reset helpers**

  Configure GPIO in CS/DC/RST/BL order, initialize SPI Device after all GPIOs, and rollback configured resources in reverse order on any failure. Store the first error; cleanup errors never replace it. Deinit attempts BL OFF, SPI Device deinit, then BL/RST/DC/CS GPIO deinit, without touching the Bus lifecycle.

- [x] **Step 4: Run focused GREEN test**

  Expected: lifecycle/reset tests exit 0 and the Bus state remains `PLATFORM_OBJECT_STARTED` after ST7789 deinit.

---

### Task 3: Command/Data Helpers 与 Table-driven Controller Init

**Files:**
- Modify: `03_Platform/platform_bsp/st7789/platform_st7789.c`
- Modify: `Tests/platform_st7789/test_platform_st7789.c`

**Produces:** private `write_command_in_transaction()`, `write_command()` and table-driven verified Vendor sequence.

- [x] **Step 1: Add failing init-protocol tests**

  Assert command order and literal parameters for:

  ```text
  11
  B2 0C 0C 00 33 33
  35 00
  36 00
  3A 05
  B7 35
  BB 2D
  C0 2C
  C2 01
  C3 15
  C4 20
  C6 0F
  D0 A4 A1
  D6 A1
  E0 70 05 0A 0B 0A 27 2F 44 47 37 14 14 29 2F
  E1 70 07 0C 08 08 04 2F 33 46 18 15 15 2B 2D
  21
  29
  ```

  Also assert: `0x11` transaction is ended before `platform_time_delay_ms(120U)`; `0x29` is final; init command stream contains no `0x2C`; no delay occurs while Bus has an active transaction.

- [x] **Step 2: Verify RED**

  Expected: init returns failure or command/delay assertions fail because the controller sequence is absent.

- [x] **Step 3: Implement table-driven init and command helpers**

  Use immutable parameter arrays and an init-entry table. The MADCTL entry obtains its one data byte from `display->madctl`. Each table entry uses its own begin/end transaction; delay is called only after end. `0x29` has no following RAMWR entry.

- [x] **Step 4: Add and pass error/cleanup tests**

  Inject SPI write, DC GPIO and delay failures. Assert successful begin always reaches end; operation error wins over end failure; init rollback leaves `initialized=FALSE`, `spiDevice.initialized=FALSE`, Bus unowned, backlight OFF, and all successfully configured GPIOs deinitialized best-effort.

---

### Task 4: Region Window、Logical Offset 与 Strict Bounds

**Files:**
- Modify: `03_Platform/platform_bsp/st7789/platform_st7789.c`
- Modify: `Tests/platform_st7789/test_platform_st7789.c`

**Produces:** private validated `prepare_region_write()` path shared by both public pixel writers.

- [x] **Step 1: Add failing region tests**

  For logical `(x=1,y=2,width=3,height=4)`, assert one transaction emits:

  ```text
  2A 00 01 00 03
  2B 00 16 00 19
  2C
  ```

  where physical Y is logical Y + 20. Assert CASET/RASET/RAMWR are not split into separate transactions.

- [x] **Step 2: Add failing strict-bound tests**

  Reject zero width/height, `x>=240`, `y>=280`, `width > 240-x`, `height > 280-y`, wrong `pixelCount`, NULL pixels and uninitialized display with deterministic existing `platform_error_t`; invalid calls produce no transaction.

- [x] **Step 3: Implement overflow-safe region validation and private window helper**

  Use subtraction-form checks (`requested <= limit - start`) and compute inclusive ends only after validation. Keep the helper private.

- [x] **Step 4: Run focused GREEN tests**

  Expected: region/bounds cases exit 0; every successful region operation leaves `bus->activeDevice == NULL`.

---

### Task 5: RGB565 Conversion、Chunk Transfer、draw_pixel/fill/fill_rect/write_rgb565

**Files:**
- Modify: `03_Platform/platform_bsp/st7789/platform_st7789.c`
- Modify: `Tests/platform_st7789/test_platform_st7789.c`

**Produces:** all four frozen drawing APIs using the 256-byte object scratch buffer.

- [x] **Step 1: Add failing RGB565 and chunk tests**

  Assert `0xF800, 0x07E0, 0x001F` transmit as `F8 00 07 E0 00 1F`. For 130 input pixels, assert two pixel writes of 256 and 4 bytes inside the same region transaction. Force second chunk failure and assert transaction cleanup/root-error preservation.

- [x] **Step 2: Implement `write_rgb565()`**

  Convert at most 128 pixels per iteration into `display->scratchBuffer`; never cast the caller pixel pointer to bytes; keep one transaction over all chunks.

- [x] **Step 3: Add failing fill/fill_rect/draw_pixel tests**

  Assert repeated-color bytes, `draw_pixel` behavior at `(0,0)`, `(239,0)`, `(0,279)`, `(239,279)`, and full-screen fill totals exactly `240*280*2 = 134400` pixel bytes with maximum write length 256. Assert `draw_pixel` is behaviorally equivalent to a 1x1 fill_rect and `fill` to a 240x280 fill_rect.

- [x] **Step 4: Implement minimal wrappers and repeated-color chunking**

  `draw_pixel()` returns `platform_st7789_fill_rect(display,x,y,1U,1U,color)`. `fill()` returns `platform_st7789_fill_rect(display,0U,0U,display->width,display->height,color)`. `fill_rect()` pre-fills the scratch buffer in high-byte-first form and reuses it until the logical pixel count is exhausted.

- [x] **Step 5: Run complete focused ST7789 test**

  Expected: all BSP/lifecycle/init/rollback/transaction/region/endian/chunk/fill/four-corner tests exit 0 with `-Werror`.

---

### Task 6: Independent ASCII 8x16 Font Resource

**Files:**
- Create: `03_Platform/platform_graphics/font/platform_font.h`
- Create: `03_Platform/platform_graphics/font/platform_font_ascii_8x16.h`
- Create: `03_Platform/platform_graphics/font/platform_font_ascii_8x16.c`
- Create: `Tests/platform_graphics/test_platform_graphics.c`

**Produces:** `platform_font_t` and `g_platformFontAscii8x16` for printable ASCII only.

- [x] **Step 1: Extract exactly Vendor `ascii_1608` entries 0..94**

  Store the 95 glyphs as private immutable data in the new `.c`. The public descriptor is exactly:

  ```c
  const platform_font_t g_platformFontAscii8x16 = {
      8U, 16U, 0x20U, 0x7EU, &fontData[0][0], 16U
  };
  ```

  Do not include or compile `lcdfont.h` from the new module.

- [x] **Step 2: Add compile/descriptor checks**

  Compile the font with the Graphics Host test and assert width/height/range/bytes-per-glyph plus hand-checked glyph rows: space row 0=`0x00`, `A` row 3=`0x08`, `A` row 13=`0xE7`, `~` row 0=`0x0C`.

- [x] **Step 3: Verify GREEN and dependency boundary**

  ```powershell
  rg -n "lcdfont|05_Vendors|HAL_|hspi1" 03_Platform/platform_graphics 03_Platform/platform_bsp/st7789
  ```

  Expected: no matches.

---

### Task 7: Minimal Graphics draw_char / draw_string

**Files:**
- Create: `03_Platform/platform_graphics/platform_graphics.h`
- Create: `03_Platform/platform_graphics/platform_graphics.c`
- Modify: `Tests/platform_graphics/test_platform_graphics.c`

**Interfaces:**
- Consumes `platform_st7789_write_rgb565()` and `platform_font_t`.
- Produces:

  ```c
  platform_error_t platform_graphics_draw_char(
      platform_st7789_t *display, uint16_t x, uint16_t y,
      char_t character, const platform_font_t *font,
      uint16_t foreground, uint16_t background);
  platform_error_t platform_graphics_draw_string(
      platform_st7789_t *display, uint16_t x, uint16_t y,
      const char_t *text, const platform_font_t *font,
      uint16_t foreground, uint16_t background);
  ```

- [x] **Step 1: Add failing glyph expansion test**

  Mock only `platform_st7789_write_rgb565()`. For `A` row byte `0x08`, assert eight row-major pixels are `BG,BG,BG,FG,BG,BG,BG,BG` (LSB-first). Assert one character makes exactly one `8x16`, `pixelCount=128` region write.

- [x] **Step 2: Implement `draw_char()`**

  Validate display/font/character and exact Phase 1 font geometry; expand into a local `uint16_t glyphPixels[128]`; call `platform_st7789_write_rgb565()` once. Do not call `draw_pixel()`.

- [x] **Step 3: Add failing printable/string tests**

  Reject characters below `0x20` and above `0x7E`; assert `"AB"` writes at `x` and `x+8`; reject a complete string when width or height exceeds display bounds before drawing its first glyph; propagate the first glyph write error; accept the empty string as a no-op with zero writes.

- [x] **Step 4: Implement `draw_string()` and pass focused Graphics tests**

  Pre-scan the complete NUL-terminated string for printable range and overflow-safe total bounds, then render left-to-right. Expected focused Graphics test: compile/run exit 0 with `-Werror`.

---

### Task 8: Keil Production Integration

**Files:**
- Modify: `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx`

- [x] **Step 1: Add include paths**

  Add exactly:

  ```text
  ../03_Platform/platform_bsp/st7789
  ../03_Platform/platform_graphics
  ../03_Platform/platform_graphics/font
  ```

- [x] **Step 2: Add production sources to focused groups**

  ```text
  platform/platform_bsp/st7789:
    platform_st7789.c
    platform_bsp_st7789.c

  platform/platform_graphics:
    platform_graphics.c
    platform_font_ascii_8x16.c
  ```

  `impl_platform_bsp_gpio.c` is already registered and is modified in place; do not register any Host test or Vendor LCD source.

- [x] **Step 3: Run Keil full rebuild**

  ```powershell
  & 'E:\APP\ProgramFile\MDK\Core\UV4\UV4.exe' -r 'E:\my_project_2026\Git_test\stm32f4_DMA_UART_ring_RTOS\RTT_elog_DMA_UART_ring_project\MDK-ARM\RTT_elog_DMA_UART_ring_project.uvprojx' -o 'E:\my_project_2026\Git_test\stm32f4_DMA_UART_ring_RTOS\.superpowers\sdd\st7789_phase1\keil-rebuild.log'
  ```

  Expected: `0 Error(s)`; record total warnings and prove no warning is attributed to the new/modified relevant production files.

---

### Task 9: Focused、Full Host Regression、Diff 与 Architecture Boundary Verification

**Files:**
- Create ignored verification helper: `.superpowers/sdd/st7789_phase1/run_host_regression.ps1`

- [x] **Step 1: Run focused tests**

  ```text
  platform_bsp_gpio
  platform_st7789
  platform_graphics
  platform_spi
  impl_platform_spi
  ```

  Each group must compile under GCC C11 with `-Wall -Wextra -Werror`, then run with exit code 0.

- [x] **Step 2: Run all existing Host groups plus the two new groups**

  The ignored helper contains the exact include/source map for every directory under `Tests` that owns a `test_*.c` Host entry. It stops on the first compile/run failure and prints a final passed-group count. Expected baseline is previous 36 groups + `platform_st7789` + `platform_graphics` = 38 groups, adjusted only if current repository enumeration proves a different exact count; record the enumerated count and names.

- [x] **Step 3: Run repository checks**

  ```powershell
  git diff --check
  rg -n 'HAL_|hspi1|main\.h|#include\s*[<"]spi\.h[>"]|lcdfont|lcd_init' RTT_elog_DMA_UART_ring_project/03_Platform/platform_bsp/st7789 RTT_elog_DMA_UART_ring_project/03_Platform/platform_graphics
  rg -n "malloc|calloc|realloc|free" RTT_elog_DMA_UART_ring_project/03_Platform/platform_bsp/st7789 RTT_elog_DMA_UART_ring_project/03_Platform/platform_graphics
  git status --short
  ```

  Expected: diff check passes; forbidden dependency/allocation searches return no matches; only planned production/test/doc/project files are modified or added.

---

### Task 10: Completion Record、Handoff、Architecture 与 Roadmap

**Files:**
- Modify: `00_Doc/04_Agent/implementation_plan.md`
- Modify: `00_Doc/04_Agent/handoff.md`
- Modify: `00_Doc/04_Agent/architecture.md` only for stable implemented details not already frozen
- Modify: `00_Doc/04_Agent/development_roadmap.md`

- [x] **Step 1: Convert this plan into an actual completion record**

  Mark every executed checkbox, change document type/status to completed, and add exact evidence: focused group results, full regression count, Keil errors/warnings, `git diff --check`, architecture boundary result, scratch strategy and deviations.

- [x] **Step 2: Update long-term entry documents**

  Record `ST7789 + Minimal Graphics Phase 1 = COMPLETE / HOST + KEIL VERIFIED`, Target Verification deferral reason, actual public API, 256-byte chunking and any repository-fact adjustment. Do not claim target verification.

- [x] **Step 3: Freeze the stop point**

  Every long-term document must point to:

  ```text
  NEXT = RTOS Display Integration Design
  ```

  Do not create or execute an RTOS Display Integration implementation plan in this phase.

- [x] **Step 4: Final verification after documentation edits**

  Re-run `git diff --check`, inspect `git diff --stat`, and re-read the final Host/Keil log summaries before reporting completion.

---

## Completion Evidence

```text
Coding Standard baseline
  00_Doc/02_架构设计/嵌入式项目C代码设计规范.md / READ
  Coding Standard Review / PASS

Focused Host
  platform_bsp_gpio / PASS
  platform_st7789 / PASS
  platform_graphics / PASS
  platform_spi / PASS
  impl_platform_spi / PASS

Full Host regression
  PASS / 38 of 38 discovered test_*.c entries
  GCC C11 / -Wall -Wextra -Werror
  Existing platform_log-only unused-parameter warning remains locally suppressed

Keil rebuild
  Arm Compiler 5.06 update 7
  0 errors / 13 pre-existing warnings
  0 warnings in new or modified relevant production files

Architecture and resource checks
  git diff --check / PASS
  C/H file headers, tabs, 120-column and Yoda scan / PASS
  no HAL, hspi1, CubeMX spi.h, main.h or Vendor LCD dependency in ST7789/Graphics
  no runtime allocation or framebuffer
  shared SPI Bus remains non-owning and is not stopped by ST7789 deinit
  invalid/unstarted SPI Bus is rejected before GPIO or reset side effects

Static configuration
  width / height / offsets / MADCTL / verified maxClockHz are centralized in
  00_Config/project_config.h
  protocol commands and reset delays remain private Driver constants

Target Verification
  DEFERRED / MERGED INTO RTOS DISPLAY INTEGRATION

Next
  RTOS Display Integration Design
```

Repository-fact corrections made during execution:

- Vendor `ascii_1608` 的 `~` row 0 实际为 `0x0C`，不是初始计划误写的 `0x16`；最终字体 1520 bytes 与 Vendor 95×16 数据逐字节一致。
- 架构扫描已将禁止项收窄为精确 CubeMX `#include "spi.h"`，不会误报合法 `platform_spi.h`。
- `platform_st7789_init()` 增加 Bus object/class/STARTED 前置验证，任何无效 Bus 调用均不产生 GPIO、SPI 或 reset 副作用。
