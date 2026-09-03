# Final Acquisition System Requirements

> 文档类型：Agent Requirements Baseline  
> 状态：Baseline  
> 版本：V2.1  
> 更新时间：2026-09-03  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 文档定位

本文件是 AI Agent 执行设计、编码和 Review 时使用的长期需求摘要。

最终业务行为的权威需求文件：

```text
00_Doc/00_项目需求/最终功能需求.md
```

本项目原有 UART DMA + RingBuffer + FreeRTOS 主线继续有效。本阶段是在已验证通信链路上增加 GPIO、软件 I2C、DHT20、MPU6050、按键和 APP 控制状态机，形成最终综合闭环。

若本文件与 `最终功能需求.md` 的业务行为冲突，以 `最终功能需求.md` 为准；若业务需求与硬件正确性或已冻结专项设计发生实质冲突，Agent 必须 STOP 并返回设计阶段确认。

---

# 2. 项目最终目标

最终系统闭环：

```text
KEY -> Platform Button -> Button Service ----+
                                              |
                                              v
PC -> UART RX -> DMA -> RingBuffer -> Command Parser
                                              |
                                              v
                                       APP Control FSM
                                              |
                                +-------------+-------------+
                                |                           |
                                v                           v
                              LED                   Sensor Acquisition
                                                            |
                                              +-------------+-------------+
                                              |                           |
                                              v                           v
                                            DHT20                      MPU6050
                                              \                           /
                                               +------- Soft I2C --------+
                                                            |
                                                     Platform GPIO

Sensor Data
    -> APP / Communication
    -> UART Service
    -> Platform UART
    -> UART DMA TX
    -> PC Serial Assistant

APP / Service Runtime State
    -> Service Log
    -> Platform Log
    -> EasyLogger
    -> SEGGER RTT
```

项目继续重点验证：UART DMA、RingBuffer、ISR / Task 协作、FreeRTOS、分层、静态内存 / Buffer 生命周期、RTT 诊断，以及 Requirements -> Design -> Plan -> Implementation -> Test -> Handoff 流程。

---

# 3. 当前目标硬件与软件环境

```text
MCU        : STM32F411CEU6 / Cortex-M4F
Flash      : 512 KB
RAM        : 128 KB
UART       : USART1 / 115200 8N1
Sensor     : DHT20 + MPU6050
Input      : 1 x KEY
Indicator  : 1 x LED
I2C        : Software I2C over GPIO
RTOS       : CMSIS-RTOS2 + FreeRTOS
Log        : EasyLogger + SEGGER RTT
Toolchain  : Keil MDK-ARM + STM32CubeMX
```

DHT20 与 MPU6050 共用一条软件 I2C 总线；最终功能不使用 STM32 硬件 I2C。

SPI / LCD 当前暂停。

User Key 硬件基线已实板确认：

```text
PA0
Input / Pull-Up / no EXTI
released = HIGH
pressed  = LOW
```

---

# 4. 系统状态与唯一状态源

APP 层必须维护唯一采集业务状态：

```text
STOPPED
RUNNING
```

启动初始化完成后：

```text
Acquisition = STOPPED
LED         = OFF
UART RX     = ACTIVE
RTT Log     = ACTIVE
```

按键和 UART 命令只是两个控制入口，不得分别维护两套采集状态。

推荐控制事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

---

# 5. 按键需求与 Phase 5 设计约束

## 5.1 最终业务行为

按键必须支持：

```text
debounce
single click
double click
long press >= 3 s
```

| 当前状态 | 按键事件 | 结果 |
| --- | --- | --- |
| STOPPED | 单击 | START，进入 RUNNING，LED 常亮 |
| STOPPED | 双击 | ONCE，采集并发送一次，保持 STOPPED |
| STOPPED | 长按 >= 3 s | 无额外业务动作 |
| RUNNING | 单击 | 保持 RUNNING |
| RUNNING | 双击 | 不执行额外单次采样 |
| RUNNING | 长按 >= 3 s | STOP，进入 STOPPED，LED 熄灭 |

Button Service 不决定上述状态相关行为，只产生 SINGLE / DOUBLE / LONG；APP 才是最终业务决策者。

## 5.2 Platform Button

必须建立：

```text
Platform GPIO HIGH / LOW
    -> Platform Button PRESSED / RELEASED
    -> Button Service gesture event
```

