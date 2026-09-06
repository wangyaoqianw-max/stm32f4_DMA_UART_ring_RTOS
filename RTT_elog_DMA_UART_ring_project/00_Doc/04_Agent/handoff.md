# 工程长期记忆与交接说明

更新时间：2026-09-06

> 本文件是 AI Agent / Codex 与人工开发者恢复工程上下文时的长期入口。  
> Phase 1~9 Core Application 已完成并通过 Host / Keil / Target 综合验证。  
> Display Extension 已完成硬件资源确认、CubeMX SPI1 + LCD GPIO、ST7789T3 最小 Bring-up、SPI Platform + STM32 Impl Phase 1。  
> ST7789 + Minimal Graphics Phase 1 已实现并通过 Host + Keil 验证。
> 当前没有 Active Implementation Plan。下一步只进入 RTOS Display Integration Design。

---

# 0. 当前状态总览

```text
Phase 1~9 Core Application               COMPLETE / TARGET VERIFIED
Final Integrated Board Test              PASS

Display Hardware Resource Review         COMPLETE
CubeMX SPI1 + LCD GPIO                    COMPLETE
Minimal ST7789 Bring-up                   TARGET VERIFIED
Temporary Bring-up Code                   REVERTED
SPI Platform + STM32 Impl Phase 1         COMPLETE / HOST + KEIL VERIFIED
ST7789 + Minimal Graphics Phase 1 Design  FROZEN
ST7789 + Minimal Graphics Implementation  COMPLETE / HOST + KEIL VERIFIED
Display Task / IPC                        NOT DESIGNED
UART Product Output Migration             NOT DESIGNED
ONCE Semantic Migration                   NOT DESIGNED
Touch / CTP                               DEFERRED

Current Active Implementation Plan        NONE
```

正式设计文档：

```text
00_Doc/02_架构设计/ST7789_Graphics_Phase1设计.md
```

下一正式动作：

```text
RTOS Display Integration Design
```

---

# 1. 工程稳定分层

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

允许：

```text
APP -> Service
APP -> Platform
Service -> Platform
Platform -> Impl
```

禁止：

```text
APP -> Impl
Service -> Impl
```

职责：

```text
APP      : 业务状态、任务调度、业务编排
Service  : 可复用业务能力，不绑定具体 MCU
Platform : 设备 / OS / 基础绘图能力
Impl     : STM32 / FreeRTOS 等具体适配
Vendor   : HAL / CMSIS / FreeRTOS / third-party
```

CubeMX generated files 只承担硬件初始化、scheduler bootstrap、IRQ/HAL callback 与 thin glue。

---

# 2. Core Application Baseline

```text
Phase 1  GPIO STM32 Impl                         COMPLETED
Phase 2  Board Resource + CubeMX Configuration   COMPLETED
Phase 3  Software I2C                            COMPLETED
Phase 4  LED Module                              COMPLETED
Phase 5  Button Module                           COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 6  DHT20 Environment Module                COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 7  MPU6050 Motion Module                   COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 8  UART Application Communication          COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 9  Final RTOS Application Integration      COMPLETED / HOST + KEIL + TARGET VERIFIED
```

稳定四任务：

```text
Communication Task   2048 B   ABOVE_NORMAL
Control Task         1024 B   ABOVE_NORMAL
Acquisition Task     1536 B   NORMAL
Indicator Task        768 B   BELOW_NORMAL
```

CubeMX `defaultTask` 不是第五个产品 Task。

APP 唯一业务状态：

```text
STOPPED
RUNNING
```

Button：

```text
SINGLE -> START
LONG   -> STOP
DOUBLE -> ONCE
```

UART：

```text
START / STOP / ONCE / STATUS / HELP
```

Unified Acquisition：

```text
DHT20 read
 -> MPU6050 read
 -> complete atomic result
```

当前 ONCE success 仍是：

```text
DHT20 success
AND MPU6050 success
AND complete UART report TX success
```

Display 接入后必须重新设计 ONCE completion semantic；不要直接把 `communicationQueue` 换成 `displayQueue`。

---

# 3. UART DMA + RingBuffer Baseline

RX：

```text
USART1 RX
 -> DMA Circular
 -> IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform UART
 -> UART Service
 -> SPSC RingBuffer
 -> Communication Task
```

TX：

```text
Communication Task
 -> UART Service
 -> Platform UART async
 -> STM32 UART Impl
 -> HAL_UART_Transmit_DMA()
```

Communication Task 是 sole USART1 product TX requester。

Display Extension 未来可能停止周期 sensor UART TX，但尚未冻结。

---

# 4. Display Hardware Contract

