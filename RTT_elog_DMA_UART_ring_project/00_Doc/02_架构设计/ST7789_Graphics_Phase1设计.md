# ST7789 + Minimal Graphics Phase 1 设计

> 状态：DESIGN FROZEN  
> 日期：2026-09-06  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`  
> 当前目标：在已完成 SPI Platform + STM32 Impl Phase 1 的基础上，建立正式 ST7789T3 Platform Driver 与最小 ASCII Graphics/Text 能力。

---

# 1. 背景

Display Extension 已完成目标板最小 Bring-up，并确认以下硬件事实：

```text
Module      : P169H002-CTP
Controller  : ST7789T3
Resolution  : 240 x 280
Pixel       : RGB565
SPI         : SPI1 / Mode 3 / 12.5 MHz / 8 bit / MSB First
X_OFFSET    : 0
Y_OFFSET    : 20
RGB565      : high-byte first on SPI wire
Backlight   : HIGH = ON / LOW = OFF
```

同时 SPI Platform + STM32 Impl Phase 1 已完成，已提供：

```text
platform_spi_bus_t
platform_spi_device_t
platform_spi_transaction_begin()
platform_spi_write()
platform_spi_transaction_end()
```

因此本阶段不再解决“SPI 是否能驱动这块屏幕”的问题，而是把临时 Bring-up 提炼成正式设备驱动和最小文字能力。

本阶段不是通用 GUI / Display Framework 设计阶段。

---

# 2. 设计目标

本阶段目标：

```text
1. 建立正式 ST7789 Platform/BSP Driver。
2. ST7789 不直接依赖 HAL / hspi1 / CubeMX SPI internals。
3. 明确 SPI Bus、SPI Device、CS/DC/RST/BL 的资源所有权。
4. 建立完整 init / deinit / backlight lifecycle。
5. 建立 command/data transaction helper 与 table-driven init sequence。
6. 建立严格 region write：CASET / RASET / RAMWR / RGB565 pixels。
7. 支持 fill / fill_rect / draw_pixel / write_rgb565。
8. 使用小型固定 scratch buffer，不使用全屏 framebuffer。
9. 建立最小 ASCII 8x16 Graphics/Text。
10. 建立 Host Test / error / rollback 合同。
11. 本阶段不单独做 Target Verification，目标板验收延期并入后续 RTOS Display Integration。
```

不做：

```text
Display Service
Display Task / Queue / snapshot
通用 display backend / surface abstraction
GUI / widget / page framework
自动 clipping / alignment / word wrap
中文 / UTF-8
transparent text
number / float formatting API
runtime rotation
SPI DMA / IRQ transfer
full framebuffer
backlight PWM
Touch / CTP
UART 产品输出迁移
ONCE 语义迁移
```

---

# 3. 分层边界

工程稳定依赖保持：

```text
APP
 ↓
Service
 ↓
Platform
 ↓
Impl
 ↓
Vendor / HAL / RTOS / Hardware
```

本阶段显示链：

```text
APP / future Bootstrap / future Display Task
        ↓
Minimal Graphics / Text
        ↓
Platform ST7789 Driver
        ↓
platform_spi_device_t
+ Platform GPIO
+ Platform Time
        ↓
Platform SPI / GPIO / OS
        ↓
STM32 / FreeRTOS Impl
        ↓
HAL / Hardware
```

说明：

- ST7789 是具体设备能力，属于 Platform/BSP，不需要 Service。
- Graphics/Text 是基于像素设备能力衍生出的绘图能力，也不属于业务 Service。
- “系统应该显示什么”属于后续 APP / Display orchestration；若后续真实出现可复用显示业务能力，再讨论 Display Service。
- 当前不提前抽象 generic display surface/backend；待接触第二类屏幕或不同控制器后，再从真实差异中提炼通用层。

---

# 4. 文件落点

建议正式结构：

```text
03_Platform/
├─ platform_bsp/
│  └─ st7789/
│     ├─ platform_st7789.h
│     ├─ platform_st7789.c
│     ├─ platform_bsp_st7789.h
│     └─ platform_bsp_st7789.c
│
└─ platform_graphics/
   ├─ platform_graphics.h
   ├─ platform_graphics.c
   └─ font/
      ├─ platform_font.h
      ├─ platform_font_ascii_8x16.h
      └─ platform_font_ascii_8x16.c
