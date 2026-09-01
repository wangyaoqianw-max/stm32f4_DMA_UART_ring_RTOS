# Embedded Firmware Architecture Contract

> 文档类型：Architecture Contract  
> 状态：Baseline  
> 版本：V2.0  
> 更新时间：2026-09-01  
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
                             +---------------- KEY
                             |
                             v
PC -> UART RX -> RingBuffer -> Command Parser
                             |
                             v
                      APP Control FSM
                      STOPPED / RUNNING
                       |            |
                       |            +-------> LED
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
KEY / UART      = 控制输入
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
```

最终功能允许新增的典型能力：

```text
Button Service
Environment Service
Motion Service
Command Parser（也可由 APP Communication 内聚，需专项设计确认）
```

Button Service 的职责是：

```text
raw key state
 -> debounce
 -> single / double / long event
```

Button Service 不负责：

- START / STOP 最终业务决策；
- LED 常亮 / 常灭；
- 直接启动 DHT20 / MPU6050。

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
```

Platform 定义：

> 上层需要什么能力。

而不是：

> STM32 HAL 如何调用。

当前稳定公共能力：

```text
Platform Common
Platform UART
Platform OS
Platform Log
Platform GPIO
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
KEY
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

Software I2C 的具体目录、对象模型、delay-us 注入方式和公共 API 必须在专项设计中冻结后再编码。

架构只先冻结以下边界：

- 依赖 Platform GPIO；
- 提供 DHT20 / MPU6050 所需 I2C transaction 能力；
- 使用微秒级 delay；
- 不使用 RTOS tick delay 直接 bit-bang；
- 不把传感器业务逻辑写入 I2C 层。

## 6.3 Board / BSP

Board / BSP 表达具体板级设备语义，例如：

```text
Status LED
User Key
DHT20
MPU6050
```

适合封装：

- LED 高 / 低有效极性；
- KEY 高 / 低有效极性；
- 具体 Soft I2C bus 绑定；
- 具体传感器地址 / 板级连接；
- 设备基础读写能力。

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

当前下一阶段可能需要新增的具体实现是 GPIO STM32 Impl，但必须先经过对应专项设计 / 计划确认。

Impl 不负责：

- 按键单双击业务；
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
- 不将 CubeMX 文件作为长期 Service / Platform 模块。

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
    -> UART Service
    -> Platform UART
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

> Task 数量由真实并发职责决定，不由设备数量决定。

推荐逻辑职责：

```text
Communication Task
- UART RingBuffer consumption
- text command assembly / parsing
- control event submission

Acquisition / Control execution context
- APP state handling
- 5 s periodic acquisition
- ONCE execution

Button processing context
- polling / debounce / click recognition
```

具体采用 Thread、Queue、Notification、Timer 还是复用现有 Task，必须在对应专项设计中确定。

禁止无必要创建：

```text
DHT20 Task
MPU6050 Task
LED Task
```

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

分层：

```text
APP decides semantic state
    -> LED / BSP capability
    -> Platform GPIO
    -> GPIO Impl
```

APP 不知道 LED 是高电平还是低电平点亮。

连续 RUNNING 期间 5 s UART 发送不改变 LED 常亮状态。

LED 三闪不能在 ISR 内通过阻塞 delay 完成。

---

# 17. 日志架构

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
- ISR 高频打印。

Platform / Impl 主要负责返回错误和事件。关键初始化信息可由 APP / Service 根据返回值统一记录，不要求所有底层层级直接调用日志 API。

---

# 18. Config / Context / Data

继续使用项目的数据模型原则：

```text
Config  -> 模块应怎样工作
Context -> 模块当前怎样运行
Data    -> 模块当前有什么结果
```

当前新增静态配置至少包括：

```text
Acquisition period = 5000 ms
Button debounce
Button double-click window
Button long-press = 3000 ms
LED one-shot blink count = 3
LED blink interval
```

放入 `00_Config`，具体宏名在专项设计中确认。

业务状态属于 Context，不应混入 Config。

传感器采集结果属于 Data，不应作为全局散乱变量跨层共享。

---

# 19. 当前冻结与待设计边界

已冻结 / 已验证：

```text
Platform UART
UART Service
SPSC RingBuffer RX
APP Communication Phase 1
Platform OS
Service Log / Platform Log / RTT
Platform GPIO Phase 1 public contract
```

当前需求已冻结、但专项设计尚需逐项完成：

```text
STM32 GPIO Impl + board binding
LED / KEY BSP
Button event service
Software I2C
DHT20
MPU6050
APP final acquisition/control FSM
UART command parser integration
5 s report integration
final board verification
```

不得把上述多个子系统一次性无设计地塞进同一实现提交。

---

# 20. 当前明确不做

```text
SPI / LCD
Roll / Pitch / Yaw
DMP
Kalman / complementary filter
W25Q64 / AT24C02
Bluetooth
复杂 GUI
复杂二进制应用协议
与最终验收无关的通用框架扩张
```

---

# 21. 架构核心结论

```text
UART 基线继续冻结。
GPIO 是底层通用能力。
Software I2C 建立在 GPIO 之上。
Sensor 驱动建立在 I2C 之上。
Button / UART 只产生控制输入。
APP FSM 是唯一业务状态源。
UART 输出业务数据。
RTT 输出内部诊断。
```

任何下一阶段实现都必须先确定自己属于上述哪一职责，再进入专项设计和执行计划。