```text
Module      : P169H002-CTP
Controller  : ST7789T3
Resolution  : 240 x 280
Interface   : 4-wire SPI display path
Pixel       : RGB565
Touch       : OUT OF CURRENT STAGE
```

引脚：

```text
PA1  -> LCD_BL
PA4  -> LCD_CS
PA5  -> SPI1_SCK
PA6  -> LCD_DC
PA7  -> SPI1_MOSI
PB10 -> LCD_RST
```

默认 GPIO：

```text
CS HIGH
DC HIGH
RST HIGH
BL LOW
```

最小 Bring-up 已验证：

```text
SPI1 Mode 3 @ 12.5 MHz
240 x 280
X_OFFSET = 0
Y_OFFSET = 20
RGB565 high-byte first
BL HIGH = ON
BLACK / WHITE / RED / GREEN / BLUE PASS
```

临时 Bring-up 代码已经回退。

---

# 5. SPI Platform + STM32 Impl Phase 1

正式设计：

```text
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
```

稳定模型：

```text
platform_spi_bus_t
 = MCU SPI Controller / Bus
 = Platform lifecycle device
 = ops + implContext + activeDevice

platform_spi_device_t
 = lightweight SPI slave descriptor
 = bus reference
 = optional CS GPIO
 = CS active level
 = SPI device config
 = initialized state
```

公共 transaction API：

```text
platform_spi_transaction_begin(device)
platform_spi_write(device, data, length)
platform_spi_transaction_end(device)
```

规则：

```text
begin success -> caller owns transaction
write requires activeDevice == device
write does not auto-end
successful begin must call end
second begin while active -> BUSY
wrong-device write/end -> INVALID_STATE
```

当前能力：

```text
blocking synchronous TX only
8-bit only
optional software CS
fixed CubeMX config validation
no runtime dynamic reconfiguration
```

STM32 Impl：

```text
HAL_SPI_Transmit()
finite timeout = 1000 ms
single HAL transfer max = 0xFFFF bytes
```

---

# 6. ST7789 + Minimal Graphics Phase 1 冻结设计

正式设计：

```text
00_Doc/02_架构设计/ST7789_Graphics_Phase1设计.md
```

正式链：

```text
APP / future Bootstrap / Display Task
        ↓
Minimal Graphics / Text
        ↓
Platform ST7789 Driver
        ↓
Platform SPI + GPIO + Time
        ↓
STM32 / FreeRTOS Impl
```

当前不增加 Display Service，不抽象 generic display backend/surface。

原因：这是第一次正式接入 SPI TFT；先把真实设备边界做稳，后续接触更多屏幕后再从真实差异中提炼复用层。

建议文件：

```text
03_Platform/platform_bsp/st7789/
    platform_st7789.h/.c
    platform_bsp_st7789.h/.c

03_Platform/platform_graphics/
    platform_graphics.h/.c
    font/platform_font.h
    font/platform_font_ascii_8x16.h/.c
```

---

# 7. ST7789 Resource / Lifecycle Contract

资源模型：

```text
platform_st7789_t owns:
    SPI Device descriptor
    CS GPIO descriptor
    DC GPIO descriptor
    RST GPIO descriptor
    BL GPIO descriptor

SPI Bus:
    shared non-owning dependency
```

信号边界：

```text
CS  -> Platform SPI transaction
DC  -> ST7789
RST -> ST7789
BL  -> ST7789
```

BSP constructor：

```text
static board/panel binding only
no GPIO configure
no delay
no command
```

`platform_st7789_init()`：

```text
Task Context only
configure GPIO
init SPI Device
hardware reset
register init sequence
controller READY
initialized TRUE
backlight remains OFF
```

`init()` 不自动 clear/fill full screen。

失败：

```text
preserve first/root error
best-effort rollback
return clean uninitialized state
```

ST7789 deinit 不 stop/deinit SPI Bus。

---

# 8. ST7789 Init / Region / Pixel Contract

初始化：

```text
explicit hardware reset
+
table-driven command sequence
```

关键：

```text
0x11 Sleep Out
 -> transaction end
 -> delay 120 ms

0x29 Display On
 -> init sequence end
```

Vendor 参考代码最后的 `0x2C RAMWR` 不进入正式 init。

区域写：

```text
one region operation = one SPI transaction

CASET
RASET
RAMWR
pixel chunks
```

公共坐标：

```text
x / y / width / height
```

严格边界，不自动 clipping。

逻辑区域：

```text
X 0..239
Y 0..279
```

内部 offset：

```text
X + 0
Y + 20
```

公共像素：

```text
uint16_t RGB565
```

SPI wire：

```text
high byte first
```

禁止全屏 framebuffer：

