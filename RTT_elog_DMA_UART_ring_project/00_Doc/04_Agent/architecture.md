# Embedded Firmware Architecture Contract

> 文档类型：Architecture Contract  
> 状态：CORE BASELINE + DISPLAY SPI EXTENSION  
> 版本：V3.1  
> 更新时间：2026-09-05  
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

APP / Service 不直接依赖：

```text
HAL
CubeMX Handle
Impl private API
FreeRTOS concrete handle
```

CubeMX generated files 只承担：

```text
hardware initialization
scheduler bootstrap
IRQ / HAL Callback
thin glue
```

禁止把主要业务逻辑塞回 generated files。

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

CubeMX `defaultTask` 不是第五个产品 Task；在 USER CODE 中退出。

当前 SPI / LCD 基础设施不增加新 Task。

---

# 4. APP Control Contract

唯一业务状态：

```text
STOPPED
RUNNING
```

允许 operation context：

```text
onceActive
onceSource
```

统一控制事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

来源：

```text
BUTTON
UART
```

Button 与 UART 不维护各自 running flag。

---

# 5. APP IPC Contract

当前稳定 APP Queue：

```text
Control Queue                 depth 8
Acquisition Command Queue     depth 4
Communication Outbound Queue  depth 8
Indicator Queue               depth 4
```

要求：

```text
bounded
copy-by-value
no temporary stack pointer
no business runtime malloc/free
queue full observable
```

当前不使用：

```text
APP state mutex
I2C mutex
Queue Set
Event Group as business event bus
```

Display IPC 尚未设计，不得提前混入当前稳定 APP IPC 合同。

---

# 6. Acquisition Contract

Unified Acquisition Service：

```text
DHT20 read
 -> MPU6050 read
 -> one complete atomic acquisition result
```

成功条件：

```text
DHT20 OK && MPU6050 OK
```

失败不提交 partial business data。

原则：

```text
Task decides WHEN
Service decides HOW
Platform decides HOW DEVICE IS ACCESSED
```

Acquisition Task 使用 absolute deadline：

```text
START -> immediate first sample
then every 2000 ms
no catch-up burst
```

---

# 7. UART Contract

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

原则：

```text
single producer / single consumer RX RingBuffer
no second RX path
Communication Task = sole product TX requester
USART1 = product control/data
RTT = diagnostics
```

Display 接入后 UART 产品数据职责可能收缩，但尚未冻结。

---

# 8. Current ONCE Contract

当前 Phase 9 基线成功条件：

```text
DHT20 success
AND MPU6050 success
AND complete UART report TX success
```

链：

```text
Control
 -> Acquisition
 -> Communication ONCE_REPORT
 -> UART TX success
 -> ONCE_TX_RESULT OK
 -> Control
 -> Indicator ONCE_SUCCESS
```

Display 接入后 ONCE completion semantic 必须重新设计。

---

# 9. Platform Device Model

Platform common 继续使用统一对象体系：

```text
platform_object_t
platform_device_t
platform_lifecycle_ops_t
platform_error_t
platform_bool_t
platform_size_t
```

新的 SPI Bus 进入该体系，设备类别为 generic SPI，而不是 DISPLAY。

---

# 10. SPI Architecture Extension

SPI Platform + STM32 Impl Phase 1 已完成。

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

Platform public headers 不暴露：

```text
SPI_HandleTypeDef
hspi1
HAL_SPI_*
STM32 RCC / SPI implementation details
```

HAL 绑定只存在于 Impl。

---

# 11. SPI Bus / SPI Device Contract

`platform_spi_bus_t`：

```text
represents MCU SPI controller / bus
formal Platform device lifecycle
ops
implContext
activeDevice transaction state
future synchronization point
```

`platform_spi_device_t`：

```text
lightweight SPI slave descriptor
name
bus reference
optional CS GPIO
CS active level
SPI config
initialized flag
```

SPI Device 不是完整 lifecycle device。

所有引用为 non-owning static references。

---

# 12. SPI Configuration Contract

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
```

`applyConfig()` 在 transaction begin 时确保：

```text
current mode == requested mode
current bit order == requested bit order
current data size == requested 8 bit
actual SCK <= maxClockHz
```

当前不进行 runtime mode / baudrate / data-size switching。

---

# 13. SPI Transaction Contract

公共 API：

```text
begin
write
end
```

规则：

```text
begin success -> caller owns transaction
write requires activeDevice == caller device
write never auto-end
successful begin must eventually call end
second device begin while active -> BUSY
```

不采用每次 write 自动切换 CS 的模型。

原因：一个设备事务可能包含多个连续阶段。

---

# 14. CS and LCD-Specific Signals

Generic SPI Device 可以拥有：

```text
optional CS
configurable CS active level
```

LCD 专属信号：

```text
DC
RST
BL
```

这些属于后续 ST7789 / display device layer，不进入 generic SPI。

---

# 15. SPI Hardware Ownership

当前：

```text
MX_SPI1_Init() = hardware fixed-configuration owner
```

STM32 SPI Impl lifecycle：

```text
validates HAL handle / hardware state
manages Platform lifecycle state
does not create second HAL_SPI_Init configuration source
```

当前实际 CubeMX 方向：

```text
SPI_DIRECTION_2LINES
```

虽然上层 Phase 1 只公开 TX 能力。

---

# 16. Display Extension Boundary

已稳定：

```text
LCD hardware pins
SPI1 Mode 3 / 12.5 MHz target-verified bring-up
Platform SPI Bus / Device
STM32 SPI Impl
```

尚未冻结：

```text
formal ST7789 Driver API
font / drawing organization
Display abstraction
Display Task
Display Queue / snapshot
UART periodic sensor TX migration
ONCE semantic migration
SPI DMA
backlight PWM
Touch / CTP
```

不要把尚未冻结项写成当前架构合同。

---

# 17. Memory / Display Constraint

STM32F411CEU6：

```text
Flash 512 KiB
SRAM  128 KiB
```

240x280 RGB565 full framebuffer：

```text
134400 B
```

因此当前显示架构禁止默认采用全屏 framebuffer。

优先：

```text
direct region update
small line/block buffer
partial refresh
```

---

# 18. Current Phase Contract

```text
Phase 1~9 Core Application               COMPLETE / TARGET VERIFIED
Display Hardware + CubeMX                COMPLETE
Minimal ST7789 Bring-up                  TARGET VERIFIED
SPI Platform + STM32 Impl Phase 1        COMPLETE / HOST + KEIL VERIFIED
Current Active Implementation Plan       NONE
Next                                     FORMAL ST7789 DRIVER DESIGN
```

低功耗、Touch、SPI DMA、W25Q64、Bluetooth 等继续作为后续独立增量阶段。