`platform_button_t` 第一版使用 caller-owned 静态轻量对象，直接拥有一个 `platform_gpio_t`，保存 active level、pull 和 initialized 状态。

不得为 Button 增加：

```text
platform_device_t
runtime registry / manager
dynamic allocation
impl_button passthrough layer
```

User Key 配置：

```text
PROJECT_USER_KEY_ACTIVE_LEVEL = PLATFORM_GPIO_LEVEL_LOW
PROJECT_USER_KEY_PULL         = PLATFORM_GPIO_PULL_UP
```

Service / APP 不得知道 LOW/HIGH 极性。

## 5.3 Button Service 输入输出

Button Service 不持有 Platform Button，不主动读取 GPIO，不直接读取 FreeRTOS tick。

调用方提供：

```text
PRESSED / RELEASED
uint32_t nowMs
```

Service 输出：

```text
NONE
SINGLE
DOUBLE
LONG
```

状态机必须可在 Host 环境直接喂入逻辑状态和时间戳测试。

## 5.4 时间参数

第一版静态参数冻结：

```text
Button sampling period = 10 ms
Button debounce        = 30 ms
Button double window   = 300 ms
Button long press      = 3000 ms
```

必须进入 `00_Config`，禁止魔法数字散落。

消抖判断依据状态持续时间，不得把算法语义固定为“连续 N 次采样”。

## 5.5 SINGLE / DOUBLE / LONG 边界

SINGLE 不能在第一次释放后立即提交。

规则：

```text
first stable RELEASE
 -> start double window
 -> second stable PRESS within <= 300 ms
       -> wait second stable RELEASE
       -> DOUBLE
 -> no second stable PRESS before timeout
       -> SINGLE
```

双击窗口判断第二次稳定 PRESS 是否在窗口内开始，不要求第二次 RELEASE 也落入窗口。

LONG：

```text
stable PRESS duration >= 3000 ms
 -> emit LONG immediately once
 -> suppress click candidate until stable RELEASE
```

必须满足：

```text
2999 ms -> no LONG
3000 ms -> LONG
one hold -> only one LONG
LONG release -> no SINGLE
first click + second long hold -> LONG only, no DOUBLE
```

时间计算必须支持 `uint32_t` 单调毫秒计数自然回绕。

## 5.6 Phase 5 Host Test

至少验证：

```text
Platform Button active-low / active-high translation
BSP User Key active-low + pull-up composition
press / release bounce suppression
single -> only SINGLE
double -> only DOUBLE
long -> only LONG
2999 / 3000 ms boundary
300 ms double boundary
expired window behavior
LONG release no SINGLE
second press long conflict
initial pressed behavior
uint32_t wraparound
irregular process interval
```

Host Test 通过注入 `nowMs` 推进时间，不真实等待 3 s。

## 5.7 Phase 5 FreeRTOS Target Smoke

目标板验证必须运行在 FreeRTOS Scheduler 已启动的真实 Task Context。

允许临时测试结构：

```text
Button Smoke Task
    -> Platform Button
    -> platform_time_get_ms()
    -> Button Service
    -> temporary Queue

Indicator Smoke Task
    -> Indicator Service
    -> Platform LED
```

Button Smoke Task sampling 约 10 ms，使用 `platform_time_delay_ms()`。

目标板同时观察：

```text
USART1 Serial Assistant
RTT / EasyLogger
LED visual behavior
```

Phase 5 Smoke-only LED 映射：

```text
SINGLE -> SERVICE_INDICATOR_EVENT_RUNNING      -> LED ON
DOUBLE -> SERVICE_INDICATOR_EVENT_ONCE_SUCCESS -> blink 3 times -> OFF
LONG   -> SERVICE_INDICATOR_EVENT_STOPPED      -> LED OFF
```

该映射仅用于验证按键事件与现有 LED / Indicator 模块在 FreeRTOS 下协作。正式系统不得把 `DOUBLE` 直接解释为 `ONCE_SUCCESS`，正式链必须是：

```text
DOUBLE -> APP SAMPLE_ONCE -> sensor acquisition -> UART TX success -> ONCE_SUCCESS
```

Smoke 必须同时确认：无重复 gesture、无 HardFault、Button / Indicator / UART / RTT 可共存、既有 UART 通信无回归。

Smoke 后必须移除临时 Task / Queue / 测试入口。

永久 Button Task、priority、stack、Button -> APP IPC 和事件缓存策略留到 Phase 9。

