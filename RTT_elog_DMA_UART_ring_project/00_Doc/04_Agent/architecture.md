# Embedded Firmware Architecture Contract

> 文档类型：Architecture Contract  
> 状态：Baseline  
> 版本：V2.2  
> 更新时间：2026-09-03  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 架构目标

本工程采用：

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

目标：

- 降低业务与 STM32 / HAL / FreeRTOS 的耦合；
- 明确产品状态、设备能力、数据处理和底层实现之间的边界；
- 明确 Buffer、ISR / Task、异步发送和共享总线所有权；
- 保持模块可测试、可替换、可维护；
- 在不破坏既有 UART 基线的前提下完成最终传感器采集闭环。

架构服务于当前工程，不为简单功能增加无实际收益的抽象。

---

# 2. 固定依赖规则

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

同时要求：

- APP / Service 不直接依赖 STM32 HAL、CubeMX Handle、Impl 私有接口；
- Platform 公共 Header 不暴露 HAL / FreeRTOS concrete handle；
- Impl 可以依赖 HAL、CMSIS、FreeRTOS、CubeMX Handle、IRQ / DMA；
- Vendor 不依赖产品业务代码；
- 下层事件向上传递使用 callback / event / notification / queue 等明确机制，不由 Impl 直接调用 APP 业务函数；
- 禁止为快速联调长期保留跨层捷径。

---

# 3. 最终系统结构

当前最终业务架构：

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
                           |            +-------> Indicator Event
                           |                         |
                           |                         v
                           |                  Indicator Task
                           |                         |
                           |                         v
                           |                 Indicator Service
                           |                         |
                           |                         v
                           |                    Platform LED
                           |
                           v
                     Acquisition Flow
                           |
                  +--------+--------+
                  |                 |
                  v                 v
                DHT20             MPU6050
                  \                 /
                   +-- Software I2C
                           |
                     Platform GPIO

Sensor Data
    -> APP / Communication
    -> UART Service
    -> Platform UART
    -> STM32 UART Impl
    -> DMA TX
    -> PC Serial Assistant

Runtime State / Error
    -> Service Log
    -> Platform Log
    -> EasyLogger Adapter
    -> RTT
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

# 4. APP 层

目录：

```text
01_APP/
```

APP 负责：

- 系统级对象装配；
- Task lifecycle；
- 产品级业务流程；
- STOPPED / RUNNING 唯一采集状态；
- 将按键事件和 UART 命令统一为控制语义；
- START / STOP / ONCE / STATUS 决策；
- 5 s 周期采集业务编排；
- 将采集结果交给既有通信链路；
- 决定 LED 产品语义；
- 根据 Service / Platform 返回结果做错误决策和关键日志。

APP 可以依赖 Service，也允许依赖经过架构确认的 Platform 公共能力。

APP 不应知道：

```text
UART_HandleTypeDef
DMA_HandleTypeDef
GPIO_TypeDef
GPIO_PIN_x
USART1 concrete registers
HAL_UART_xxx
HAL_GPIO_xxx
FreeRTOS concrete handles
RingBuffer internal indices
Impl private context
Button active HIGH / LOW polarity
```

按键和 UART 不得各自拥有一套 `running` 标志。

---

# 5. Service 层

目录：

```text
02_Service/
```

Service 提供可复用的软件语义和数据处理能力。

当前稳定模块：

```text
UART Service
Log Service
Service Common / RingBuffer composition
Indicator Service
```

最终功能允许新增的典型能力：

```text
Button Service
Environment Service
Motion Service
Command Parser（也可由 APP Communication 内聚，需专项设计确认）
```

Indicator Service 的职责是：

```text
STOPPED semantic event      -> LED OFF
RUNNING semantic event      -> LED ON
ONCE_SUCCESS semantic event -> blink 3 times -> OFF
```

Indicator Service 不负责：

- 维护 APP `RUNNING / STOPPED` 真实业务状态；
- 判断 ONCE 是否允许执行；
- 判断 UART 是否真正发送成功；
- 创建最终 Indicator Task；
- 直接依赖 STM32 HAL。

Button Service 第一阶段冻结为纯时间驱动 gesture recognizer：

```text
logical PRESSED / RELEASED + nowMs
 -> time-based debounce
 -> stable PRESS / RELEASE edge
 -> SINGLE / DOUBLE / LONG event
```