```

禁止正式 ST7789 Driver 直接 include：

```text
05_Vendors/lcd/lcd_init.h
05_Vendors/lcd/lcdfont.h
Core/Inc/spi.h
STM32 HAL SPI headers
hspi1
```

Vendor LCD 文件只作为：

```text
已验证 init register sequence 参考
reset / Sleep Out delay 参考
MADCTL / COLMOD / gamma 等配置来源
ASCII 字模资源来源
```

---

# 5. ST7789 对象与资源所有权

采用：

```text
device-local resource ownership
+
shared SPI Bus non-owning dependency
```

概念对象：

```c
typedef struct
{
    platform_spi_device_t spiDevice;

    platform_gpio_t cs;
    platform_gpio_t dc;
    platform_gpio_t reset;
    platform_gpio_t backlight;

    platform_spi_device_config_t spiConfig;

    platform_gpio_level_t csActiveLevel;
    platform_gpio_level_t resetActiveLevel;
    platform_gpio_level_t backlightOnLevel;

    uint16_t width;
    uint16_t height;
    uint16_t xOffset;
    uint16_t yOffset;
    uint8_t madctl;

    /* small fixed TX scratch buffer, size implementation-defined */

    platform_bool_t initialized;
} platform_st7789_t;
```

生命周期关系：

```text
platform_spi_bus_t
    ↑ shared / non-owning dependency

platform_st7789_t
 ├─ owns spiDevice descriptor
 ├─ owns CS GPIO descriptor
 ├─ owns DC GPIO descriptor
 ├─ owns RST GPIO descriptor
 └─ owns BL GPIO descriptor
```

“owns”表示：

```text
对象存储责任
初始化 / 反初始化责任
```

ST7789 不拥有 SPI Bus，不得在 deinit 中 stop/deinit SPI Bus。

信号语义边界：

```text
CS  -> Platform SPI transaction controls it
DC  -> ST7789 Driver controls it
RST -> ST7789 Driver controls it
BL  -> ST7789 Driver controls it
```

ST7789 正常 command/pixel path 不直接手工切换 CS。

---

# 6. BSP Construct 合同

BSP constructor 只做静态板级/面板绑定，不做硬件初始化：

```c
platform_bsp_st7789_construct_display(&g_display);
```

当前绑定：

```text
PA4  -> CS
PA6  -> DC
PB10 -> RST
PA1  -> BL

240 x 280
X offset = 0
Y offset = 20
Mode 3
MSB First
8 bit
max clock compatible with current 12.5 MHz
CS active low
BL active high
MADCTL = current verified panel orientation
```

BSP constructor 不：

```text
configure GPIO
init SPI Device
reset controller
delay
send command
turn backlight on
```

典型使用：

```c
platform_bsp_st7789_construct_display(&g_display);
platform_st7789_init(&g_display, &g_spi1Bus);
```

---

# 7. Lifecycle / Init 合同

`platform_st7789_init()` 是完整设备初始化入口，不拆分公开 reset/controller_start API。

执行上下文：

```text
Task Context only
```

原因：正式初始化使用 `platform_time_delay_ms()`，当前实现基于 CMSIS-RTOS2 `osDelay()`，不允许在 scheduler start 前作为裸机 delay 使用。

初始化顺序：

```text
validate object / bus
 ↓
configure CS GPIO to safe state
configure DC GPIO
configure RST GPIO
configure BL GPIO -> OFF
 ↓
platform_spi_device_init()
 ↓
hardware reset
 ↓
ST7789 register init sequence
 ↓
controller READY
 ↓
initialized = TRUE
```

成功合同：

```text
platform_st7789_init() == OK
=> controller READY for drawing
=> backlight remains OFF
```

`init()` 不自动 clear/fill full screen。

理由：

```text
clear screen = pixel rendering operation
not controller lifecycle operation
```

典型未来启动流程：

```text
scheduler start
 ↓
