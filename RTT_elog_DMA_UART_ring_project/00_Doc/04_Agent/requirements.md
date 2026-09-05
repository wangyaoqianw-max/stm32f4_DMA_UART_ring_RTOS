# Embedded Firmware Requirements Baseline

> 文档类型：Agent Requirements Baseline  
> 状态：CORE BASELINE + DISPLAY SPI EXTENSION  
> 版本：V3.1  
> 更新时间：2026-09-05  
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
```

当前没有 Active Implementation Plan。

下一阶段：

```text
formal ST7789 Driver design discussion
```

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

固定 4 个产品任务：

| Task | Initial Stack | Priority | Core Responsibility |
| --- | ---: | --- | --- |
| Communication | 2048 B | ABOVE_NORMAL | UART RX parser + sole product TX |
| Control | 1024 B | ABOVE_NORMAL | Button polling + sole APP FSM |
| Acquisition | 1536 B | NORMAL | sensor scheduling/execution |
| Indicator | 768 B | BELOW_NORMAL | LED semantic execution |

CubeMX `defaultTask` 不作为第五个产品 Task。

当前 SPI / LCD Platform 基础设施不增加 Task。

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

Communication Task 是唯一产品 TX requester。

Display 接入后是否停止周期 sensor UART TX 尚未冻结。

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

Display Queue / snapshot 尚未设计。

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

不要重新把临时 Bring-up code 当作正式驱动。

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

DC / RST / BL 是 LCD-specific signals，不进入 generic SPI。

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

但 Platform Phase 1 只公开 TX 能力。

---

# 18. Display Resource Requirements

全屏 RGB565 framebuffer：

```text
240 * 280 * 2 = 134400 B
```

超过当前 128 KiB SRAM 可接受范围。

因此正式显示实现不得默认采用全屏 framebuffer。

优先：

```text
direct region update
small line/block buffer
partial refresh
```

SPI DMA 不是当前硬性需求，只有正式刷新暴露性能问题后再评估。

---

# 19. Current Verification Status

SPI Phase 1：

```text
Focused Host tests : PASS / 2 groups
Host regression    : PASS / 36 groups
Keil rebuild       : PASS / 0 errors
Target test        : NOT REQUIRED BY PLAN
```

正式 ST7789 Driver 完成后需要重新做目标板验证正式调用链。

---

# 20. Active Scope

已经完成：

```text
Phase 1~9 Core Application
Final Integrated Board Test
Display hardware/CubeMX
Minimal ST7789 Bring-up
SPI Platform + STM32 Impl Phase 1
```

当前下一步：

```text
formal ST7789 Driver architecture/API discussion
```

当前不直接实施：

```text
Display Task / Queue
UART output migration
ONCE semantic migration
SPI DMA
runtime SPI reconfiguration
Touch / CTP
backlight PWM
W25Q64
Bluetooth
low-power / Tickless
```

必须先讨论、冻结设计，再创建新的 Implementation Plan。
