# Embedded Firmware Architecture Contract

> 文档类型：Architecture Contract  
> 状态：Baseline  
> 版本：V2.5  
> 更新时间：2026-09-04  
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

Acquisition Result
   ↓
Communication Task
   ↓
UART Service
   ↓
Platform UART async TX
   ↓
STM32 UART Impl / DMA2_Stream7
   ↓
USART1 TX
   ↓
PC
```

核心原则：

```text
APP Control FSM = 唯一业务状态源
Button / UART   = 控制输入
Sensor          = 数据来源
Communication   = UART 文本协议入口/出口
UART Service    = 传输服务
UART Platform   = 通用 UART 硬件能力
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
2 s 周期采集编排
Button / UART 控制事件统一处理
LED 产品语义决策
UART 数据上报编排
```

APP Communication 负责：

```text
UART byte stream -> strict CRLF line
START / STOP / ONCE / STATUS / HELP command parsing
protocol-local HELP / invalid / overflow response
APP_CTRL_* event conversion
future APP control result -> UART text
future acquisition result -> business report text
```

APP Communication 不负责：

```text
STOPPED / RUNNING truth
Sensor access
LED control
permanent RTOS IPC policy
```

Button Service 只负责：

```text
PRESSED / RELEASED + nowMs
 -> debounce
 -> SINGLE / DOUBLE / LONG
```

Indicator Service 只负责：

```text
STOPPED      -> LED OFF
RUNNING      -> LED ON
ONCE_SUCCESS -> blink 3 times -> OFF
```

UART Service 负责：

```text
RX stream / RingBuffer / data-loss detection
UART transport lifecycle
TX DMA transaction lifecycle
timeout / cancel / transport statistics
```

UART Service 不解释应用命令，不控制 LED、DHT20、MPU6050 或 APP 状态。

后续统一 Acquisition Service 负责组合 DHT20 + MPU6050 的采集能力；不为单个传感器建立空转发 Service。

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

## Platform DHT20

```text
caller-owned lightweight sensor context
non-owning reference to shared platform_i2c_t
init / read / deinit
raw + converted RH/T data
atomic measurement output
```

## Platform MPU6050

```text
caller-owned lightweight sensor context
non-owning reference to shared platform_i2c_t
7-bit address = 0x68 / 0x69
WHO_AM_I expected = 0x68
init = identity verify + wake + fixed configuration
read = one 14-byte burst from 0x3B
raw + g / dps six-axis output
atomic measurement output
```

---

# 5. UART / RingBuffer / DMA Contract

## 5.1 RX stable path

```text
USART1 RX
 -> DMA2_Stream2 Circular
 -> IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform UART RX_DATA
 -> UART Service
 -> SPSC RingBuffer
 -> Platform Notify From ISR
 -> Communication Task
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
Consumer = Communication Task
```

不得给当前 SPSC RX 路径增加普通 Mutex。

## 5.2 TX DMA target contract

CubeMX hardware configuration：

```text
USART1_TX
DMA2_Stream7
Channel 4
Memory -> Peripheral
Mode = Normal
IRQ priority = 5
```

当前状态：

```text
hardware/generated configuration = READY
production async TX path         = IMPLEMENTED / HOST + KEIL VERIFIED
target verification              = DEFERRED TO PHASE 9
```

冻结正式链：

```text
Communication Task
 -> UART Service service_uart_write()
 -> Platform UART write_async()
 -> STM32 UART Impl
 -> HAL_UART_Transmit_DMA()
 -> DMA2_Stream7 Normal
 -> HAL_UART_TxCpltCallback()
 -> PLATFORM_UART_EVENT_TX_COMPLETE
 -> UART Service wakes owner Task
```

RX / TX transaction 状态独立：

```text
rxActive
 txActive