bootstrap/startup task context
 ↓
ST7789 init
 ↓
draw boot screen
 ↓
backlight on
 ↓
system startup / diagnostics
 ↓
normal UI
```

当前 `defaultTask` 是否作为一次性 Bootstrap Thread、业务任务如何通过 startup gate 等问题，延期到 RTOS Display Integration 设计。

---

# 8. Init Failure / Deinit 合同

`initialized` 仅在全部初始化成功后置 TRUE。

任意步骤失败：

```text
preserve first/root failure
 ↓
best-effort rollback initialized resources in reverse order
 ↓
backlight OFF
 ↓
return to clean constructed + uninitialized state
```

ST7789 init 不允许出现：

```text
initialized == FALSE
but SPI Device / GPIO resources remain logically occupied
```

Deinit 概念顺序：

```text
backlight OFF
 ↓
stop device use
 ↓
platform_spi_device_deinit()
 ↓
BL GPIO deinit
RST GPIO deinit
DC GPIO deinit
CS GPIO deinit
 ↓
initialized = FALSE
```

ST7789 deinit 不 stop/deinit SPI Bus。

---

# 9. Hardware Reset

硬复位使用 private helper：

```c
static platform_error_t platform_st7789_hardware_reset(
    platform_st7789_t *display);
```

第一版继续采用已经目标板验证过的宽松 reset delay，不在本阶段优化几十毫秒：

```text
RESET ASSERT delay
RESET RELEASE delay
POST RESET delay
```

具体 delay 数值使用私有命名常量，不散落 magic number。

Delay 统一使用：

```text
platform_time_delay_ms()
```

禁止：

```text
HAL_Delay()
Vendor delay_ms()
busy loop delay
```

---

# 10. Command / Data Transaction Helper

需要两个 private helper 层次：

```text
write_command_in_transaction()
write_command()
```

`write_command_in_transaction()`：

```text
caller already owns SPI transaction
DC = COMMAND
write command byte
if dataLength > 0:
    DC = DATA
    write parameter bytes
```

`write_command()`：

```text
transaction_begin
 ↓
write_command_in_transaction
 ↓
transaction_end
```

初始化命令通常使用 `write_command()`。

区域写入需要在一个 transaction 中连续执行 CASET / RASET / RAMWR / pixel stream，因此使用 `write_command_in_transaction()`。

Delay 不允许发生在持有 SPI transaction 时。

例如 Sleep Out：

```text
transaction begin
0x11
transaction end
 ↓
delay 120 ms
```

禁止：

```text
transaction begin
0x11
delay 120 ms
transaction end
```

---

# 11. Controller Init Sequence

采用：

```text
explicit hardware reset
+
table-driven register init sequence
```

init entry 概念：

```c
typedef struct
{
    uint8_t command;
    const uint8_t *data;
    platform_size_t dataLength;
    uint16_t delayAfterMs;
} platform_st7789_init_entry_t;
```

已验证 vendor sequence 中的 B2 / B7 / BB / C0 / C2 / C3 / C4 / C6 / D0 / D6 / E0 / E1 等面板配置值，Phase 1 原样保留，不在本阶段做寄存器“优化”。

关键：

```text
0x11 Sleep Out
 -> transaction end
 -> delay 120 ms

0x29 Display On
 -> init sequence end
```

Vendor `LCD_Init()` 最后的：

```text
0x2C RAMWR
```

不进入正式 init sequence。

原因：RAMWR 表示“后续进入像素写入”，属于 region/pixel operation，而不是 controller initialization。

MADCTL / geometry / offsets 属于 panel/BSP static configuration；Phase 1 不支持 runtime rotation。

---

# 12. Region Write 基础模型

区域绘制采用：

```text
one region operation
=
one complete SPI transaction
```

流程：

```text
transaction_begin
 ↓
CASET 0x2A
 ↓
RASET 0x2B
 ↓
RAMWR 0x2C
 ↓
RGB565 pixel chunk 1
RGB565 pixel chunk 2
...
 ↓
