# Embedded Firmware Requirements Baseline

> 文档类型：Agent Requirements Baseline  
> 状态：CORE BASELINE + ST7789 / GRAPHICS PHASE 1 IMPLEMENTED  
> 版本：V3.2  
> 更新时间：2026-09-06  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 文档定位

本文件是 AI Agent / Codex 进行设计、编码和 Review 时使用的长期需求摘要。

Phase 1~9 Core Application 已完成并通过 Host / Keil / Target 综合验证。

当前 Display Extension 已完成：

```text
hardware resource confirmation
CubeMX SPI1 + LCD GPIO
minimal ST7789 target bring-up
SPI Platform + STM32 Impl Phase 1
ST7789 + Minimal Graphics Phase 1
```

当前没有 Active Implementation Plan。

下一阶段只进入：

```text
RTOS Display Integration Design
```

在 RTOS Display Integration 设计冻结前，不得提前实现 Display Task / IPC / startup gate / UART output migration / ONCE semantic migration。

---

# 2. Stable Layering Requirements

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED

APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

APP / Service 禁止直接依赖：

```text
HAL
CubeMX Handle
Impl private API
FreeRTOS concrete handle
```

CubeMX generated files 只承担初始化、Scheduler、IRQ / HAL Callback 和薄胶水。

---

# 3. Hardware / Software Environment

```text
MCU        : STM32F411CEU6 / Cortex-M4F
Flash      : 512 KiB
RAM        : 128 KiB
UART       : USART1 / 115200 8N1
Sensors    : DHT20 + MPU6050
Input      : PA0 User Key
Indicator  : PC13 Status LED
I2C        : Software I2C over PB6/PB7
RTOS       : CMSIS-RTOS2 + FreeRTOS
Log        : EasyLogger + SEGGER RTT
Display    : P169H002-CTP / ST7789T3 / 240x280
Toolchain  : Keil MDK-ARM + STM32CubeMX
```

Touch / CTP 当前不在范围内。

---

# 4. Core Application Completion Status

```text
Phase 1~9                    COMPLETE
Host regression              PASS
Keil production build        PASS
Final integrated target test PASS
```

核心产品行为已冻结为稳定基线，除缺陷修复或正式 Display 业务迁移外不随意重构。

---

# 5. Final RTOS Task Requirements

当前稳定产品任务仍为 4 个：

| Task | Initial Stack | Priority | Core Responsibility |
| --- | ---: | --- | --- |
| Communication | 2048 B | ABOVE_NORMAL | UART RX parser + sole product TX |
| Control | 1024 B | ABOVE_NORMAL | Button polling + sole APP FSM |
| Acquisition | 1536 B | NORMAL | sensor scheduling/execution |
| Indicator | 768 B | BELOW_NORMAL | LED semantic execution |

CubeMX `defaultTask` 当前不是第五个常驻产品 Task。

ST7789 + Graphics Phase 1 不新增运行任务。

后续是否建立 Display Task、是否复用 `defaultTask` 作为一次性 Bootstrap Thread、是否需要 startup gate，必须在 RTOS Display Integration Design 中重新冻结。

---

# 6. APP Control Requirements

唯一业务状态：

```text
STOPPED
RUNNING
```

唯一 owner：

```text
Control Task / APP FSM
```

统一控制事件：

```text
START
STOP
SAMPLE_ONCE
GET_STATUS
```

来源：

```text
Button
UART
```

Button / UART 不得维护独立 running flag。

---

# 7. Acquisition Requirements

Unified Acquisition Service：

```text
DHT20 read
 -> MPU6050 read
 -> complete atomic acquisition result
```

成功：

```text
DHT20 OK && MPU6050 OK
```

失败不得提交 partial business data。

周期：

```text
START -> immediate first sample
then every 2000 ms by absolute deadline
```

Acquisition Task 是 DHT20 / MPU6050 / shared Software I2C 唯一运行时访问者。

---

# 8. UART Requirements

RX：

```text
USART1 RX DMA Circular
IDLE / HT / TC
UART Platform / Service
SPSC RingBuffer
Communication Task
```

要求：

```text
single producer / single consumer
no ordinary RingBuffer mutex
no second RX path
```

TX：

```text
Communication Task
 -> UART Service
 -> Platform UART async
 -> STM32 UART Impl
 -> USART1 TX DMA
```

Communication Task 是当前唯一产品 TX requester。

Display 接入后是否停止周期 sensor UART TX 尚未冻结，必须在 RTOS Display Integration 阶段讨论。

---

# 9. Current UART Protocol Requirements

严格命令：

```text
START\r\n
STOP\r\n
ONCE\r\n
STATUS\r\n
HELP\r\n
```

规则：

```text
strict CRLF
uppercase only
case-sensitive
no trim
no arguments
fixed-size storage
```

当前 report：

