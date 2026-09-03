# Final Acquisition System Development Roadmap

> 文档类型：Development Phase Roadmap  
> 状态：Baseline  
> 日期：2026-09-03  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 文档目的

本文档用于把最终功能需求拆分为可独立设计、实现、验证和交接的开发阶段。

本文档只回答：

```text
后续分成哪些 Phase？
各 Phase 依赖什么？
各 Phase 做到哪里停止？
什么条件下可以进入下一 Phase？
```

本文档不是具体执行计划；当前执行计划只使用：

```text
00_Doc/04_Agent/implementation_plan.md
```

切换 Phase 前，应先完成专项设计，再重写 / 更新 `implementation_plan.md`。

最终功能需求以 `00_Doc/00_项目需求/最终功能需求.md` 为准，架构边界以 `00_Doc/04_Agent/architecture.md` 为准。

---

# 2. 当前基线

已经完成并作为后续开发基础复用：

```text
UART Platform / STM32 Impl
UART DMA RX / TX
UART Service
SPSC RingBuffer
APP Communication Phase 1
Platform OS
Service Log + EasyLogger + RTT
Platform GPIO Phase 1 / Host Verified
GPIO STM32 Impl / Board Resource / CubeMX Configuration
Software I2C / Host + Keil + DHT20 Target Smoke Verified
LED Module / Host + Keil + Target Board Verified
```

当前最终闭环尚缺：

```text
Button implementation
DHT20
MPU6050 basic motion data
UART application command / report
Final RTOS scheduling
Final APP Control FSM
Integrated board verification
```

---

# 3. 总体阶段顺序

```text
Phase 1  GPIO STM32 Impl                         COMPLETED
    ↓
Phase 2  Board Resource + CubeMX Configuration   COMPLETED
    ↓
Phase 3  Software I2C                            COMPLETED
    ↓
Phase 4  LED Module                              COMPLETED
    ↓
Phase 5  Button Module                           CURRENT / DESIGN FROZEN
    ↓
Phase 6  DHT20 Environment Module
    ↓
Phase 7  MPU6050 Motion Module
    ↓
Phase 8  UART Application Communication
    ↓
Phase 9  RTOS Task / Event Design
    ↓
Phase 10 Final APP Integration
    ↓
Final Integrated Board Test
```

---

# 4. Phase 1 — GPIO STM32 Impl

目标：为 Host Verified 的 `Platform GPIO` 提供 STM32F411 + HAL 具体实现。

范围：

```text
configure
write
read
deinit
```

不做：

```text
LED 产品语义
KEY 产品语义
Debounce
Soft I2C
EXTI
Button click state machine
```

完成门槛：Platform GPIO 公共 API 稳定、STM32 Impl 编译通过、无 APP / Service 反向依赖、Coding Standard Review PASS。

状态：`COMPLETED`。

---

# 5. Phase 2 — Board Resource + CubeMX Configuration

冻结：

```text
PC13 -> Status LED
PA0  -> User Key
PB6  -> Software I2C SCL
PB7  -> Software I2C SDA
USART1 existing resources
```

User Key 目标板已确认：

```text
PA0
Input / Pull-Up / no EXTI
released HIGH
pressed LOW
```

完成门槛：Keil Build、输入可读、输出可控、资源无冲突、CubeMX 边界符合规范。

状态：`COMPLETED / TARGET BOARD VERIFIED`。

---

# 6. Phase 3 — Software I2C

目标：基于 Platform GPIO 实现与 STM32 HAL 解耦的软件 I2C，为 DHT20 / MPU6050 提供统一通信能力。

冻结：

```text
Master-only
7-bit
synchronous
START / STOP / ACK / NACK
multi-byte read / write
write-read / repeated START
platform_delay_us()
Open-Drain + external pull-up
```

完成门槛：Host / Keil / DHT20 Target Smoke / Logic Analyzer 验证。

状态：`COMPLETED`。

---

# 7. Phase 4 — LED Module

专项设计：

```text
00_Doc/02_架构设计/LED_Phase1设计.md
```

冻结分层：

```text
Indicator Service
        ↓
Platform LED
        ↓
Platform GPIO
        ↓
STM32 GPIO Impl
```