transaction_end
```

不要拆成：

```text
CASET transaction
RASET transaction
RAMWR transaction
pixel transaction
```

内部窗口 helper 保持 private；上层不暴露 ST7789 protocol state。

建议 private 语义：

```text
prepare_region_write()
```

它负责：

```text
logical x/y/width/height
 ↓
logical -> physical offset mapping
 ↓
CASET / RASET / RAMWR
 ↓
controller ready for pixel stream
```

---

# 13. Coordinates / Offset / Boundary

公共区域参数统一：

```text
x
y
width
height
```

不用 `x1/y1/x2/y2` 作为公共 API。

内部：

```text
xEnd = x + width - 1
yEnd = y + height - 1
```

当前面板物理映射：

```text
physicalX = logicalX + 0
physicalY = logicalY + 20
```

APP / Graphics 永远只看到逻辑区域：

```text
X: 0..239
Y: 0..279
```

Phase 1 使用严格边界校验，不自动 clipping。

非法区域：

```text
return PLATFORM_ERR_INVALID_PARAM
```

推荐使用防溢出校验：

```text
x < widthLimit
requestedWidth > 0
requestedWidth <= widthLimit - x
```

Y 同理。

Clipping / wrap / layout 属于未来 Graphics/UI 层。

---

# 14. RGB565 Public Data Contract

公共像素/颜色保持 logical `uint16_t RGB565`：

```text
BLACK = 0x0000
WHITE = 0xFFFF
RED   = 0xF800
GREEN = 0x07E0
BLUE  = 0x001F
```

上层不处理 SPI wire endian。

STM32F411 little-endian 内存中的 `uint16_t` 不能直接 cast 成 `uint8_t *` 批量发送。

ST7789 Driver 必须转换为：

```text
high byte first
low byte second
```

例如：

```text
0xF800 -> F8 00
```

`write_rgb565()` 像素排列：

```text
row-major
left -> right
top -> bottom
```

---

# 15. Scratch Buffer / Chunk Contract

STM32F411 SRAM：

```text
128 KiB
```

240 x 280 RGB565 full framebuffer：

```text
134400 B
```

禁止 full framebuffer。

当前 STM32 SPI Impl 单次 HAL transfer 上限：

```text
0xFFFF bytes
```

因此 ST7789 Driver 必须天然支持 chunked write。

策略：

```text
small fixed scratch buffer
stored in display object or equivalent static storage
no runtime malloc/free
no large function-stack framebuffer
```

`write_rgb565()`：

```text
read a small block of uint16_t pixels
 ↓
convert to high-byte-first bytes
 ↓
platform_spi_write()
 ↓
repeat within same region transaction
```

`fill_rect()`：

```text
fill scratch buffer with repeated RGB565 color bytes
 ↓
repeat writes until width * height pixels complete
```

具体 scratch buffer 大小属于实施细节，可在不违反 stack/SRAM 边界前提下确定。

---

# 16. Public ST7789 API Scope

Phase 1 公共能力：

```c
platform_st7789_init(...);
platform_st7789_deinit(...);

platform_st7789_backlight_on(...);
platform_st7789_backlight_off(...);

platform_st7789_draw_pixel(...);
platform_st7789_fill(...);
platform_st7789_fill_rect(...);
platform_st7789_write_rgb565(...);
```

语义：

```text
draw_pixel
 -> convenience wrapper
 -> fill_rect(x, y, 1, 1)

fill
 -> fill_rect(full logical screen)

fill_rect
 -> repeated-color region path

write_rgb565
 -> caller-supplied pixel-buffer region path
```

`draw_pixel()` 不是新的底层传输实现。

第一版不加入：

```text
draw_line
draw_rect
draw_circle
draw_bitmap
show_char/show_string in ST7789 driver
```

这些不属于 controller driver。

基础 RGB565 颜色宏可以提供 BLACK/WHITE/RED/GREEN/BLUE；大量 UI/theme color 不进入 ST7789 Driver。

---

# 17. write_rgb565() 参数合同

概念 API：

```c
platform_error_t platform_st7789_write_rgb565(
    platform_st7789_t *display,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    const uint16_t *pixels,
    platform_size_t pixelCount);