```text
ENV,T=...,...\r\n
IMU,AX=...,AY=...,AZ=...,GX=...,GY=...,GZ=...\r\n
```

这些是 Phase 9 稳定基线；Display Product Output 迁移后可通过新设计修改。

---

# 10. Current ONCE Requirement

当前成功条件：

```text
DHT20 success
AND MPU6050 success
AND complete UART report TX success
```

成功后：

```text
Indicator blink 3 times
remain STOPPED
```

Display 接入后必须重新定义 ONCE completion semantic，不能简单把 UART Queue 换成 Display Queue。

---

# 11. APP IPC Requirements

当前稳定 Queue：

```text
Control Queue                 depth 8
Acquisition Command Queue     depth 4
Communication Outbound Queue  depth 8
Indicator Queue               depth 4
```

要求：

```text
Platform Queue abstraction
bounded
value-copy
no temporary stack pointer
no infinite producer blocking
queue full observable
```

Display Queue / snapshot / latest-value strategy 尚未设计。

---

# 12. ISR / Memory Requirements

ISR / HAL Callback 仅允许：

```text
capture
necessary copy
lightweight state update
ISR-safe notify
quick exit
```

禁止：

```text
business FSM in ISR
Software I2C transaction in ISR
full UART parser in ISR
sensor business in ISR
blocking LED blink in ISR
runtime malloc/free in business paths
heavy formatted logs in ISR
```

核心运行数据优先：

```text
static
caller-owned
value-copy
```

---

# 13. Logging Requirements

```text
USART1 -> product control/data
RTT    -> initialization / state / diagnostics / errors
```

禁止正常运行逐 byte、逐 DMA step、逐 I2C bit、逐 Button poll 刷日志。

---

# 14. Display Hardware Requirements

当前已验证屏幕：

```text
P169H002-CTP
ST7789T3
240 x 280
RGB565
```

引脚：

```text
PA1  LCD_BL
PA4  LCD_CS
PA5  SPI1_SCK
PA6  LCD_DC
PA7  SPI1_MOSI
PB10 LCD_RST
```

目标板已确认：

```text
Mode 3
12.5 MHz current SCK
8 bit
MSB First
software CS
X_OFFSET 0
Y_OFFSET 20
RGB565 high-byte first
BL High = ON
BL Low = OFF
```

临时 Bring-up code 只作为硬件事实来源，不作为正式驱动实现。

---

# 15. SPI Platform Requirements

SPI Platform + STM32 Impl Phase 1 已完成。

正式对象：

```text
platform_spi_bus_t
platform_spi_device_t
```

公共事务：

```text
platform_spi_transaction_begin()
platform_spi_write()
platform_spi_transaction_end()
```

SPI Device 配置：

```text
mode
bitOrder
dataBits
maxClockHz
```

Phase 1 能力：

```text
blocking synchronous TX
8-bit only
optional CS
configurable CS active level
fixed CubeMX config validation
```

Platform public API 禁止暴露：

```text
SPI_HandleTypeDef
hspi1
HAL_SPI_*
```

STM32 Impl 是 HAL SPI 唯一绑定位置。

---

# 16. SPI Transaction Requirements

规则：

```text
begin success transfers transaction ownership to caller
write requires activeDevice == device
write does not auto-end
successful begin must eventually call end
second begin while bus active -> BUSY
wrong-device write/end -> INVALID_STATE
```

CS 可以为 NULL。

ST7789 中：

```text
CS  -> generic Platform SPI transaction
DC  -> ST7789 Driver
RST -> ST7789 Driver
BL  -> ST7789 Driver
```

---

# 17. SPI Configuration Ownership

当前硬件固定配置 owner：

```text
CubeMX MX_SPI1_Init()
```

STM32 SPI Impl：

```text
validates actual Mode / FirstBit / DataSize / SCK
uses finite blocking HAL_SPI_Transmit timeout
does not runtime reconfigure SPI in Phase 1
```

当前实际 CubeMX Direction：

```text
SPI_DIRECTION_2LINES
```

但 Platform SPI 当前只公开 TX 能力。

---

# 18. ST7789 Driver Requirements

ST7789 + Minimal Graphics Phase 1 已实现并冻结以下合同。

层级：

```text
Platform concrete device driver
```

对象拥有：

```text
platform_spi_device_t
CS GPIO descriptor
DC GPIO descriptor
RST GPIO descriptor
BL GPIO descriptor
fixed TX scratch buffer
```

SPI Bus 是 shared non-owning dependency；ST7789 deinit 不得 stop/deinit SPI Bus。

BSP construct：

```text
只做 static binding/config
不配置 GPIO
不发送 SPI
不 delay
不打开背光
```

`platform_st7789_init()`：

```text
Task Context only
uses platform_time_delay_ms()
atomic full controller initialization
backlight remains OFF on success
no automatic full-screen clear
failure -> best-effort rollback
```

Init sequence：

