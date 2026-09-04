# Embedded Firmware Architecture Contract

> 文档类型：Architecture Contract  
> 状态：Baseline  
> 版本：V2.3  
> 更新时间：2026-09-03  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 总体架构

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

依赖规则：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

APP / Service 不直接依赖 HAL、CubeMX Handle、Impl 私有接口或 FreeRTOS concrete handle。

CubeMX 文件只承担基础初始化、Scheduler、IRQ / HAL Callback 和薄胶水。

---

# 2. 最终系统主链

```text
User Key
   ↓
Platform Button
   ↓ PRESSED / RELEASED
Button Service
   ↓ SINGLE / DOUBLE / LONG
   +-----------------------------+
                                 |
PC -> UART RX -> RingBuffer -> Command Parser
                                 |
                                 v
                          APP Control FSM
                          STOPPED / RUNNING
                           |            |
                           |            +-----> Indicator Event
                           |                       ↓
                           |                 Indicator Task
                           |                       ↓
                           |                Indicator Service
                           |                       ↓
                           |                  Platform LED
                           |
                           v
                     Acquisition Flow
                           |
                  +--------+--------+
                  |                 |
                DHT20             MPU6050
                  \                 /
                   +-- Software I2C
                           ↓
                     Platform GPIO
```

核心原则：

```text
APP Control FSM = 唯一业务状态源
Button / UART   = 控制输入
Sensor          = 数据来源
UART            = 业务数据通道
RTT             = 诊断通道
LED             = 用户状态反馈
```

---

# 3. APP / Service 边界

APP 负责：

```text
系统级对象装配
Task lifecycle
STOPPED / RUNNING 唯一状态
START / STOP / ONCE / STATUS 业务决策
5 s 周期采集编排
Button / UART 控制事件统一处理
LED 产品语义决策
UART 数据上报编排
```

Button Service 只负责：

```text
PRESSED / RELEASED + nowMs
 -> debounce
 -> SINGLE / DOUBLE / LONG
```

Button Service 不负责：

```text
GPIO active level
APP RUNNING / STOPPED
START / STOP / ONCE 决策
LED / Sensor 控制
Task / Queue ownership
```

Indicator Service 只负责：

```text
STOPPED      -> LED OFF
RUNNING      -> LED ON
ONCE_SUCCESS -> blink 3 times -> OFF
```

UART Service 继续只负责通信数据流、RingBuffer 和 UART 生命周期，不解释应用命令业务。

---

# 4. Platform 设备能力

## GPIO

```text
lightweight resource
INPUT / OUTPUT
PULL
OUTPUT TYPE
read / write / configure / deinit
no LED / Button / Sensor semantics
```

## Software I2C

```text
Platform GPIO based
Master only
7-bit
synchronous
no internal mutex
platform_delay_us() timing
```

## Platform LED

```text
lightweight actuator
caller-owned
owns one platform_gpio_t
active-level translation
no platform_device_t
no impl_led
```

## Platform Button

Phase 5 已实现并验证：

```text
lightweight input device
caller-owned
owns one platform_gpio_t
activeLevel + pull + initialized
read -> PRESSED / RELEASED
no platform_device_t
no registry / manager
no impl_button
```

User Key：

```text
PA0
Input / Pull-Up / no EXTI
released = HIGH
pressed  = LOW
PROJECT_USER_KEY_ACTIVE_LEVEL = PLATFORM_GPIO_LEVEL_LOW
PROJECT_USER_KEY_PULL         = PLATFORM_GPIO_PULL_UP
```

Platform Button 不做 debounce / single / double / long。

---

# 5. Button Service 冻结合同

配置：

```text
sample period = 10 ms
debounce      = 30 ms
double window = 300 ms
long press    = 3000 ms
```

输入 / 输出：

```text
input  = platform_button_state_t + uint32_t nowMs
output = NONE / SINGLE / DOUBLE / LONG
```

算法规则：

```text
Debounce = stable elapsed time, not N samples
SINGLE = first stable release + double-window timeout
DOUBLE = second stable PRESS starts within <= 300 ms, then second stable release
LONG = stable PRESS >= 3000 ms, emit exactly once
LONG release = no SINGLE
second press LONG = LONG only
```