```

要求：

```text
display initialized
width > 0
height > 0
region fully in logical display bounds
pixels != NULL
pixelCount == width * height
```

保留 `pixelCount`，用于校验调用者 buffer 数据量，避免越界读取。

---

# 18. Minimal Graphics / Text Layer

Graphics 是基于 ST7789 像素能力衍生的绘图能力，不加入 Service。

Phase 1 允许：

```text
platform_graphics
    ↓
direct dependency on platform_st7789_t
```

当前只有一类显示后端，不提前创建：

```text
platform_display_t
generic surface
backend ops
```

待未来真实接触第二类 SPI 屏 / 不同控制器后，再提炼通用 display/graphics abstraction。

---

# 19. Font Resource

当前 `05_Vendors/lcd/lcdfont.h` 已包含 ASCII 6x12、8x16 等字模。

正式代码不直接 include 整个 vendor header。

Phase 1 只提取：

```text
ASCII 8 x 16
printable ASCII 0x20..0x7E
```

形成独立 `.h/.c` 字体资源。

概念字体描述：

```c
typedef struct
{
    uint8_t width;
    uint8_t height;
    char firstChar;
    char lastChar;
    const uint8_t *glyphData;
    platform_size_t bytesPerGlyph;
} platform_font_t;
```

Graphics 不直接写死 vendor array symbol。

当前不加入：

```text
Chinese font
UTF-8 decoder
GB2312 / Unicode table
multiple font families
```

温度单位可暂时显示 `C`；以后若确有需要，只增加 degree glyph，而不因此引入完整 Unicode 系统。

---

# 20. Minimal Graphics API

Phase 1 只提供核心文字能力：

```c
platform_graphics_draw_char(...);
platform_graphics_draw_string(...);
```

参数需要能够表达：

```text
display
x / y
font
foreground RGB565
background RGB565
```

Phase 1 使用 opaque text：

```text
glyph bit = 1 -> foreground
glyph bit = 0 -> background
```

一个 8x16 glyph：

```text
128 pixels
```

Graphics 将 glyph 展开成一个小型 RGB565 pixel block，然后一次调用：

```text
platform_st7789_write_rgb565(8 x 16)
```

禁止逐像素调用 `draw_pixel()` 作为正式字符渲染路径。

字符串：

```text
A at x
B at x + fontWidth
C at x + 2 * fontWidth
...
```

第一版严格检查字符串整体区域，不自动 clipping / wrap。

不提供：

```text
show_uint
show_float
printf wrapper
text alignment
auto wrap
ellipsis
transparent text
```

业务数据格式化属于 APP / future Display logic：

```text
business value
 -> snprintf / formatting
 -> string
 -> graphics_draw_string()
```

---

# 21. Error Contract

ST7789 / Graphics 不新增专用错误体系，统一复用 `platform_error_t`。

典型：

```text
NULL_POINTER
INVALID_PARAM
NOT_INITIALIZED
ALREADY_INITIALIZED
BUSY
TIMEOUT
IO
NOT_SUPPORTED
```

底层 SPI / GPIO / Time 错误尽量原样传播。

## 21.1 Transaction Cleanup

只要 `platform_spi_transaction_begin()` 成功：

```text
caller must attempt transaction_end()
```

即使 CASET / RASET / RAMWR / pixel write 中途失败，也必须 best-effort end，避免 `activeDevice` 残留。

错误优先级：

```text
operation failed
 -> preserve/return original operation error
 -> cleanup error does not overwrite root failure

operation succeeded
transaction_end failed
 -> return transaction_end error
