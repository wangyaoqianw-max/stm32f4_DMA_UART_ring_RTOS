# 工程长期记忆与交接说明

更新时间：2026-09-06

> 本文件是 AI Agent / Codex 与人工开发者恢复工程上下文时的长期入口。  
> Phase 1~9 Core Application 已完成并通过 Host / Keil / Target 综合验证。  
> Display Extension 的硬件资源、CubeMX SPI1 + LCD GPIO、最小 Bring-up、SPI Platform + STM32 Impl、ST7789 + Minimal Graphics 均已完成。  
> RTOS Display Integration 已完成代码、Host、Keil 与人工目标板功能验收，当前阶段正式关闭。

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
ST7789 + Minimal Graphics Phase 1         COMPLETE / HOST + KEIL VERIFIED
RTOS Display Integration Design           FROZEN
Display Task / IPC                        COMPLETE / HOST + KEIL + TARGET FUNCTION VERIFIED
UART Product Output Migration             COMPLETE / HOST + KEIL + TARGET FUNCTION VERIFIED
ONCE Semantic Migration                   COMPLETE / HOST + KEIL + TARGET FUNCTION VERIFIED
Touch / CTP                               DEFERRED

RTOS Display Integration                  COMPLETE
Host Full Regression                      PASS 40/40
Keil Full Rebuild                         PASS / 0 ERRORS
Target Functional Verification            PASS
Current Active Implementation Plan        NONE
```

未作为当前功能阶段关闭阻塞项执行：

```text
Dedicated LCD fault-injection target test
Task stack high-water mark observation
Queue peak occupancy observation
```

以上转为后续可选可靠性/资源优化项。

正式设计文档：

```text
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
00_Doc/02_架构设计/ST7789_Graphics_Phase1设计.md
00_Doc/02_架构设计/RTOS_Display_Integration_Design.md
```

下一正式动作：

```text
No active plan.
Choose the next project stage before creating a new implementation plan.
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
APP      : 业务状态、任务调度、业务编排、页面编排
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

当前产品任务基线为 5 Task：

```text
Communication Task   2048 B   ABOVE_NORMAL
Control Task         1024 B   ABOVE_NORMAL
Acquisition Task     1536 B   NORMAL
Display Task         1536 B   NORMAL
Indicator Task        768 B   BELOW_NORMAL
```

CubeMX `defaultTask` 仍不是产品 Task，scheduler 启动后立即 `osThreadExit()`。

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

---

# 3. 最新 ONCE 成功语义

旧 Phase 9：

```text
DHT20 success
AND MPU6050 success
AND complete UART report TX success
```

当前冻结语义：

```text
ONCE success
= DHT20 success
AND MPU6050 success
```

下游输出不参与 ONCE 成功定义：

```text
UART TX
Display Queue send
LCD render
future storage / Bluetooth / CAN / Modbus
```

Indicator `ONCE_SUCCESS` 固定表示：

> 完整双传感器单次采集成功。

ONCE completion：

```text
APP_CONTROL_MESSAGE_ONCE_COMPLETE(result)
```

成功：

```text
onceActive = false
Indicator ONCE_SUCCESS
UART source -> OK ONCE
```

失败：

```text
onceActive = false
no success blink
UART source -> ERR ACQUISITION_FAILED
```

Control completion 属于控制面消息，Acquisition -> Control 使用可靠阻塞 Queue send；Display publish 为 best-effort NO_WAIT。

---

# 4. UART DMA + RingBuffer 稳定职责

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

Communication Task 继续是 sole USART1 runtime communication owner。

Display Integration 后 UART 功能完整保留：

```text
START / STOP / ONCE / STATUS / HELP
strict CRLF parser
control request
control response
local parser error response
DMA RX / RingBuffer
UART TX DMA
```

仅移除：

```text
periodic sensor measurement report to PC
ONCE sensor measurement report to PC
```

产品角色：

```text
UART = command / response / debug communication channel
LCD  = measurement presentation / product data display channel
```

---

# 5. Display Hardware Contract

```text
Module      : P169H002-CTP
Controller  : ST7789T3
Resolution  : 240 x 280
Interface   : 4-wire SPI display path
Pixel       : RGB565
Touch       : DEFERRED
```

Pins：