Button Service 不持有 `platform_button_t *`，不主动读取 GPIO，不主动获取 RTOS tick；调用方负责提供逻辑按键状态和单调毫秒时间。

Button Service 不负责：

- HIGH / LOW active-level translation；
- START / STOP 最终业务决策；
- APP RUNNING / STOPPED 状态；
- LED 常亮 / 常灭；
- 直接启动 DHT20 / MPU6050；
- 创建永久 Task / Queue。

第一版公共事件：

```text
NONE
SINGLE
DOUBLE
LONG
```

Sensor 类 Service 如引入，应负责数据转换、状态、错误语义，不直接依赖 HAL。

UART Service 继续只负责通信数据流、RingBuffer 和 UART 生命周期，不直接解释传感器业务。

---

# 6. Platform 层

目录：

```text
03_Platform/
├── platform_common/
├── platform_mcu/
├── platform_os/
├── platform_middleware/
└── platform_bsp/
    ├── led/
    └── button/
```

Platform 定义：

> 上层需要什么能力。

而不是：

> STM32 HAL 如何调用。

当前稳定 / 已冻结公共能力方向：

```text
Platform Common
Platform UART
Platform OS
Platform Log
Platform GPIO
Platform I2C
Platform LED
Platform Button
```

## 6.1 Platform GPIO

`platform_gpio_t` 是轻量 MCU Resource，不继承 `platform_device_t`，当前 Phase 1 公共合同保持冻结。

职责：

- INPUT / OUTPUT；
- PULL；
- OUTPUT TYPE；
- INITIAL LEVEL；
- configure / read / write / deinit；
- 隐藏具体 GPIO Port / Pin / HAL。

Platform GPIO 不知道：

```text
LED
Button
SCL
SDA
DHT20
MPU6050
```

设备语义和有效电平不得回流进 GPIO 通用接口。

## 6.2 Software I2C

Software I2C 是 GPIO 之上的通信协议能力：

```text
Software I2C
    -> Platform GPIO
    -> GPIO Impl
    -> HAL / Hardware
```

协议逻辑必须避免直接依赖 `HAL_GPIO_xxx`。

当前第一阶段已冻结：

- 依赖 Platform GPIO；
- 提供 DHT20 / MPU6050 所需 I2C transaction 能力；
- 使用 `platform_delay_us()` 微秒级 busy-wait；
- Master-only / 7-bit / synchronous；
- 不把传感器业务逻辑写入 I2C 层。

## 6.3 Platform LED

Platform LED 是 GPIO 之上的轻量执行器能力。

第一阶段对象不接入 `platform_device_t` / runtime device type，不建立 registry / manager。

职责：

```text
init
on
off
toggle
deinit
active-level translation
```

Platform LED 不负责 RUNNING / STOPPED / ONCE_SUCCESS 等产品状态。

LED 与底层 GPIO 为一对一关系，第一阶段 `platform_led_t` 直接拥有自己的 `platform_gpio_t` 存储。

## 6.4 Platform Button

Platform Button 是 GPIO 之上的轻量输入设备能力，与 LED 一样不为层级对称增加新的 STM32 Impl。

第一阶段 `platform_button_t`：

```text
caller-owned static object
owns one platform_gpio_t
stores activeLevel
stores pull
stores initialized state
no malloc/free
no platform_device_t
no registry / manager
```

职责：

```text
init
read -> PRESSED / RELEASED
deinit
active-level translation
input pull configuration
```

Platform Button 不负责：

```text
debounce
single / double / long
APP START / STOP / ONCE
RTOS Task ownership
```

冻结 User Key 静态板级属性：

```text
PA0
Input / Pull-Up / no EXTI
released = HIGH
pressed  = LOW
PROJECT_USER_KEY_ACTIVE_LEVEL = PLATFORM_GPIO_LEVEL_LOW
PROJECT_USER_KEY_PULL         = PLATFORM_GPIO_PULL_UP
```

## 6.5 Board / BSP

Board / BSP 表达具体板级设备语义，例如：

```text
Status LED
User Key
DHT20
MPU6050
```

适合封装：

- LED 高 / 低有效极性；
- Button 高 / 低有效极性与输入 pull；
- 具体 Soft I2C bus 绑定；
- 具体传感器地址 / 板级连接；
- 设备基础读写能力。

Status LED：

