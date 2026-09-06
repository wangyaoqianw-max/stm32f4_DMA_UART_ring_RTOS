# Embedded Firmware Architecture Contract

> 文档类型：Architecture Contract  
> 状态：CORE BASELINE + DISPLAY DRIVER DESIGN FROZEN  
> 版本：V3.2  
> 更新时间：2026-09-06  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 总体分层

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

固定依赖：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

APP / Service 不直接依赖 HAL、CubeMX Handle、Impl private API、FreeRTOS concrete handle。

CubeMX generated files 只承担 hardware initialization、scheduler bootstrap、IRQ/HAL callback 与 thin glue。

---

# 2. Core Application Runtime Architecture

```text
PA0 Button
 -> Platform Button
 -> Button Service
 -> Control Task
                    \
                     -> APP Control FSM
                    /
USART1 RX DMA
 -> UART Service
 -> RingBuffer
 -> Communication Task
 -> Control Queue
```

APP FSM 输出：

```text
                  +-> Acquisition Command Queue
                  |      -> Acquisition Task
                  |      -> Acquisition Service
APP Control FSM --+      -> DHT20 -> MPU6050 -> Shared Soft I2C
                  |
                  +-> Indicator Queue -> Indicator Task -> Indicator Service -> LED
                  |
                  +-> Communication Outbound Queue -> Communication Task -> UART TX DMA
```

核心 owner：

```text
Control Task / APP FSM = sole STOPPED / RUNNING business truth owner
Acquisition Task       = sole DHT20 / MPU6050 / shared Soft-I2C runtime accessor
Communication Task     = sole USART1 product TX requester
Indicator Task         = LED semantic executor
```

---

# 3. Stable Four-Task Contract

| Task | Responsibility | Initial Stack | Priority |
| --- | --- | ---: | --- |
| Communication | UART RX parser、outbound format/TX | 2048 B | ABOVE_NORMAL |
| Control | Button polling、唯一 APP FSM | 1024 B | ABOVE_NORMAL |
| Acquisition | periodic/ONCE scheduling、sensor execution | 1536 B | NORMAL |
| Indicator | LED semantic execution | 768 B | BELOW_NORMAL |

CubeMX `defaultTask` 不是第五个产品 Task；当前 SPI / ST7789 / Graphics 基础能力本身不增加 Product Task。

后续 RTOS Display Integration 再讨论 bootstrap/defaultTask、startup gate、Display Task 与 Display IPC。

---

# 4. APP / IPC / Acquisition / UART Baseline

APP 唯一业务状态：

```text
STOPPED
RUNNING
```

当前 Queue：

```text
Control Queue
Acquisition Command Queue
Communication Outbound Queue
Indicator Queue
```

要求：bounded、copy-by-value、no temporary stack pointer、no business runtime malloc/free、queue full observable。

Unified Acquisition：

```text
DHT20 read
 -> MPU6050 read
 -> one complete atomic acquisition result
```

UART RX：

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

UART TX：

```text
Communication Task
 -> UART Service
 -> Platform UART async
 -> STM32 UART Impl
 -> HAL_UART_Transmit_DMA()
```

当前 ONCE success：

```text
DHT20 success
AND MPU6050 success
AND complete UART report TX success
```

Display 接入后必须重新设计 ONCE completion semantic。

---

# 5. Platform Device Model

Platform common 继续使用：

```text
platform_object_t
platform_device_t
platform_lifecycle_ops_t
platform_error_t
platform_bool_t
platform_size_t
```

SPI Bus 是 generic SPI lifecycle device。

ST7789 是 Platform/BSP concrete device driver，不另造平行对象体系。

---

# 6. SPI Architecture Extension

正式链：

```text
upper device driver
    ↓
platform_spi_device_t
    ↓
platform_spi_transaction_begin()
platform_spi_write()
platform_spi_transaction_end()
    ↓
platform_spi_bus_t / ops
    ↓
STM32 SPI Impl
    ↓
HAL SPI
```

Platform public headers 不暴露 `SPI_HandleTypeDef`、`hspi1`、`HAL_SPI_*`。

SPI Device config：

```text
mode
bitOrder
dataBits
maxClockHz
```

Phase 1：

```text
blocking synchronous TX only
8-bit only
fixed CubeMX configuration validation
optional software CS
```

Transaction：

```text
begin success -> caller owns transaction
write requires activeDevice == caller device
write never auto-end
successful begin must eventually call end
second begin while active -> BUSY
```

当前实际 CubeMX：

```text
SPI_DIRECTION_2LINES
```

但上层只公开 TX 能力。

---

# 7. Display Hardware Contract

```text
P169H002-CTP
ST7789T3
240 x 280
RGB565
SPI1 Mode 3
12.5 MHz
8 bit
MSB First
X_OFFSET = 0
Y_OFFSET = 20
RGB565 high-byte first
BL HIGH = ON
```

引脚：

```text
PA1  BL
PA4  CS
PA5  SCK
PA6  DC
PA7  MOSI
PB10 RST
```

Minimal Bring-up 已完成目标板验证；临时代码已回退。

---

# 8. Display Layer Contract

当前冻结正式链：

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
        ↓