```text
PA1  -> LCD_BL
PA4  -> LCD_CS
PA5  -> SPI1_SCK
PA6  -> LCD_DC
PA7  -> SPI1_MOSI
PB10 -> LCD_RST
```

Default GPIO：

```text
CS HIGH
DC HIGH
RST HIGH
BL LOW
```

Validated：

```text
SPI1 Mode 3 @ 12.5 MHz
240 x 280
X_OFFSET = 0
Y_OFFSET = 20
RGB565 high-byte first
BL HIGH = ON
BLACK / WHITE / RED / GREEN / BLUE PASS
```

---

# 6. SPI Platform + STM32 Impl Stable Contract

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

Transaction：

```text
platform_spi_transaction_begin(device)
platform_spi_write(device, data, length)
platform_spi_transaction_end(device)
```

当前能力：

```text
blocking synchronous TX only
8-bit only
optional software CS
fixed CubeMX config validation
no runtime dynamic reconfiguration
no DMA
```

Display Integration 已补齐：

```text
Platform BSP display-SPI bus constructor
Platform SPI Bus lifecycle facade
```

禁止 APP 直接调用 `impl_platform_spi1_construct()` 或直接操作 lifecycle function pointer。

---

# 7. ST7789 + Graphics Stable Contract

正式链：

```text
APP Display Task
        ↓
Minimal Graphics / Text
        ↓
Platform ST7789 Driver
        ↓
Platform SPI + GPIO + Time
        ↓
STM32 / FreeRTOS Impl
```

不增加 Display Service，不增加 generic display backend/surface。

ST7789 owns：

```text
SPI Device descriptor
CS / DC / RST / BL GPIO descriptors
```

SPI Bus 是 shared non-owning dependency。

`platform_st7789_init()`：

```text
Task Context only
configure GPIO
init SPI Device
hardware reset
register init sequence
initialized TRUE
backlight remains OFF
```

公开能力：

```text
init / deinit
backlight_on / backlight_off
draw_pixel
fill
fill_rect
write_rgb565
```

Graphics：

```text
ASCII 8x16 only
draw_char
draw_string
opaque fg/bg
```

禁止 full framebuffer：

```text
240 * 280 * 2 = 134400 B > STM32F411 SRAM
```

---

# 8. RTOS Display Integration Final Model

Display Task 是永久第五个产品 Task，并且是：

```text
sole ST7789 / Graphics runtime owner
```

生命周期：

```text
Task Entry
 -> platform_st7789_init()
 -> Boot Page
 -> backlight ON
 -> 1000 ms dwell
 -> Main UI static layout
 -> drain pending Display Queue
 -> render latest cache
 -> wait Display Queue forever
```

Boot Page：

```text
SENSOR MONITOR
STM32F4 + RTOS
STARTING...
```

不作为 startup gate，不显示虚假的 `SYSTEM OK / SENSOR OK`。

Main UI：

```text
SENSOR MONITOR
STATE : RUNNING / STOPPED
TEMP / HUM
ACCEL X/Y/Z (g)
GYRO X/Y/Z (dps)
```

首次有效采集前 measurement 显示 `--`；STOP 后保留最后一次有效 measurement。

局部刷新，不每 2 s 全屏重画。

---

# 9. Display IPC Final Contract

消息：

```text
APP_DISPLAY_MESSAGE_SYSTEM_STATE
APP_DISPLAY_MESSAGE_MEASUREMENT
```

Producer：

```text
Control      -> SYSTEM_STATE
Acquisition  -> MEASUREMENT
```

Consumer：

```text
Display Task only
```

Queue：

```text
Depth = 4
copy-by-value
producer = NO_WAIT
```

Display Queue 是 bounded FIFO；Display Task 使用 consumer-side coalescing：

```text
WAIT_FOREVER first message
 -> update cache
 -> NO_WAIT drain current backlog
 -> keep latest state / measurement
 -> render once
```

Display presentation cache 不是真实业务状态副本；唯一业务真值仍在 Control FSM。

---

# 10. Display Failure Isolation Contract

Display 是 non-critical output subsystem。

LCD failure 不得：

```text
stop Acquisition
stop Control
stop Communication
stop Indicator
change APP FSM
change acquisition success
```

Startup init failure：

```text
log
BL OFF
available = false
no automatic retry
Display Task remains alive
continue draining queue / updating cache
```