```text
platform_bsp_led_construct_status_led()
 -> platform_bsp_gpio_construct_status_led()
 -> PROJECT_STATUS_LED_ACTIVE_LEVEL
```

User Key：

```text
platform_bsp_button_construct_user_key()
 -> platform_bsp_gpio_construct_user_key()
 -> PROJECT_USER_KEY_ACTIVE_LEVEL
 -> PROJECT_USER_KEY_PULL
```

BSP 不负责 APP 的 START / STOP / ONCE 业务状态机。

---

# 7. Impl 层

目录：

```text
04_Impl/
├── impl_board/
├── impl_bsp/
├── impl_mcu/
├── impl_os/
└── impl_middleware/
```

Impl 回答：

> Platform 定义的能力在 STM32F411 + FreeRTOS 环境如何实现。

可以依赖：

- STM32 HAL；
- CMSIS；
- FreeRTOS；
- CubeMX Handle；
- IRQ / DMA；
- EasyLogger / RTT。

LED 与 Button 均不新增无职责透传 Impl：

```text
Platform LED / Platform Button
 -> Platform GPIO
 -> STM32 GPIO Impl
```

禁止为了层级对称增加：

```text
impl_led_on() -> platform_gpio_write()
impl_button_read() -> platform_gpio_read()
```

Impl 不负责：

- 按键 debounce / 单双击 / 长按；
- 5 s 采集策略；
- UART 命令语义；
- APP 状态机；
- DHT20 / MPU6050 产品级行为。

---

# 8. Vendor 层

包括：

```text
STM32 HAL
CMSIS
FreeRTOS
EasyLogger
SEGGER RTT
```

原则：

- 不因业务功能直接修改第三方源码；
- 通过 Impl Adapter / Vendor Port 适配；
- APP / Service 不直接调用 Vendor private API。

---

# 9. CubeMX 边界

CubeMX 生成文件：

```text
main.c
freertos.c
gpio.c
usart.c
stm32f4xx_it.c
...
```

只承担：

- Clock / peripheral 基础初始化；
- Scheduler 启动；
- IRQ / HAL Callback 入口；
- 与自研架构的薄胶水。

要求：

- 优先只改 `USER CODE` 区；
- 不在生成文件中写 APP FSM；
- 不在生成文件中写 Button Service；
- 不在生成文件中写 DHT20 / MPU6050 完整驱动；
- 不在生成文件中写软件 I2C 协议实现；
- 不将 CubeMX 文件作为长期 Service / Platform 模块；
- Phase 5 Smoke 临时 Task / Queue 入口必须在测试结束后移除。

---

# 10. 已冻结 UART RX 数据流

```text
USART1
  -> DMA Circular + IDLE / HT / TC
  -> STM32 UART Impl
  -> Platform UART RX_DATA / ERROR / CANCELED
  -> UART Service
  -> SPSC RingBuffer
  -> Platform Notify From ISR
  -> APP Communication Task
  -> Application byte-stream handling
```

新增命令解析必须消费该链路，不得重新设计第二套 RX。

UART 下层只产生字节流和通信事件，不负责识别 `START` / `STOP` 等业务命令。

---

# 11. UART TX 数据流

最终业务 TX：

```text
Sensor Data / Command Response
    -> APP / Communication
    -> UART Service / Platform UART
    -> STM32 UART Impl
    -> HAL / DMA
    -> USART1
    -> PC
```

异步 TX Buffer 生命周期继续遵守现有 UART 合同：

> 在 TX Complete / Error / Canceled 之前不得被修改或失效。

ONCE LED 成功反馈如依赖“发送成功”，应优先使用明确的 TX Complete 语义。

---

# 12. RingBuffer 与并发合同

RingBuffer 继续是：

```text
SPSC byte stream
caller-owned storage
usable capacity = storageSize - 1
no malloc/free
no HAL / UART / DMA knowledge
no silent overwrite
```

当前生产者 / 消费者：

```text
Producer = UART Service RX callback
Consumer = APP Communication Task
```

不得因为增加命令解析而给现有 SPSC 路径加入普通 Mutex。

命令解析发生在 Task 上下文。

---

# 13. Software I2C 并发合同

第一阶段 DHT20 与 MPU6050 共用一条软件 I2C 总线。

推荐第一版使用单一采集执行上下文串行访问：

