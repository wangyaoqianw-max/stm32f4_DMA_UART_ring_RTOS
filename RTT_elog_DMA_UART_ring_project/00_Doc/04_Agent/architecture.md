# 工程架构长期说明

更新时间：2026-09-06

实现状态：

```text
RTOS Display Integration  COMPLETE
Host full regression      PASS 40/40
Keil full rebuild         PASS / 0 errors
Target functional test    PASS
```

说明：人工目标板功能验收已确认正常；独立 LCD 故障注入与 Task/Queue 资源高水位观测未作为本功能阶段关闭的阻塞条件，转为后续按需验证/优化项。

---

# 1. 稳定分层

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
Platform : 设备能力、OS 抽象、基础 Graphics
Impl     : STM32 / FreeRTOS concrete adaptation
Vendor   : HAL / CMSIS / FreeRTOS / third-party
```

CubeMX generated files 只承担：

```text
hardware init
scheduler bootstrap
IRQ / HAL callback
thin glue
```

---

# 2. 设计原则

```text
no runtime malloc/free
static ownership
Queue copy-by-value
never enqueue stack pointer
single runtime owner for each hardware capability
business truth has one owner
failure semantics are local to each subsystem
```

所有跨 Task 数据必须通过稳定 IPC 合同，不通过临时全局变量共享业务状态。

---

# 3. 产品 Task Ownership

最终 5 个产品 Task：

```text
Communication Task
Control Task
Acquisition Task
Display Task
Indicator Task
```

Ownership：

```text
Communication = sole USART1 runtime communication owner
Control       = sole APP STOPPED/RUNNING FSM owner
Acquisition   = sole DHT20 / MPU6050 / shared-I2C runtime owner
Display       = sole ST7789 / Graphics runtime owner
Indicator     = sole LED semantic executor
```

CubeMX `defaultTask` 不是产品 Task，启动后退出。

---

# 4. APP Control Model

唯一业务状态：

```text
STOPPED
RUNNING
```

正交 operation context：

```text
onceActive
onceSource
```

输入：

```text
Button:
SINGLE -> START
DOUBLE -> ONCE
LONG   -> STOP

UART:
START
STOP
ONCE
STATUS
HELP
```

Control 是唯一业务状态真值拥有者。

Display 中的 RUNNING/STOPPED 只是 presentation snapshot。

---

# 5. Unified Acquisition

Service：

```text
DHT20 read
 -> MPU6050 read
 -> atomic complete result
```

成功：

```text
DHT20 OK && MPU6050 OK
```

Acquisition Task 负责：

```text
2 s absolute-deadline scheduling
START / STOP command consumption
ONCE execution
stale periodic result suppression after STOP
measurement publish
ONCE completion publish
```

Acquisition Task 不直接控制：

```text
UART
LED
LCD rendering
```

---

# 6. ONCE 成功语义

冻结定义：

```text
ONCE success
= complete dual-sensor acquisition success
```

不包含：

```text
UART TX
Display Queue send
LCD render
future storage / CAN / Bluetooth / Modbus
```

因此：

```text
Acquisition Success
= 数据是否正确获得

Display Success
= 数据是否正确显示

Communication Success
= 通信是否正确完成

Storage Success
= 数据是否正确保存
```

Indicator `ONCE_SUCCESS` 只表示采集链成功。

---

# 7. UART Architecture

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

UART 产品角色：

```text
command / response / debug communication channel
```

Display Integration 后不再承担 sensor measurement product output。

Communication Task 不再知道：

```text
DHT20
MPU6050
periodic measurement payload
ONCE measurement payload
Display
```

---

# 8. Display Architecture

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

当前不增加 Display Service。

Display Task：

```text
sole ST7789 / Graphics runtime owner
```

启动：

```text
scheduler start
 -> Display Task
 -> ST7789 init
 -> Boot Page
 -> backlight ON
 -> Main UI
 -> event-driven partial refresh