Runtime render failure：

```text
log / statistics
keep dirty flag
available remains true
next Display event retries latest cache
```

不增加周期性 retry loop。

该合同已由实现与 Host tests 覆盖；独立目标板 fault injection 可后续按需执行。

---

# 11. Final APP Data Flow

```text
UART RX
 -> Communication
 -> Control Queue
 -> Control
```

```text
Button
 -> Control
```

```text
Control
 -> Acquisition Queue
 -> Acquisition
 -> Unified Acquisition Service
```

Successful measurement：

```text
Acquisition
 -> Display Queue / MEASUREMENT
 -> Display
 -> ST7789
```

State：

```text
Control
 -> Display Queue / SYSTEM_STATE
 -> Display
```

ONCE completion：

```text
Acquisition
 -> Control Queue / ONCE_COMPLETE
 -> Control
 -> Indicator
 -> optional UART response
```

UART response：

```text
Control
 -> Communication Response Queue
 -> Communication
 -> UART
```

关键解耦：

```text
Acquisition -X-> Communication measurement data
Communication -X-> ONCE completion
Display -X-> Control business result
```

---

# 12. Composition Root

`app_system.c` 已集成：

```text
g_displaySpiBus
g_display
g_displayQueue
g_appDisplay
g_displayThread
```

Pre-scheduler：

```text
construct SPI Bus / ST7789
SPI Bus lifecycle init/start
create Display Queue
app_display_init
create Display Thread
```

Post-scheduler / Display Task：

```text
platform_st7789_init
Boot/Main UI
runtime render
```

Rollback 保持 strict reverse order。

---

# 13. Static Resource Baseline

```text
Communication Task   2048 B   ABOVE_NORMAL
Control Task         1024 B   ABOVE_NORMAL
Acquisition Task     1536 B   NORMAL
Display Task         1536 B   NORMAL
Indicator Task        768 B   BELOW_NORMAL
```

```text
Control Queue                8
Acquisition Command Queue    4
Communication Response       8
Display Queue                4
Indicator Queue              4
```

新增：

```text
PROJECT_DISPLAY_TASK_STACK_SIZE_BYTES = 1536
PROJECT_DISPLAY_TASK_PRIORITY         = NORMAL
PROJECT_DISPLAY_QUEUE_DEPTH           = 4
PROJECT_DISPLAY_BOOT_DURATION_MS      = 1000
```

这些资源值已经支持当前目标板功能正常运行。后续资源优化仅在有 high-water mark / Queue peak 证据时进行。

---

# 14. Implementation Plan 状态

正式记录：

```text
00_Doc/04_Agent/implementation_plan.md
```

最终结果：

```text
Implementation           COMPLETE
Host verification        PASS 40/40
Keil rebuild             PASS / 0 errors
Target functional test   PASS
Documentation closeout   COMPLETE
```

当前：

```text
Active Implementation Plan = NONE
```

不要重新设计或重做：

```text
Minimal ST7789 bring-up
SPI Platform Phase 1
ST7789 / Graphics Phase 1
Display Task / IPC architecture
ONCE semantic definition
Main UI information architecture
UART measurement-output migration
```

---

# 15. 推荐恢复资料

优先读取：

```text
00_Doc/04_Agent/handoff.md
00_Doc/02_架构设计/RTOS_Display_Integration_Design.md
00_Doc/04_Agent/implementation_plan.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
00_Doc/02_架构设计/ST7789_Graphics_Phase1设计.md
```

---

# 16. Target Verification Result

人工板测功能确认：PASS。

本次确认的功能基线包括：

```text
Boot Page -> Main UI 正常
initial STOPPED / placeholder behavior 正常
START / STOP 正常
RUNNING 下周期 LCD measurement refresh 正常
Button ONCE 正常
UART ONCE -> OK ONCE 正常
UART START / STOP / STATUS / HELP 正常
UART 不再输出周期/ONCE ENV/IMU measurement report
ONCE success LED behavior 正常
```

当前工程可直接作为完整功能基线继续使用。

未单独记录：

```text
Dedicated LCD fault-injection target test
five product Task high-water marks
Display / Communication Queue peak occupancy
```

这些项目属于后续可选可靠性/资源优化工作，不阻塞当前阶段完成。