---

# 6. UART 命令需求

PC 串口助手使用文本命令：

```text
START\r\n
STOP\r\n
ONCE\r\n
STATUS\r\n
HELP\r\n
```

| 命令 | STOPPED | RUNNING |
| --- | --- | --- |
| START | 进入 RUNNING | 保持 RUNNING，返回 already running |
| STOP | 保持 STOPPED，返回 already stopped | 进入 STOPPED |
| ONCE | 单次采集并发送，保持 STOPPED | 不额外采样，返回 already running |
| STATUS | 返回 STOPPED | 返回 RUNNING |
| HELP | 返回命令列表 | 返回命令列表 |

必须复用：

```text
USART1
 -> DMA Circular + IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform UART Event
 -> UART Service
 -> SPSC RingBuffer
 -> APP Communication Task
 -> Command Parser
 -> APP Control Event
```

UART Service 不得直接控制 LED、DHT20、MPU6050 或 APP 状态。

---

# 7. 周期采集与发送

第一阶段统一业务周期：

```text
Acquisition / Report Period = 5000 ms
```

RUNNING 每 5 s：DHT20 -> MPU6050 -> 组织数据 -> UART TX -> RTT DEBUG 摘要。

STOPPED 不执行周期采集 / 发送。

---

# 8. DHT20 第一阶段需求

必须实现：初始化、通信状态、温度、相对湿度、数据有效性和明确错误返回。

DHT20 不得直接依赖 STM32 HAL GPIO API。

---

# 9. MPU6050 第一阶段需求

必须实现：初始化、WHO_AM_I、Accel XYZ、Gyro XYZ、基础转换和错误返回。

当前不实现：

```text
Roll
Pitch
Yaw
Complementary Filter
Kalman Filter
DMP
高频姿态融合
```

---

# 10. 软件 I2C 需求

Software I2C 必须建立在 Platform GPIO 上，不得直接调用 `HAL_GPIO_xxx`。

至少支持 START / STOP / ACK / NACK / byte / multi-byte / write-read；bit timing 不得使用 RTOS tick delay。

第一阶段单一采集执行上下文串行访问 DHT20 / MPU6050；只有真实多访问者时才增加 transaction-level synchronization。

---

# 11. LED 产品语义

```text
STOPPED               -> OFF
RUNNING               -> ON
RUNNING 周期发送       -> 保持 ON
ONCE TX SUCCESS        -> 闪 3 次 -> OFF
ONCE sample/TX failure -> 保持 OFF
```

只有成功完成业务发送后才提交 `ONCE_SUCCESS`。LED active level 属于 Board / BSP，闪烁不得在 ISR / HAL Callback 中执行。

---

# 12. UART 数据输出

必须复用：

```text
APP / Communication
 -> UART Service
 -> Platform UART
 -> STM32 UART Impl
 -> HAL / DMA
 -> USART1
 -> PC Serial Assistant
```

第一阶段使用文本格式，不要求自定义二进制应用协议。

异步 TX Buffer 在 complete / error / cancel 前不得修改或失效。

---

# 13. RTT / EasyLogger 需求

正式链：

```text
APP / Service
 -> service_log
 -> Platform Log
 -> EasyLogger Adapter
 -> EasyLogger / SEGGER RTT
```

```text
INFO  -> 初始化、START / STOP / ONCE、状态变化
DEBUG -> 5 s 摘要、完整 UART 命令、业务 TX 状态
WARN  -> 可恢复 I2C / Sensor / UART / GPIO 异常
ERROR -> 初始化或关键操作失败
```

正常运行禁止逐 UART byte、逐 I2C bit / ACK、逐 DMA 步骤、逐 Button 10 ms polling 刷日志。

ISR / HAL Callback 禁止大量格式化日志。

---

# 14. 推荐 RTOS 执行模型

永久任务数量由真实并发职责决定，不由设备数量决定。

已确认：

```text
Communication Task
Acquisition Task
Indicator Task
```

Indicator Task 已因阻塞闪烁隔离职责而冻结为独立执行上下文。

永久 Button processing context 尚未冻结；Phase 5 使用临时 Button Smoke Task + Indicator Smoke Task 只为了在真实 FreeRTOS 环境验证，不代表永久架构。

不得为 DHT20 / MPU6050 机械创建独立 Task。

---

# 15. 分层与依赖要求

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