时间判断统一使用 wraparound-safe elapsed arithmetic：

```c
(uint32_t)(nowMs - startMs)
```

Button 第一阶段采用 polling，不启用 EXTI。

Phase 5 实现状态：

```text
Platform Button / BSP Button              IMPLEMENTED / VERIFIED
Button Service                            IMPLEMENTED / VERIFIED
Host Tests                                PASS
Keil                                      PASS
Target FreeRTOS Smoke                     PASS
Temporary Smoke path                      REMOVED
```

---

# 6. UART / RingBuffer 稳定合同

RX：

```text
USART1
 -> DMA Circular + IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform UART Event
 -> UART Service
 -> SPSC RingBuffer
 -> Platform Notify From ISR
 -> APP Communication Task
```

RingBuffer：

```text
SPSC byte stream
caller-owned storage
usable capacity = storageSize - 1
no malloc/free
no silent overwrite
```

当前：

```text
Producer = UART Service RX callback
Consumer = APP Communication Task
```

不得给当前 SPSC 路径增加普通 Mutex。

---

# 7. ISR / Task / 并发合同

ISR / HAL Callback 只允许 capture、必要数据搬运、轻量状态和 ISR-safe notify。

禁止在 ISR 中执行：

```text
Button gesture FSM
Software I2C transaction
完整命令解析
Sensor business
LED blocking blink
malloc/free
ordinary mutex
大量格式化日志
```

当前永久 Task 方向：

```text
Communication Task      CONFIRMED
Acquisition Task        PLANNED
Indicator Task          CONFIRMED
Button context          NOT YET FROZEN
```

Indicator Task 用于隔离三闪阻塞时序。

Phase 5 目标板曾使用临时 Button Smoke Task + Indicator Smoke Task + Platform Queue；该 Harness 已删除，不构成永久 RTOS 架构。

永久 Button Task / priority / stack / IPC / event buffering 留到 Phase 9。

---

# 8. Software I2C 并发

DHT20 与 MPU6050 第一阶段共用软件 I2C。

优先：

```text
one Acquisition context
 -> DHT20 transaction
 -> MPU6050 transaction
```

只有出现真实多个并发访问者时才增加 Mutex，且互斥必须覆盖完整 transaction。

---

# 9. APP Control FSM 冻结方向

唯一状态：

```text
STOPPED
RUNNING
```

统一控制映射：

```text
Button SINGLE -> START
Button DOUBLE -> SAMPLE_ONCE
Button LONG   -> STOP
UART START    -> START
UART STOP     -> STOP
UART ONCE     -> SAMPLE_ONCE
UART STATUS   -> GET_STATUS
```

Button / UART 不得各自维护 running 标志。

正式 DOUBLE 业务：

```text
DOUBLE
 -> APP SAMPLE_ONCE
 -> DHT20 + MPU6050 acquisition
 -> UART TX
 -> TX success
 -> ONCE_SUCCESS
 -> LED 3 blinks -> OFF
```

---

# 10. Config / Context / Data

```text
Config  = 模块应怎样工作
Context = 模块当前怎样运行
Data    = 模块当前有什么结果
```

已冻结静态配置：

```text
UART communication parameters
Software I2C timing
Status LED active level / blink timing
User Key active level / pull
Button sample / debounce / double / long timing
```

Button raw / stable / gesture / timestamps 属于 Context。

Sensor readings 属于 Data。

---

# 11. 日志合同

正式链：

```text
APP / Service
 -> service_log
 -> Platform Log
 -> EasyLogger Adapter
 -> RTT
```

禁止正常运行时逐 UART byte、逐 I2C bit / ACK、逐 LED edge、逐 Button 10 ms polling 刷日志。

---

# 12. 当前阶段状态

```text
Phase 3 Software I2C   COMPLETED
Phase 4 LED            COMPLETED
Phase 5 Button         COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
Phase 6 DHT20          COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

Phase 5 不再有 `implementation PENDING` 状态。

Phase 6 已按冻结专项设计实现，并完成 RTT、逻辑分析仪与连续约 2 s 实板验证。

当前明确不做：SPI / LCD / GUI、W25Q64、AT24C02、Bluetooth、姿态融合、Button EXTI、无需求驱动框架扩张。