```

Display 是 non-critical output subsystem；LCD failure 不得改变 Control 或 Acquisition 成功语义。

---

# 9. Display IPC

Display Queue：

```text
SYSTEM_STATE
MEASUREMENT
```

Producer：

```text
Control      -> SYSTEM_STATE
Acquisition  -> MEASUREMENT
```

Consumer：

```text
Display Task
```

全部 copy-by-value。

Display Queue 为 bounded FIFO；consumer 使用：

```text
wait first
 -> drain backlog
 -> update presentation cache
 -> render latest state once
```

这是一种 consumer-side latest-state coalescing，而不是 overwrite queue。

---

# 10. Display UI Contract

Boot Page：

```text
SENSOR MONITOR
STM32F4 + RTOS
STARTING...
```

Main UI：

```text
STATE : RUNNING / STOPPED
TEMP
HUM
ACCEL X/Y/Z (g)
GYRO X/Y/Z (dps)
```

首次有效采样前显示 `--`。

STOP 后保留 latest valid measurement。

局部刷新动态区域，不周期性全屏重绘。

---

# 11. SPI Architecture

```text
platform_spi_bus_t
 = Platform lifecycle device
 = MCU SPI Controller / Bus

platform_spi_device_t
 = lightweight slave descriptor
```

Transaction：

```text
begin
write
end
```

当前 Phase 1：

```text
blocking synchronous TX
8-bit
fixed CubeMX config validation
no runtime reconfiguration
no DMA
```

APP 不得直接调用 Impl SPI constructor。

正式 board binding：

```text
APP
 -> Platform BSP SPI constructor
 -> Impl SPI1
```

SPI Bus lifecycle 必须通过 Platform façade 调用，不让 APP 解引用 lifecycle function table。

---

# 12. ST7789 Resource Ownership

ST7789 owns：

```text
SPI Device descriptor
CS GPIO
DC GPIO
RST GPIO
BL GPIO
```

SPI Bus：

```text
shared non-owning dependency
```

ST7789 deinit 不 stop/deinit SPI Bus。

禁止 full framebuffer：

```text
240 * 280 * 2 = 134400 B > STM32F411 SRAM
```

采用 small fixed scratch buffer + chunked SPI write。

---

# 13. APP IPC Final Shape

Control Queue：

```text
CONTROL_REQUEST
ONCE_COMPLETE(result)
```

Acquisition Command Queue：

```text
START_PERIODIC
STOP_PERIODIC
SAMPLE_ONCE
```

Communication Response Queue：

```text
app_control_response_t
```

Display Queue：

```text
SYSTEM_STATE
MEASUREMENT
```

Indicator Queue：

```text
STOPPED
RUNNING
ONCE_SUCCESS
```

旧类型已删除：

```text
ONCE_ACQUISITION_FAILED
ONCE_TX_RESULT
PERIODIC_REPORT
ONCE_REPORT
```

---

# 14. Final Data Flow

```text
UART RX
 -> Communication
 -> Control
```

```text
Button
 -> Control
```

```text
Control
 -> Acquisition
 -> Unified Acquisition Service
```

```text
Acquisition success
 -> Display / MEASUREMENT
```

```text
Control state change
 -> Display / SYSTEM_STATE
```

```text
ONCE completion
Acquisition -> Control -> Indicator / optional UART response
```

```text
Control response
Control -> Communication -> UART
```

关键解耦：

```text
Acquisition -X-> Communication measurement output
Communication -X-> ONCE completion
Display -X-> business success definition
```

---

# 15. Static Resource Baseline

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

资源数值是当前已验证 bring-up baseline。Task stack high-water mark / Queue peak 可在后续资源优化阶段按需记录，不阻塞当前功能基线使用。

---

# 16. 当前正式设计入口

```text
00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
00_Doc/02_架构设计/ST7789_Graphics_Phase1设计.md
00_Doc/02_架构设计/RTOS_Display_Integration_Design.md
```