Platform LED：轻量、caller-owned、无 `platform_device_t`、无 `impl_led`。

Indicator Service：

```text
STOPPED      -> OFF
RUNNING      -> ON
ONCE_SUCCESS -> blink 3 times -> OFF
```

配置：

```text
active level = LOW
blink count = 3
blink ON = 100 ms
blink OFF = 100 ms
```

目标板 Smoke 在 FreeRTOS Scheduler 启动后的临时 Task Context 中执行，使用 LED visual + RTT + Serial Assistant regression observation。

状态：`COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED`。

---

# 8. Phase 5 — Button Module

## 8.1 当前状态

```text
DESIGN BASELINE FROZEN
IMPLEMENTATION PENDING
```

本 Phase 设计讨论已经完成。下一步先形成正式 Button 设计文档，再生成新的 `implementation_plan.md`，之后才允许 Codex 实现。

## 8.2 目标与分层

建立完整链：

```text
PA0 electrical HIGH / LOW
    ↓
Platform GPIO
    ↓
Platform Button -> PRESSED / RELEASED
    ↓
Button Service -> SINGLE / DOUBLE / LONG
    ↓
Future APP -> START / SAMPLE_ONCE / STOP
```

Phase 5 不实现最终 APP Control FSM。

## 8.3 Platform Button / BSP Button

第一阶段冻结：

```text
platform_button_t
- lightweight input device
- caller-owned static object
- owns one platform_gpio_t
- activeLevel
- pull
- initialized
- no malloc/free
- no platform_device_t
- no registry / manager
- no impl_button layer
```

公共能力方向：

```text
platform_button_init
platform_button_read -> PRESSED / RELEASED
platform_button_deinit
```

BSP：

```text
platform_bsp_button_construct_user_key()
 -> platform_bsp_gpio_construct_user_key()
 -> active level + pull
```

Config：

```text
PROJECT_USER_KEY_ACTIVE_LEVEL = PLATFORM_GPIO_LEVEL_LOW
PROJECT_USER_KEY_PULL         = PLATFORM_GPIO_PULL_UP
```

## 8.4 Button Service

Service 不持有 `platform_button_t *`，不主动读取 GPIO，不直接获取 RTOS tick。

输入：

```text
logical button state: PRESSED / RELEASED
nowMs: caller-provided monotonic uint32_t ms
```

输出：

```text
NONE
SINGLE
DOUBLE
LONG
```

内部：

```text
time-based debounce
    ↓
stable PRESS / RELEASE edge
    ↓
gesture FSM
```

建议 process 形态：

```text
service_button_process(service, buttonState, nowMs, &event)
```

## 8.5 时间参数

冻结：

```text
PROJECT_BUTTON_SAMPLE_PERIOD_MS = 10 ms
PROJECT_BUTTON_DEBOUNCE_MS      = 30 ms
PROJECT_BUTTON_DOUBLE_CLICK_MS  = 300 ms
PROJECT_BUTTON_LONG_PRESS_MS    = 3000 ms
```

消抖以“状态持续时间 >= 30 ms”确认，不用固定 N 次采样定义。

## 8.6 Gesture 边界

单击 / 双击：

```text
first stable RELEASE
 -> wait second click
 -> second stable PRESS within <= 300 ms -> SECOND_PRESS
 -> second stable RELEASE -> DOUBLE
 -> timeout with no second PRESS -> SINGLE
```

窗口从第一次稳定 RELEASE 开始，只要求第二次稳定 PRESS 在窗口内开始。

长按：

```text
stable PRESS duration >= 3000 ms -> LONG immediately once
```

规则：

```text
2999 ms -> not LONG
3000 ms -> LONG
LONG only once
LONG release -> no SINGLE
second press becomes LONG -> LONG only, no DOUBLE
```

时间比较必须使用 elapsed-time 差值，支持 `uint32_t` 自然回绕。

## 8.7 Host Test

至少建立：

```text
Tests/platform_button/
Tests/platform_bsp_button/
Tests/service_button/
```

验收：