```text
Acquisition context
    -> DHT20 transaction
    -> MPU6050 transaction
```

这样不需要为了两个设备提前引入总线 Mutex。

若未来存在多个并发访问者：

```text
LOCK BUS
START ... STOP
UNLOCK BUS
```

互斥范围必须覆盖完整 transaction，而不是单 byte。

---

# 14. RTOS 任务组织原则

第一阶段原则：

> Task 数量由真实并发职责决定，不由设备数量机械决定。

当前已确认的最终任务方向包括：

```text
Communication Task
- UART RingBuffer consumption
- text command assembly / parsing
- control event submission

Acquisition Task
- DHT20 + MPU6050 serialized acquisition
- 5 s periodic acquisition/report execution

Indicator Task
- consume indicator events
- execute RUNNING / STOPPED / ONCE_SUCCESS indication
- isolate blink delay from APP / UART / acquisition contexts
```

永久 Button 是否独立 Task 仍留到 Phase 9；Phase 5 不借 Smoke Test 提前冻结该决定。

Phase 5 目标板验证允许临时：

```text
Button Smoke Task
    -> 10 ms polling
    -> Platform Button read
    -> Platform OS time
    -> Button Service process
    -> temporary queue

Indicator Smoke Task
    -> consume mapped event
    -> Indicator Service
```

该结构只用于在真实 FreeRTOS Scheduler 下验证 Button + Indicator 联动，不是永久 Task 架构。

禁止无必要创建：

```text
DHT20 Task
MPU6050 Task
```

Indicator Task 已有明确并发职责，不属于“按设备机械拆 Task”。

具体 Thread、Queue、Notification、Timer、优先级、栈大小和永久事件缓存策略仍由 Phase 9 冻结。

---

# 15. APP Control FSM

业务状态只允许一个真实来源：

```text
STOPPED
RUNNING
```

统一控制来源：

```text
Button single -> START
Button double -> SAMPLE_ONCE
Button long   -> STOP
UART START    -> START
UART STOP     -> STOP
UART ONCE     -> SAMPLE_ONCE
UART STATUS   -> GET_STATUS
```

状态机负责：

- 是否允许事件；
- LED 产品状态；
- 是否启动 5 s 周期业务；
- ONCE 是否执行；
- 错误和命令响应。

Button Service 与 Command Parser 不得直接拥有产品状态。

---

# 16. LED 架构边界

LED 的业务语义：

```text
STOPPED -> OFF
RUNNING -> ON
ONCE successful TX -> blink 3 times -> OFF
```

冻结分层：

```text
APP decides semantic state
    -> Indicator Event
    -> Indicator Task
    -> Indicator Service
    -> Platform LED
    -> Platform GPIO
    -> GPIO Impl
```

APP 不知道 LED 是高电平还是低电平点亮。

连续 RUNNING 期间 5 s UART 发送不改变 LED 常亮状态。

LED 三闪不能在 ISR 内通过阻塞 delay 完成。

由于最终采用独立 Indicator Task，三闪允许在该 Task Context 内使用 `platform_time_delay_ms()` 顺序延时；不得直接调用 `HAL_Delay()`、`osDelay()` 或 `vTaskDelay()` 绕过 Platform OS。

---

# 17. Button 架构边界

正式能力链：

```text
PA0 electrical level
    -> Platform GPIO HIGH / LOW
    -> Platform Button PRESSED / RELEASED
    -> Button Service SINGLE / DOUBLE / LONG
    -> APP START / SAMPLE_ONCE / STOP
```

第一版采用 polling，不启用 EXTI。

冻结时间参数：

```text
sample period = 10 ms
debounce      = 30 ms
double window = 300 ms
long press    = 3000 ms
```

Debounce 使用稳定时间阈值，不以固定采样次数定义行为。

Gesture 规则：

```text
SINGLE = first stable release + double window timeout
DOUBLE = second stable press starts within <= 300 ms, then second stable release
LONG   = stable press duration >= 3000 ms, emit once immediately
```

LONG 产生后当前 click candidate 作废，松手不得再产生 SINGLE；第二击如果持续达到 long threshold，最终只产生 LONG，不产生 DOUBLE。

`nowMs` 由调用方注入，所有 elapsed-time 判断使用可处理 `uint32_t` wraparound 的差值方式。

Phase 5 Smoke 可以将 Button 事件临时映射到既有 Indicator Service：