职责：

```text
APP
- 产品状态机 / 控制决策 / 周期业务编排

Service
- UART / Log / Indicator / Button gesture recognition / data processing

Platform
- UART / GPIO / OS / Log / Software I2C / LED / Button public capability

Impl
- STM32 / FreeRTOS / Middleware concrete implementation

Vendor
- STM32 HAL / CMSIS / FreeRTOS / EasyLogger / RTT
```

APP / Service 不得直接依赖 HAL、CubeMX Handle 或 Impl 私有接口。

---

# 16. 既有 UART / RingBuffer 基线继续冻结

```text
DMA Circular + IDLE / HT / TC RX
Platform UART Event
UART Service
SPSC RingBuffer
APP Communication Task
caller-owned / static storage
RX ISR / Task boundary
UART async TX buffer lifetime contract
RingBuffer overflow detection
Service Log -> RTT
```

RingBuffer 继续 SPSC、无 malloc/free、当前链路无普通 mutex、无 silent overwrite。

---

# 17. ISR、并发与内存要求

ISR / HAL Callback 允许 capture / necessary copy / lightweight state / ISR-safe notify / quick exit。

禁止 blocking、ordinary mutex、malloc/free、完整协议、Button gesture FSM、Sensor 业务、LED delay、大量格式化日志、非 ISR-safe API。

核心链路优先 static / caller-owned storage。

---

# 18. 当前范围冻结

必须完成：

```text
GPIO STM32 Impl + board verification
LED
Platform Button / BSP Button
Button debounce / single / double / long
Software I2C
DHT20
MPU6050 basic 6-axis
APP Control FSM
UART START / STOP / ONCE / STATUS / HELP
5 s acquisition + UART report
ONCE success LED feedback
RTT diagnostic coverage
final integrated board test
```

当前不做：Roll / Pitch / Yaw、DMP / filters、SPI / LCD / GUI、W25Q64 / AT24C02、Bluetooth、复杂二进制协议、Phase 5 Button EXTI、无需求驱动框架扩展。

---

# 19. 配置要求

产品静态配置集中到 `00_Config`。

至少包括：

```text
Acquisition period = 5000 ms
PROJECT_USER_KEY_ACTIVE_LEVEL = PLATFORM_GPIO_LEVEL_LOW
PROJECT_USER_KEY_PULL = PLATFORM_GPIO_PULL_UP
PROJECT_BUTTON_SAMPLE_PERIOD_MS = 10 ms
PROJECT_BUTTON_DEBOUNCE_MS = 30 ms
PROJECT_BUTTON_DOUBLE_CLICK_MS = 300 ms
PROJECT_BUTTON_LONG_PRESS_MS = 3000 ms
PROJECT_STATUS_LED_ACTIVE_LEVEL = PLATFORM_GPIO_LEVEL_LOW
LED success blink count = 3
LED blink ON = 100 ms
LED blink OFF = 100 ms
Sensor / command buffer sizes if needed
```

---

# 20. 最终验收核心场景

必须至少验证：

1. 上电后 STOPPED、LED 灭、UART RX / RTT 正常；
2. STOPPED 单击 -> RUNNING、LED 常亮；
3. RUNNING 每 5 s 输出一组 DHT20 + MPU6050 数据；
4. RUNNING 长按 >= 3 s -> STOPPED、LED 灭、停止周期上报；
5. STOPPED 双击 -> 单次采集和发送 -> TX 成功后 LED 闪 3 次 -> STOPPED；
6. UART `START` / `STOP` 与按键控制使用同一真实状态；
7. UART `ONCE` 在 STOPPED 正确执行；
8. `STATUS` / `HELP` 返回明确文本；
9. 初始化、关键采集和 UART 收发在 RTT 中可观察；
10. I2C / Sensor / UART / RingBuffer 异常不静默失败。

---

# 21. 核心原则

```text
UART / RingBuffer 是已验证基础通信能力。
GPIO 是底层通用资源能力。
Platform Button 将电平转换为 PRESSED / RELEASED。
Button Service 将稳定输入转换为 SINGLE / DOUBLE / LONG。
APP Control FSM 是唯一业务状态源。
UART 与 Button 只是控制入口。
UART 是业务数据输出终端。
RTT 是内部运行状态与诊断终端。
```

第一阶段目标是完成稳定、清晰、可验收的基础闭环，而不是继续扩大功能数量。