```text
active-low / active-high translation
User Key LOW active + PULL_UP binding
debounce bounce suppression
single only
DOUBLE only, no SINGLE
a long hold emits LONG once
LONG release no SINGLE
2999 / 3000 ms boundary
double-click 300 ms boundary
expired double window behavior
second press long conflict
initial pressed behavior
uint32_t wraparound
irregular process interval
```

Host Test 使用逻辑时间，不真实 sleep 3 s。

## 8.8 FreeRTOS Target Smoke

目标板验证必须在 FreeRTOS Scheduler 正常运行后执行。

临时结构：

```text
Button Smoke Task
    -> every ~10 ms
    -> platform_button_read()
    -> platform_time_get_ms()
    -> service_button_process()
    -> temporary Queue

Indicator Smoke Task
    -> consume mapped event
    -> service_indicator_handle_event()
    -> Platform LED
```

延时统一使用 `platform_time_delay_ms()`。

两个临时 Task 的原因：`ONCE_SUCCESS` 三闪会阻塞 Indicator 调用上下文约 600 ms，不能因此停止 Button 10 ms polling。

观察通道必须同时包括：

```text
USART1 Serial Assistant
RTT / EasyLogger
LED visual behavior
```

Smoke-only 映射：

```text
SINGLE -> SERVICE_INDICATOR_EVENT_RUNNING      -> LED ON
DOUBLE -> SERVICE_INDICATOR_EVENT_ONCE_SUCCESS -> 3 blinks -> OFF
LONG   -> SERVICE_INDICATOR_EVENT_STOPPED      -> LED OFF
```

注意：DOUBLE 直接映射 ONCE_SUCCESS 仅用于 Phase 5 测试。正式业务仍为：

```text
DOUBLE -> APP SAMPLE_ONCE -> acquisition -> UART TX success -> ONCE_SUCCESS
```

人工板测序列：

```text
startup OFF
single -> SINGLE + LED ON
double -> DOUBLE + 3 blinks + OFF
single -> LED ON
long >= 3 s -> LONG + OFF
```

同时确认：

```text
no duplicate events
no HardFault
Button / Indicator / UART / RTT coexist under FreeRTOS
existing UART communication regression PASS
```

Smoke 完成后移除临时 Task / Queue / 测试入口，并恢复正常固件路径。

## 8.9 Phase 5 明确不冻结

永久 RTOS 细节留到 Phase 9：

```text
permanent Button Task ownership
priority
stack size
final Button -> APP IPC
final event buffering policy
```

Phase 5 Smoke Task / Queue 不能成为永久架构既成事实。

## 8.10 完成门槛

```text
Platform Button Host Test PASS
Platform BSP Button Host Test PASS
Button Service Host Test PASS
existing regression PASS
Keil Full Rebuild PASS
FreeRTOS Button + Indicator smoke PASS
Serial Assistant event observation PASS
RTT event / error observation PASS
LED visual mapping PASS
existing UART communication regression PASS
temporary smoke removed PASS
normal-path Keil rebuild PASS
Coding Standard Review PASS
```

---

# 9. Phase 6 — DHT20 Environment Module

目标：通过 Software I2C 实现 DHT20 初始化、通信状态、温度、湿度与错误语义。

预期链：

```text
Environment Service / Device Module
       ↓
DHT20 device capability
       ↓
Software I2C
```

DHT20 不新增 STM32 专用 Impl；专项设计阶段评估统一 `platform_device_t` 模型。

完成门槛：初始化、温湿度读取、校验 / 状态 / 超时、RTT 诊断通过。

---

# 10. Phase 7 — MPU6050 Motion Module

第一阶段仅做：

```text
Accel X / Y / Z
Gyro X / Y / Z
```

不做：

```text
Roll / Pitch / Yaw
DMP
Kalman Filter
Complementary Filter
高频姿态融合
```

专项设计阶段评估统一 `platform_device_t` 模型。

完成门槛：WHO_AM_I、初始化、六轴读取、数据转换规则和异常诊断明确。

---

# 11. Phase 8 — UART Application Communication

必须复用：

```text
UART DMA RX
 -> UART Service
 -> RingBuffer
 -> Communication Task / APP communication
 -> Command Parser
```

命令：

```text
START
STOP
ONCE
STATUS
HELP
```