```

允许 RX + TX 同时 active；第二个同方向 transaction 返回 BUSY。

## 5.3 Platform TX buffer ownership

`platform_uart_write_async()` 成功后直到 TX_COMPLETE / TX CANCELED / terminal TX error：

```text
caller keeps buffer valid and unchanged
```

Impl 不复制、不释放 caller buffer。

## 5.4 UART Service TX semantics

第一版 Service 只向 Task 提供同步完成语义：

```text
service_uart_write(data, length, timeout)
```

内部使用 async DMA + notify wait。

强保证：

```text
service_uart_write() returns
=> DMA no longer accesses caller TX buffer
```

不增加：

```text
public Service async TX API
TX RingBuffer
TX Queue
TX worker Task
buffer pool
```

TX 单次 transaction failure 不自动破坏健康 RX session；RX / device fatal error 才进入 Service ERROR 路径。

## 5.5 TX wait / timeout

`service_uart_wait_event()` 继续用于 RX / Service observable event，不用于等待 TX completion。

TX 使用内部专用 wait：

```text
Notify = wake hint
TX Context = truth
```

必须用 `platform_time_get_ms()` 维持总 timeout deadline；无关 RX notification 不得延长 TX timeout。

---

# 6. UART Application Protocol Contract

命令：

```text
START\r\n
STOP\r\n
ONCE\r\n
STATUS\r\n
HELP\r\n
```

第一版 framing：

```text
strict CRLF
uppercase only
case-sensitive
no trim
no parameters
no dynamic allocation
```

RingBuffer read boundary != command boundary，必须正确处理：

```text
fragmented command
multiple commands in one chunk
CRLF split across chunks
```

Line Context：

```text
commandLine[]
commandLength
pendingCr
discardLine
```

建议产品配置：

```text
PROJECT_COMM_COMMAND_LINE_BUFFER_SIZE = 32U
```

超长或 malformed line 一旦确认非法，整行丢弃直到下一个 CRLF；不得从同行后半段重新识别合法命令。

非法命令属于 protocol error，不进入 APP Communication fatal ERROR。

---

# 7. APP Control Contract

唯一业务状态：

```text
STOPPED
RUNNING
```

统一控制事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

映射：

```text
Button SINGLE -> APP_CTRL_START
Button DOUBLE -> APP_CTRL_SAMPLE_ONCE
Button LONG   -> APP_CTRL_STOP
UART START    -> APP_CTRL_START
UART STOP     -> APP_CTRL_STOP
UART ONCE     -> APP_CTRL_SAMPLE_ONCE
UART STATUS   -> APP_CTRL_GET_STATUS
```

`HELP` 属于 Communication-local command，不进入 APP FSM。

Phase 8 只冻结逻辑 control handler outlet；Queue / direct call / consumer Task 留到 Phase 9。

handler submission result != business execution result。

Button / UART 不得各自维护 running 标志。

---

# 8. USART1 Product TX Ownership

第一阶段冻结：

```text
Communication Task = sole USART1 product TX requester
```

未来：

```text
Acquisition Task
 -> acquisition result IPC
 -> Communication Task
 -> format report
 -> service_uart_write()
```

Acquisition Task 不直接 TX。

不使用普通 TX mutex 解决多个生产者；真正多来源消息先汇聚到 Communication Task。

---

# 9. USART1 / RTT Channel Isolation

正式职责：

```text
USART1 -> product control + product data
RTT    -> initialization / state / diagnostics / errors
```

Phase 8 production implementation 已退出旧正式旁路：

```text
printf / fputc -> HAL_UART_Transmit(&huart1)
uartMutexHandle
USART1_mutex_Init()
```

不得让 direct HAL blocking TX 与 UART Service DMA TX 同时成为正式 owner。

---

# 10. ISR / Task / 并发合同

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
APP Control consumer    NOT YET FROZEN
```

Phase 9 冻结 permanent IPC / priority / stack / buffering。

---

# 11. Software I2C 并发

DHT20 与 MPU6050 共用软件 I2C。

第一阶段固定：

```text
one Acquisition context
 -> DHT20 complete transaction
 -> MPU6050 complete transaction
```

当前不增加 Mutex。

只有出现真实多个并发访问者时才增加 transaction-level Mutex。

---

# 12. Config / Context / Data

```text
Config  = 模块应怎样工作
Context = 模块当前怎样运行
Data    = 模块当前有什么结果
```

UART Phase 8：

```text
command buffer size       -> Config
line length / pending CR  -> Context
parsed command            -> transient Data
TX active / TX result     -> Service Context
APP STOPPED / RUNNING     -> APP Control Context, NOT Communication Context
```

产品统一采集周期：

```text
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

---

# 13. 日志合同

正式链：

```text
APP / Service
 -> service_log
 -> Platform Log
 -> EasyLogger Adapter
 -> RTT
```

禁止正常运行时逐 UART byte、逐 DMA step、逐 I2C bit / ACK、逐 Button polling 刷日志。

---

# 14. Phase Status

```text
Phase 3 Software I2C   COMPLETED
Phase 4 LED            COMPLETED
Phase 5 Button         COMPLETED / TARGET VERIFIED
Phase 6 DHT20          COMPLETED / TARGET VERIFIED
Phase 7 MPU6050        COMPLETED / TARGET VERIFIED
Phase 8 UART App       IMPLEMENTATION COMPLETED / HOST + KEIL VERIFIED
                        TARGET VERIFICATION DEFERRED TO PHASE 9
```

Phase 8 正式设计：

```text
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
```

当前明确不做：SPI / LCD / GUI、W25Q64、AT24C02、Bluetooth、姿态融合、Button EXTI、无需求驱动框架扩张。