```text
SINGLE -> RUNNING      -> LED ON
DOUBLE -> ONCE_SUCCESS -> blink 3 times -> OFF
LONG   -> STOPPED      -> LED OFF
```

该映射只用于测试。正式业务仍由 APP 决定，DOUBLE 不能直接等价于 ONCE_SUCCESS。

---

# 18. 日志架构

正式日志链：

```text
APP / Service
    -> service_log
    -> Platform Log
    -> EasyLogger Adapter
    -> RTT
```

日志职责：

```text
INFO  = 生命周期、控制事件、业务状态变化
DEBUG = 5 s 采集摘要、完整命令、业务 TX 状态
WARN  = 可恢复异常
ERROR = 初始化或关键通信失败
```

禁止把日志变成：

- UART byte trace；
- I2C bit / ACK trace；
- LED 每个亮灭边沿 trace；
- Button 每 10 ms polling trace；
- ISR 高频打印。

Platform / Impl 主要负责返回错误和事件。关键初始化信息可由 APP / Service 根据返回值统一记录，不要求所有底层层级直接调用日志 API。

---

# 19. Config / Context / Data

继续使用项目的数据模型原则：

```text
Config  -> 模块应怎样工作
Context -> 模块当前怎样运行
Data    -> 模块当前有什么结果
```

当前新增静态配置至少包括：

```text
Acquisition period = 5000 ms
PROJECT_USER_KEY_ACTIVE_LEVEL = LOW
PROJECT_USER_KEY_PULL = PULL_UP
PROJECT_BUTTON_SAMPLE_PERIOD_MS = 10 ms
PROJECT_BUTTON_DEBOUNCE_MS = 30 ms
PROJECT_BUTTON_DOUBLE_CLICK_MS = 300 ms
PROJECT_BUTTON_LONG_PRESS_MS = 3000 ms
Status LED active level = LOW
LED one-shot blink count = 3
LED blink on time = 100 ms
LED blink off time = 100 ms
```

业务状态属于 Context，不应混入 Config。

Button Service 的 raw / stable / gesture / timestamps 属于 Context。

传感器采集结果属于 Data，不应作为全局散乱变量跨层共享。

---

# 20. 当前冻结与待设计边界

已冻结 / 已验证：

```text
Platform UART
UART Service
SPSC RingBuffer RX
APP Communication Phase 1
Platform OS
Service Log / Platform Log / RTT
Platform GPIO Phase 1 public contract
STM32 GPIO Impl + board GPIO binding
Software I2C Phase 1
LED Phase 1 frozen design / implemented / verified
Button Phase 5 design baseline frozen
```

Button 当前状态：

```text
Platform Button / BSP Button boundary      FROZEN
User Key active level / pull               FROZEN
polling / time contract                    FROZEN
Button Service event contract              FROZEN
debounce / single / double / long rules    FROZEN
Host Test strategy                         FROZEN
FreeRTOS target smoke strategy             FROZEN
implementation                             PENDING
```

仍需逐项专项设计 / 实现：

```text
DHT20
MPU6050
UART application communication
Final permanent RTOS event delivery details
APP final acquisition/control FSM
5 s report integration
final board verification
```

不得把上述多个子系统一次性无设计地塞进同一实现提交。

---

# 21. 当前明确不做

```text
SPI / LCD
Roll / Pitch / Yaw
DMP
Kalman / complementary filter
W25Q64 / AT24C02
Bluetooth
复杂 GUI
复杂二进制应用协议
Button EXTI in Phase 5
与最终验收无关的通用框架扩张
```

---

# 22. 架构核心结论

```text
UART 基线继续冻结。
GPIO 是底层通用能力。
Software I2C 建立在 GPIO 之上。
Platform LED 建立在 GPIO 之上。
Platform Button 建立在 GPIO 之上。
Platform Button 只表达 PRESSED / RELEASED。
Button Service 只表达 SINGLE / DOUBLE / LONG。
Indicator Service 表达提示灯语义。
Indicator Task 隔离提示灯事件与阻塞闪烁。
Sensor 驱动建立在 I2C 之上。
Button / UART 只产生控制输入。
APP FSM 是唯一业务状态源。
UART 输出业务数据。
RTT 输出内部诊断。
```

任何下一阶段实现都必须先确定自己属于上述哪一职责，再进入专项设计和执行计划。