第一阶段文本上报至少包含 DHT20 temperature / humidity 与 MPU6050 Accel / Gyro XYZ。

完成门槛：命令解析、数据格式、TX Buffer 生命周期和 RTT 诊断符合既有合同。

---

# 12. Phase 9 — RTOS Task / Event Design

目标：在各模块能力已明确后冻结永久 Task、周期调度、事件通知和所有权。

已确认最终方向：

```text
Communication Task
Acquisition Task
Indicator Task
```

永久 Button processing context 仍在本 Phase 决定；Phase 5 临时 Button Smoke Task 只提供运行环境验证证据，不预先决定永久 Button Task。

本阶段需要明确：

```text
Communication processing context
Button periodic processing context
5 s acquisition scheduling
ONCE execution context
Indicator event delivery
Indicator Task priority / stack / queue policy
APP control event delivery
UART async TX completion handling
```

Software I2C 第一阶段继续由单一 Acquisition Task 串行访问；只有真实多访问者时才增加 transaction-level mutex。

---

# 13. Phase 10 — Final APP Integration

APP 是唯一状态源：

```text
STOPPED
RUNNING
```

控制事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

最终行为：

```text
Startup -> STOPPED -> indicator STOPPED -> UART RX active
START -> RUNNING -> indicator RUNNING -> every 5 s acquire + report
STOP -> STOPPED -> stop periodic flow -> indicator STOPPED
ONCE while STOPPED -> acquire once -> UART TX -> success -> ONCE_SUCCESS -> remain STOPPED
```

完成门槛：最终需求所有验收场景 + Keil + Target Board + Serial Assistant + RTT + Button + Error-path + Review + Handoff。

---

# 14. RTT / EasyLogger 横切要求

```text
INFO  -> 初始化、功能启停、关键状态变化
DEBUG -> 采集摘要、完整命令、业务 TX 状态、必要内部状态
WARN  -> 可恢复 GPIO / I2C / Sensor / UART 异常
ERROR -> 初始化失败、关键操作失败
```

禁止逐 UART byte、逐 I2C bit / ACK、逐 LED edge、逐 10 ms Button poll 作为正常日志。

---

# 15. Config 横切要求

最终至少包含：

```text
Acquisition report period = 5000 ms
PROJECT_USER_KEY_ACTIVE_LEVEL = LOW
PROJECT_USER_KEY_PULL = PULL_UP
PROJECT_BUTTON_SAMPLE_PERIOD_MS = 10 ms
PROJECT_BUTTON_DEBOUNCE_MS = 30 ms
PROJECT_BUTTON_DOUBLE_CLICK_MS = 300 ms
PROJECT_BUTTON_LONG_PRESS_MS = 3000 ms
Status LED active level = LOW
LED ONCE blink count = 3
LED blink ON = 100 ms
LED blink OFF = 100 ms
UART command / report limits if needed
```

---

# 16. implementation_plan 使用规则

```text
development_roadmap.md
    = 整个最终功能阶段路线

implementation_plan.md
    = 当前唯一已确认 Phase 的可执行施工计划
```

每个 Phase：

```text
Inspect repository
 -> Discuss scope / design
 -> Freeze design document
 -> Update implementation_plan.md
 -> Codex / Agent implementation
 -> Host / Keil / Board verification
 -> Review
 -> Update handoff
 -> Enter next Phase
```

---

# 17. 当前下一步

当前：

```text
Phase 5 — Button Module
DESIGN BASELINE FROZEN / IMPLEMENTATION PENDING
```

当前 `implementation_plan.md` 仍保存 LED Phase 4 完成计划，不能作为 Button 施工计划。

下一步：

```text
Write Button Phase 5 formal design document
    ↓
Update 00_Doc/04_Agent/implementation_plan.md
    ↓
Codex implementation
```

在新计划冻结前：

- 不直接开始 Button 编码；
- 不提前实现最终 APP Control FSM；
- 不让 Button Service 维护产品 RUNNING / STOPPED 状态；
- 不把按键 active level 泄漏到 Service / APP；
- 不把 Phase 5 临时 Smoke Task 当成永久 Task；
- 不提前开始 DHT20 / MPU6050 Phase。