```text
explicit hardware reset
+
table-driven command sequence
0x11 Sleep Out -> transaction end -> 120 ms delay
0x29 Display On -> final init command
0x2C RAMWR excluded from init
```

Region write：

```text
one logical region = one SPI transaction
CASET -> RASET -> RAMWR -> pixel chunks
```

坐标：

```text
logical 240 x 280
X offset = 0
Y offset = 20
strict bounds
no clipping
```

公共像素格式：

```text
uint16_t RGB565
wire = high byte first
```

公开 API 限定为：

```text
platform_st7789_init
platform_st7789_deinit
platform_st7789_backlight_on
platform_st7789_backlight_off
platform_st7789_draw_pixel
platform_st7789_fill
platform_st7789_fill_rect
platform_st7789_write_rgb565
```

当前不扩展：

```text
runtime rotation
SPI DMA
SPI read
GUI/widget
backlight PWM
Touch / CTP
```

---

# 19. Minimal Graphics Requirements

Graphics/Text 是 Platform 绘图能力，不是 Display Service。

当前直接依赖：

```text
platform_st7789_t
```

在没有第二类真实显示后端之前，不创建 generic display surface/backend abstraction。

Phase 1 字体：

```text
printable ASCII 0x20..0x7E
8 x 16
opaque foreground/background
```

Vendor `lcdfont.h` 只作为字模来源；正式代码使用独立字体 `.h/.c`，不得直接 include Vendor LCD font header。

公开 API：

```text
platform_graphics_draw_char
platform_graphics_draw_string
```

一个字符使用一次 `platform_st7789_write_rgb565()` 区域写，不使用逐像素 `draw_pixel()` 作为正式字符渲染路径。

当前不实现：

```text
Chinese / UTF-8
transparent text
alignment
auto wrap
number/float formatting
printf wrapper
GUI/widget/page
```

业务数据转字符串属于 APP / 后续 Display logic。

---

# 20. Display Resource Requirements

全屏 RGB565 framebuffer：

```text
240 * 280 * 2 = 134400 B
```

超过当前 128 KiB SRAM 可接受范围。

因此当前显示实现禁止全屏 framebuffer。

正式 ST7789 Driver 使用：

```text
fixed 256-byte scratch buffer
128 RGB565 pixels/chunk
direct region update
chunked blocking SPI write
no runtime malloc/free
```

SPI DMA 不是当前硬性需求；只有 RTOS Display Integration 暴露明显 CPU 占用、任务阻塞或刷新延迟问题后再评估。

---

# 21. Error Requirements

ST7789 / Graphics 继续统一使用现有：

```text
platform_error_t
```

不创建 ST7789-specific error enum。

要求：

```text
SPI / GPIO / Time errors propagate where possible
successful transaction_begin must be followed by best-effort transaction_end
root operation error wins over cleanup error
init failure preserves first/root error and performs rollback
```

当前 ST7789 path 是 write-oriented；Host success 不能替代物理面板验证。

---

# 22. Current Verification Status

Core：

```text
Phase 1~9 Host / Keil / Target          PASS
Final Integrated Board Test            PASS
```

SPI Platform + STM32 Impl Phase 1：

```text
Focused Host tests                     PASS / 2 groups
Host regression                        PASS / 36 groups
Keil rebuild                           PASS / 0 errors
```

ST7789 + Minimal Graphics Phase 1：

```text
Focused Host tests                     PASS
Full Host regression                   PASS / 38 groups
Keil rebuild                           PASS / 0 errors
Warnings                               13 pre-existing / no new relevant warning
Coding Standard Review                 PASS
Standalone Target Verification         DEFERRED / MERGED INTO RTOS DISPLAY INTEGRATION
```

Minimal Bring-up 已经验证 SPI/ST7789 基础物理链路；正式 ST7789 + Graphics 系统级目标板验证将在下一阶段 RTOS Display Integration 中完成。

---

# 23. Active Scope

已经完成：

```text
Phase 1~9 Core Application
Final Integrated Board Test
Display hardware/CubeMX
Minimal ST7789 Bring-up
SPI Platform + STM32 Impl Phase 1
ST7789 + Minimal Graphics Phase 1
```

当前下一步：

```text
RTOS Display Integration Design
```

下一阶段需要讨论并冻结：

```text
bootstrap/defaultTask role
startup gate
boot screen contents
Display Task ownership
Display IPC / snapshot / latest-value strategy
Acquisition -> Display data flow
main screen content/layout
UART periodic sensor output migration
STATUS / HELP / ACK routing
ONCE completion semantic migration
partial refresh policy
```

当前不直接实施：

```text
RTOS Display Integration
SPI DMA
runtime SPI reconfiguration
Touch / CTP
backlight PWM
W25Q64
Bluetooth
low-power / Tickless
```

必须先完成 RTOS Display Integration 设计讨论与冻结，再创建新的 Implementation Plan。