HAL / Hardware
```

重要边界：

```text
ST7789 = concrete device capability
Graphics/Text = basic drawing capability
Display Service = NOT REQUIRED YET
```

当前不抽象 generic display backend/surface。

原因：这是第一次正式接入 SPI TFT；先完成真实设备驱动，再在接触第二类屏幕/控制器后从真实差异中提炼可复用层。

---

# 9. ST7789 Resource Ownership

采用：

```text
device-local resource ownership
+
shared SPI Bus non-owning dependency
```

`platform_st7789_t` owns：

```text
platform_spi_device_t descriptor
CS GPIO descriptor
DC GPIO descriptor
RST GPIO descriptor
BL GPIO descriptor
```

SPI Bus：

```text
shared non-owning dependency
```

信号边界：

```text
CS  -> Platform SPI transaction controls it
DC  -> ST7789 controls it
RST -> ST7789 controls it
BL  -> ST7789 controls it
```

ST7789 deinit 不 stop/deinit SPI Bus。

---

# 10. ST7789 Lifecycle Contract

BSP constructor：

```text
board/panel static binding only
no GPIO configure
no delay
no command
```

`platform_st7789_init()`：

```text
Task Context only
validate
configure CS/DC/RST/BL
BL remains OFF
init SPI Device
hardware reset
controller init sequence
initialized = TRUE
```

成功：controller READY for drawing，backlight still OFF。

`init()` 不自动 full-screen clear。

失败：

```text
first/root failure preserved
best-effort reverse rollback
clean constructed + uninitialized state
```

当前 `platform_time_delay_ms()` 基于 `osDelay()`，因此 ST7789 init 不属于 scheduler-start 前的裸机初始化路径。

---

# 11. ST7789 Protocol Contract

初始化采用：

```text
explicit hardware reset
+
table-driven command sequence
```

关键：

```text
0x11 Sleep Out
 -> end transaction
 -> delay 120 ms

0x29 Display On
 -> init sequence end
```

Vendor 代码末尾的：

```text
0x2C RAMWR
```

不进入正式 init；RAMWR 属于 pixel region operation。

Command helper 分为：

```text
write_command_in_transaction()
write_command()
```

Delay 不允许发生在持有 SPI transaction 时。

---

# 12. Region / Pixel Contract

一个完整区域刷新：

```text
one SPI transaction

CASET
RASET
RAMWR
pixel chunks
```

公共坐标：

```text
x / y / width / height
```

逻辑区域：

```text
X 0..239
Y 0..279
```

Driver 内部吸收：

```text
X offset 0
Y offset 20
```

Phase 1：strict bounds，no automatic clipping。

公共 RGB565：

```text
uint16_t logical color/pixel
```

SPI wire：

```text
high byte first
```

当前 STM32 SPI Impl 单次 transfer max：

```text
0xFFFF bytes
```

因此 large pixel writes 必须 chunk。

---

# 13. Display Memory Contract

STM32F411CEU6：

```text
SRAM = 128 KiB
```

全屏 RGB565：

```text
240 * 280 * 2 = 134400 B
```

因此禁止默认 full framebuffer。

正式路径：

```text
direct region update
small fixed scratch buffer
chunked write
no runtime malloc/free
```

SPI DMA 暂不加入；待正式刷新暴露真实 CPU/阻塞问题后再评估。

---

# 14. ST7789 Public Drawing Scope

Phase 1：

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

不加入：

```text
draw_line
draw_circle
draw_bitmap
show_char/show_string inside ST7789 driver
```

---

# 15. Minimal Graphics/Text Contract

文件层：

```text
03_Platform/platform_graphics/
```

Phase 1：

```text
ASCII 8x16 only
printable ASCII 0x20..0x7E
opaque text
draw_char
draw_string
```

Vendor `lcdfont.h` 仅作为字模来源，正式资源提取为独立 `.h/.c`。

一个 8x16 glyph：

```text
bitmap
 -> expand to 128 RGB565 pixels
 -> one write_rgb565 region write
```

禁止正式字符路径逐 pixel 调 `draw_pixel()`。

当前不支持：

```text
Chinese / UTF-8
transparent text
alignment
wrap
ellipsis
show_uint/show_float
printf wrapper
GUI/widget
```

业务值格式化属于 APP / future Display orchestration。

---

# 16. Error Contract

ST7789 / Graphics 统一复用 `platform_error_t`，不创建 ST7789-specific error hierarchy。

常见：

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

底层 SPI / GPIO / Time error 尽量原样传播。

事务规则：

```text
transaction begin success
 -> later operation success/failure both require best-effort transaction_end
```

错误优先级：

```text
operation error > cleanup error
```

---

# 17. Verification Contract

ST7789 path 当前 write-oriented，因此 Host success 不能证明 physical panel 正确显示。

长期验证：

```text
Host Test + Target Verification
```

但 ST7789 + Minimal Graphics Phase 1：

```text
Host focused tests required
full Host regression required
Keil rebuild required
standalone Target Verification not required
```

正式 ST7789 Target Verification：

```text
DEFERRED / MERGED INTO RTOS DISPLAY INTEGRATION
```

后续系统板测覆盖 boot screen、ASCII、offset/four corners、sensor refresh、long-running update、核心任务无回归。

---

# 18. Display Startup Direction

未来允许：

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

但以下尚未冻结：

```text
defaultTask bootstrap role
startup gate
boot screen exact content/layout
Display Task
Display IPC
main screen
```

这些属于 RTOS Display Integration。

---

# 19. Current Phase Contract

```text
Phase 1~9 Core Application               COMPLETE / TARGET VERIFIED
Display Hardware + CubeMX                COMPLETE
Minimal ST7789 Bring-up                  TARGET VERIFIED
SPI Platform + STM32 Impl Phase 1        COMPLETE / HOST + KEIL VERIFIED
ST7789 + Minimal Graphics Design         FROZEN
ST7789 + Minimal Graphics Implementation NOT STARTED
Current Active Implementation Plan       NONE
Next                                     CREATE IMPLEMENTATION PLAN
```

正式设计：

```text
00_Doc/02_架构设计/ST7789_Graphics_Phase1设计.md
```

Implementation 完成后再进入 RTOS Display Integration Design。

低功耗、Touch、SPI DMA、W25Q64、Bluetooth 等继续作为后续独立增量阶段。