```

## 21.2 Init Rollback

同样遵循：

```text
first/root failure wins
cleanup is best-effort
initialized remains FALSE
```

---

# 22. Write-only Verification Limit

当前 ST7789 path 为 write-oriented，没有正式 readback verification。

因此：

```text
platform_st7789_init() == OK
```

表示 MCU 侧：

```text
GPIO / SPI / command sequence execution succeeded
```

不能证明物理面板一定正确显示。

所以长期验证策略必须包含：

```text
Host Test
+
Target Verification
```

但本阶段 Target Verification 延期并入下一阶段 RTOS Display Integration。

---

# 23. Host Test Strategy

ST7789 Host Test 使用 fake/mock：

```text
Platform SPI
Platform GPIO
Platform Time
```

重点验证：

```text
BSP construct config
GPIO safe initial state
reset sequence / delay
0x11 + 120 ms
init command order
0x29 final controller command
init does not send 0x2C
init rollback
transaction begin/end cleanup
CASET / RASET / RAMWR
logical -> physical offset
strict bounds
RGB565 high-byte-first conversion
chunk write
full-screen fill logic
draw_pixel four-corner bounds
```

Graphics Host Test mock：

```text
platform_st7789_write_rgb565()
```

验证：

```text
8x16 glyph expansion
foreground/background mapping
ASCII range
x advance by font width
string bounds
one glyph -> one region write
```

---

# 24. Target Verification Strategy

本阶段不单独要求 Target Verification。

原因：

```text
Minimal ST7789 Bring-up 已验证基础硬件链路
正式 ST7789 Phase 当前重点是架构/Host correctness
下一阶段 RTOS Display Integration 自然会调用正式 Driver + Graphics
```

因此记录为：

```text
Formal ST7789 Target Verification
= DEFERRED / MERGED INTO RTOS DISPLAY INTEGRATION
```

后续系统级板测至少覆盖：

```text
boot screen visible
ASCII text correct
four corners / offset correct
sensor data refresh correct
no obvious flower screen / corruption
long-running display update stable
UART / Control / Acquisition remain functional
```

不要把“延期”描述成“ST7789 永远不需要目标板验证”。

---

# 25. Phase Completion Criteria

本阶段实施完成条件：

```text
1. ST7789 Platform Driver implemented
2. ST7789 BSP construct/panel config implemented
3. init / deinit / backlight implemented
4. fill / fill_rect / draw_pixel / write_rgb565 implemented
5. Minimal ASCII 8x16 Graphics/Text implemented
6. focused Host Tests PASS
7. full Host regression PASS
8. Keil rebuild PASS / 0 errors
9. no new relevant warnings
10. no standalone Target Verification required
```

正式目标板验收并入下一阶段。

---

# 26. Next Phase Boundary

本设计冻结后：

```text
NEXT = create ST7789 + Minimal Graphics Phase 1 Implementation Plan
```

实施完成后再进入：

```text
RTOS Display Integration Design
```

下一阶段再讨论：

```text
one-shot bootstrap thread / defaultTask role
startup gate
boot screen contents
Display Task ownership
Display IPC / snapshot / latest-value strategy
sensor data -> display flow
main screen layout
UART periodic sensor output migration
ONCE completion semantic migration
partial refresh policy
```

不要在当前 ST7789/Graphics Phase 提前实现这些业务问题。

---

# 27. 设计冻结摘要

```text
ST7789 = Platform concrete device driver
Graphics = lightweight Platform drawing capability
No Display Service yet
No generic display abstraction yet

ST7789 owns:
    SPI Device descriptor
    CS/DC/RST/BL GPIO descriptors
ST7789 does not own SPI Bus

init:
    Task Context only
    full atomic bring-up
    backlight remains OFF
    no automatic full-screen clear
    failure -> rollback

init sequence:
    explicit hardware reset
    table-driven commands
    Sleep Out -> 120 ms after transaction end
    Display On -> sequence end
    RAMWR excluded from init

region write:
    CASET + RASET + RAMWR + pixels
    one SPI transaction
    strict bounds
    internal offset

pixels:
    uint16_t RGB565 public form
    high-byte first wire form
    small scratch buffer
    chunked transfer
    no framebuffer

public drawing:
    draw_pixel
    fill
    fill_rect
    write_rgb565

minimal text:
    ASCII 8x16 only
    draw_char / draw_string
    opaque foreground/background
    vendor font extracted, not directly included

verification:
    Host + Keil in this phase
    Target verification merged into RTOS Display Integration
```