```text
240 * 280 * 2 = 134400 B > 128 KiB SRAM
```

使用：

```text
small fixed scratch buffer
chunked SPI write
no runtime malloc/free
```

---

# 9. ST7789 / Graphics Public Scope

ST7789 Phase 1：

```text
init / deinit
backlight_on / backlight_off
draw_pixel
fill
fill_rect
write_rgb565
```

复用：

```text
draw_pixel -> fill_rect(1x1)
fill       -> fill_rect(full screen)
```

Minimal Graphics/Text：

```text
ASCII 8x16 only
printable ASCII 0x20..0x7E
opaque foreground/background
draw_char
draw_string
```

Vendor `lcdfont.h` 只作为字模来源，不直接 include 到正式模块。

当前不做：

```text
Chinese / UTF-8
transparent text
alignment / wrap
show_uint / show_float / printf wrapper
GUI / widget
```

业务数据格式化属于 APP / future Display logic。

---

# 10. Error / Verification Contract

统一复用 `platform_error_t`，不增加 ST7789-specific error hierarchy。

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

规则：

```text
begin success
 -> 后续即使失败，也必须 best-effort transaction_end

operation error > cleanup error
```

当前 ST7789 path 是 write-oriented，因此 Host success 不能证明物理 panel 一定正确显示。

长期验证必须有：

```text
Host Test + Target Verification
```

但本阶段：

```text
Formal ST7789 standalone Target Verification
= DEFERRED / MERGED INTO RTOS DISPLAY INTEGRATION
```

本阶段完成证据：

```text
ST7789 Platform Driver implemented
BSP construct implemented
Minimal ASCII Graphics implemented
focused Host tests PASS / platform_bsp_gpio + platform_st7789 + platform_graphics
full Host regression PASS / 38 groups
Keil rebuild PASS / 0 errors / 13 pre-existing warnings
new or modified relevant production files / 0 warnings
Coding Standard Review / PASS
standalone Target Verification / DEFERRED, MERGED INTO RTOS DISPLAY INTEGRATION
```

实现保持冻结 API：ST7789 仅公开 init/deinit、背光开关、draw_pixel、fill、
fill_rect 与 write_rgb565；Graphics 仅公开 draw_char 与 draw_string。面板逻辑尺寸、
offset、MADCTL 和 SPI 最大时钟集中在 `00_Config/project_config.h`，固定 256-byte
scratch buffer 仍为 Driver 实现资源。SPI Bus 为 non-owning，deinit 不停止共享 Bus。

---

# 11. Display Startup Direction — Later Phase

`platform_st7789_init()` 固定为 Task Context，因为 `platform_time_delay_ms()` 当前基于 `osDelay()`。

未来目标允许类似手机/手表的启动显示：

```text
scheduler start
 ↓
bootstrap/startup context
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

以下尚未设计：

```text
defaultTask 是否作为一次性 Bootstrap Thread
startup gate
启动画面 exact content/layout
Display Task
Display IPC
主界面
```

全部延期到 RTOS Display Integration。

---

# 12. 当前未冻结内容

不要提前实现或假定：

```text
Display Service
Display abstraction / generic backend
Display Task
Display Queue / snapshot / latest-value strategy
boot screen exact layout/content
main screen exact layout/content
partial refresh policy
UART periodic sensor TX removal
STATUS / HELP / ACK final routing
ONCE completion migration
SPI DMA
runtime SPI mode / clock switching
backlight PWM
Touch / CTP
```

---

# 13. 下一正式入口

当前没有 Active Implementation Plan。

下一步：

```text
RTOS Display Integration Design
```

下一阶段只做设计，冻结启动上下文、Display Task/IPC 和数据流后，再生成独立实施计划。

不要重新执行：

```text
Minimal ST7789 Bring-up
SPI Platform Phase 1
已冻结的 ST7789 / Graphics 基础设计讨论
```

---

# 14. 推荐恢复资料

优先读取：

```text
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/requirements.md
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
00_Doc/02_架构设计/ST7789_Graphics_Phase1设计.md
00_Doc/04_Agent/implementation_plan.md   # completed SPI record only

03_Platform/platform_mcu/spi/
04_Impl/impl_mcu/impl_platform_spi.*
05_Vendors/lcd/
Core/Src/spi.c
Core/Inc/spi.h
```

需要后续 RTOS Display Integration 上下文时读取：

```text
00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md
01_APP/app_system.c
01_APP/app_control.*
01_APP/app_acquisition.*
01_APP/app_communication.*
Core/Src/freertos.c
03_Platform/platform_os/platform_time.h
04_Impl/impl_os/freertos/impl_freertos_time.c
```
