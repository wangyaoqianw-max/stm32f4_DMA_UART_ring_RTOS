# 工程长期记忆与交接说明

更新时间：2026-09-03

> 本文件是 AI Agent 与人工开发者恢复工程上下文时的长期入口。
> 只保存长期目标、稳定架构合同、已验证能力、当前阶段、技术债和下一步。
> 详细业务需求以 `00_Doc/00_项目需求/最终功能需求.md` 为准。
> 整体阶段拆分以 `00_Doc/04_Agent/development_roadmap.md` 为准。
> 当前具体施工步骤只以 `00_Doc/04_Agent/implementation_plan.md` 为准。

---

# 1. 项目定位

工程根目录：

```text
RTT_elog_DMA_UART_ring_project/
```

目标环境：

```text
MCU        : STM32F411CEU6 / Cortex-M4F
Flash      : 512 KB
RAM        : 128 KB
UART       : USART1 / 115200 8N1
RTOS       : CMSIS-RTOS2 + FreeRTOS
Debug Log  : EasyLogger + SEGGER RTT
Sensors    : DHT20 + MPU6050
I2C        : Software I2C over GPIO
Input      : 1 x KEY
Indicator  : 1 x LED
```

当前最终目标：

> 在已验证 UART 通信链路和五层架构基础上，加入 GPIO、Software I2C、DHT20、MPU6050、按键控制、LED 状态反馈、UART 文本命令和 APP Control FSM，形成完整数据采集系统。

---

# 2. 稳定总体架构

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

CubeMX 生成文件只作为初始化、IRQ / HAL Callback、Scheduler 和薄适配入口，长期业务逻辑不得堆积其中。

---

# 3. 已验证基线

已完成 / 已验证：

```text
Platform BSP UART Binding Phase 1   COMPLETED
Communication UART Binding          VERIFIED
UART Service Phase 1                COMPLETED
Base RX Vertical Slice              VERIFIED
APP Phase 1                         COMPLETED / VERIFIED
Platform OS                         COMPLETED / VERIFIED
Service Log Phase 1                 COMPLETED / HOST + RTT VERIFIED
Platform Log Naming Refactor        COMPLETED / KEIL + RTT VERIFIED
RingBuffer SPSC Review              REVIEWED
Platform GPIO Phase 1               COMPLETED / HOST VERIFIED
GPIO Header Isolation               PASS
GPIO Platform Dependency Boundary   PASS
GPIO Coding Standard Review         PASS
GPIO STM32 Impl Phase 1             COMPLETED / HOST + KEIL VERIFIED
Board Resource Freeze               PASS
CubeMX GPIO Configuration           PASS
Board / GPIO Context Binding        COMPLETED / HOST VERIFIED
Target Board GPIO Verification      PASS
Software I2C Phase 3                COMPLETED / HOST + KEIL + DHT20 TARGET SMOKE VERIFIED
LED Module Phase 4                  COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

真实板测已确认：

```text
PC13 Status LED                       PASS
PA0 User Key                          PASS
PB6 Open-Drain Pull-Low / Release     PASS
PB7 Open-Drain Pull-Low / Release     PASS
PB7 Physical Readback                 PASS
LED OFF / ON / 3 blink / final OFF    PASS
Indicator RTT stage sequence          PASS
Existing UART communication regression PASS
```

PA0 User Key 电气行为已经由目标板验证，不再是推断：

```text
PA0
Input / Pull-Up / no EXTI
released = HIGH
pressed  = LOW
```

---

# 4. UART / RingBuffer 稳定合同

当前 RX 链：

```text
USART1
  ↓
DMA Circular + IDLE / HT / TC
  ↓
STM32 UART Impl
  ↓
Platform UART RX_DATA / ERROR / CANCELED
  ↓
UART Service
  ↓
SPSC RingBuffer
  ↓
Platform Notify From ISR
  ↓
APP Communication Task
```

RingBuffer 冻结：

```text
SPSC byte stream
caller-owned storage
usable capacity = storageSize - 1
no malloc/free
no UART / DMA / HAL knowledge
no silent overwrite
```

当前：

```text
Producer = UART Service RX callback
Consumer = APP Communication Task
```

不得给当前 SPSC 链路加入普通 Mutex。

新增 `START / STOP / ONCE / STATUS / HELP` 必须复用现有 UART Service / RingBuffer RX 链。

---

# 5. ISR / Task 合同

ISR / HAL Callback 只允许：

```text
capture
copy necessary bytes
update lightweight state
ISR-safe notify
quick exit
```

禁止：

```text
blocking
ordinary mutex
malloc/free
完整协议解析
传感器业务
Software I2C transaction
LED 闪烁延时
Button debounce / gesture FSM
大量格式化日志
非 ISR-safe RTOS API
```

Software I2C 为 Task Context / synchronous / caller serialized。

LED 三闪最终在独立 Indicator Task Context 执行，不在 ISR / HAL Callback 执行。

Button 第一阶段采用周期轮询 + 时间状态机，不启用 EXTI；未来若因低功耗需要 EXTI，也只能用于 wake / notify，消抖和手势识别仍在 Task Context 完成。

---

# 6. RTT / EasyLogger 基线

正式链路：

```text
APP / Service
    ↓
service_log
    ↓
Platform Log
    ↓
EasyLogger Adapter
    ↓
EasyLogger / RTT
```

日志原则：

```text
INFO  -> 初始化、START / STOP / ONCE、关键状态切换
DEBUG -> 5 s 采集摘要、完整 UART 命令、业务 TX 状态
WARN  -> 可恢复 GPIO / I2C / Sensor / UART 异常
ERROR -> 初始化失败、关键操作失败
```

禁止正常运行时：

```text
逐 UART byte 打日志
逐 I2C bit / byte / ACK 打日志
逐 LED on/off 边沿打日志
逐 GPIO read/write 打日志
逐 10 ms Button polling 打日志
ISR 中大量格式化日志
```

---

# 7. Platform GPIO / Board 基线

Platform GPIO 公共 API：

```text
platform_gpio_init
platform_gpio_configure
platform_gpio_write
platform_gpio_read
platform_gpio_deinit
```

Generic STM32 GPIO Impl：

```text
Caller-owned Context
Context = GPIO_TypeDef *port + uint16_t pin
One Platform GPIO = one physical pin
No Context Pool
No Registry
No dynamic allocation
```

当前板级资源：

```text
PC13 -> Status LED
PA0  -> User Key
PB6  -> Software I2C SCL
PB7  -> Software I2C SDA
PA9  -> USART1_TX
PA10 -> USART1_RX
```

Board BSP 已提供：

```text
platform_bsp_gpio_construct_status_led()
platform_bsp_gpio_construct_user_key()
platform_bsp_gpio_construct_soft_i2c_scl()
platform_bsp_gpio_construct_soft_i2c_sda()
```

BSP GPIO constructor 只 construct / bind，不执行 GPIO hardware configure。

---

# 8. Software I2C 已冻结 / 已完成摘要

定位：

```text
Platform MCU basic communication capability
Master only
7-bit address
Synchronous
No internal mutex
No dynamic allocation
```

公共 API：

```text
platform_i2c_init
platform_i2c_write
platform_i2c_read
platform_i2c_write_read
platform_i2c_deinit
```

SCL / SDA：

```text
PB6 / PB7
Open-Drain Output
No Pull
external pull-up
HIGH = release
LOW = actively pull low
physical readback
```

时序：

```text
platform_delay_us()
Cortex-M4 DWT CYCCNT
nominal ~100 kHz
Repeated START supported
last RX byte NACK
```

Phase 3 目标板证据（2026-09-02）：

```text
Keil Full Rebuild (with smoke)         0 errors
Serial Assistant                       I2C_SMOKE,TXRX,PASS,status=0x18
RTT / EasyLogger                       i2c smoke txrx pass, status=0x18
Logic Analyzer                         START -> 0x38(W) -> 0xAC 0x33 0x00 -> STOP
Decoder ACK                            address and all three command bytes ACK
Temporary verification path           removed
Keil Full Rebuild (normal path)        0 errors
```

---

# 9. LED Phase 4 完成记录

专项设计：

```text
00_Doc/02_架构设计/LED_Phase1设计.md
```

最终状态：

```text
Phase 4 — COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

实施与验证证据（2026-09-03）：

```text
Platform LED / BSP LED / Indicator Service       IMPLEMENTED
Final Host regression (5 suites, -Werror)         PASS
Normal-path Keil Full Rebuild                     PASS: 0 errors, 20 pre-existing warnings
Phase 4 source warning scan                       PASS
Target LED visual verification                    PASS — OFF / ON / 3 blinks / final OFF
Target RTT stage observation                      PASS
Target + PC Serial Assistant communication check  PASS
Temporary FreeRTOS indicator smoke path           REMOVED
Coding Standard Review                            PASS
```

---

# 10. LED Phase 4 冻结设计

最终职责链：

```text
APP / Control           Phase 10
    ↓ semantic event
Indicator Task          Phase 9
    ↓
Indicator Service       Phase 4
    ↓
Platform LED            Phase 4
    ↓
Platform GPIO           existing
    ↓
STM32 GPIO Impl         existing
```

Phase 4 不实现正式 APP Control FSM，不永久创建 Indicator Task。

Platform LED：

```text
lightweight LED actuator abstraction
caller-owned static object
no malloc/free
no platform_device_t
no registry / manager
no new impl_led layer
```

公共能力：

```text
platform_led_init
platform_led_on
platform_led_off
platform_led_toggle
platform_led_deinit
```

Status LED 静态有效电平：

```text
PROJECT_STATUS_LED_ACTIVE_LEVEL = PLATFORM_GPIO_LEVEL_LOW
```

Indicator Service：

```text
SERVICE_INDICATOR_EVENT_STOPPED      -> OFF
SERVICE_INDICATOR_EVENT_RUNNING      -> ON
SERVICE_INDICATOR_EVENT_ONCE_SUCCESS -> blink 3 times -> OFF
```

闪烁配置：

```text
blink count  = 3
blink ON ms  = 100
blink OFF ms = 100
```

延时统一使用 `platform_time_delay_ms()`。

---

# 11. Indicator Task 冻结方向

最终系统明确采用独立：

```text
Indicator Task
```

职责：

```text
consume indicator events
serialize LED behavior
execute blink timing
isolate LED delay from APP / UART / acquisition contexts
```

以下永久运行细节仍留到 Phase 9：

```text
Task creation
priority
stack size
queue / notification mechanism
event buffering / overwrite policy
```

Phase 5 目标板验证允许创建临时 Indicator Smoke Task；该测试 Task 不等于 Phase 9 的永久 Task 配置。

---

# 12. Device Model 当前决策

不要求所有硬件相关模块机械使用统一 Device 类型。

当前分类：

```text
GPIO              -> lightweight resource, no platform_device_t
LED               -> lightweight actuator, no platform_device_t
Button            -> lightweight input device, no platform_device_t
Software I2C      -> communication capability, no platform_device_t
UART              -> real device object, platform_device_t already used
DHT20             -> future design should evaluate platform_device_t
MPU6050           -> future design should evaluate platform_device_t
```

DHT20 / MPU6050 具有明确设备身份、配置、生命周期和数据语义，后续专项设计优先参考 UART 的 Device 模型，而不是照搬 LED / Button 轻量对象。

---

# 13. Button Phase 5 冻结设计基线

当前阶段：

```text
Phase 5 — Button Module
DESIGN BASELINE FROZEN / IMPLEMENTATION PENDING
```

设计讨论已经收束；尚未生成新的 Phase 5 `implementation_plan.md`，不得直接开始编码。

## 13.1 Platform Button / BSP Button

冻结分层：

```text
Button Service
    ↓ logical PRESSED / RELEASED + nowMs
Platform Button
    ↓
Platform GPIO
    ↓
STM32 GPIO Impl
```

`platform_button_t` 第一版：

```text
lightweight input device
caller-owned static object
owns one platform_gpio_t
stores activeLevel + pull + initialized state
no malloc/free
no platform_device_t
no registry / manager
no impl_button layer
```

建议公共能力：

```text
platform_button_init
platform_button_read -> PLATFORM_BUTTON_STATE_PRESSED / RELEASED
platform_button_deinit
```

BSP Button 负责将 User Key 的板级属性与 GPIO Binding 装配起来：

```text
platform_bsp_button_construct_user_key()
    -> platform_bsp_gpio_construct_user_key()
    -> active level
    -> pull configuration
```

User Key 冻结静态配置：

```text
PROJECT_USER_KEY_ACTIVE_LEVEL = PLATFORM_GPIO_LEVEL_LOW
PROJECT_USER_KEY_PULL         = PLATFORM_GPIO_PULL_UP
```

具体 PA0 / GPIOA / GPIO_PIN_0 不进入 Service / APP。

Platform Button 负责 HIGH / LOW 到 PRESSED / RELEASED 的转换，不识别 SINGLE / DOUBLE / LONG。

## 13.2 Button Service

Button Service 第一版是纯时间驱动状态机，不持有 `platform_button_t *`，不主动读取 GPIO，也不主动调用 RTOS tick。

输入合同：

```text
platform_button_state_t buttonState
uint32_t nowMs
```

输出事件：

```text
SERVICE_BUTTON_EVENT_NONE
SERVICE_BUTTON_EVENT_SINGLE
SERVICE_BUTTON_EVENT_DOUBLE
SERVICE_BUTTON_EVENT_LONG
```

推荐 process 形态：

```text
service_button_process(service, buttonState, nowMs, &event)
```

Service Context 只保存输入历史、稳定状态、时间戳和 gesture state，不保存 GPIO / Task / Queue / FreeRTOS handle。

内部逻辑分为两个阶段：

```text
raw state
 -> time-based debounce
 -> stable PRESS / RELEASE edge
 -> gesture FSM
 -> SINGLE / DOUBLE / LONG
```

## 13.3 时间参数与边界

第一版冻结：

```text
PROJECT_BUTTON_SAMPLE_PERIOD_MS = 10 ms
PROJECT_BUTTON_DEBOUNCE_MS      = 30 ms
PROJECT_BUTTON_DOUBLE_CLICK_MS  = 300 ms
PROJECT_BUTTON_LONG_PRESS_MS    = 3000 ms
```

消抖：

```text
raw state 改变 -> 记录 rawChangedMs
raw state 连续保持 >= 30 ms -> 更新 stable state
```

不得用“固定 N 次采样”等价替代时间阈值，避免算法语义被 Task 周期绑定。

单击 / 双击：

```text
first stable RELEASE
 -> WAIT_SECOND
 -> second stable PRESS within <= 300 ms -> SECOND_PRESS
 -> second stable RELEASE -> DOUBLE
 -> no second PRESS and window expires -> SINGLE
```

双击窗口从第一次稳定 RELEASE 开始；判断第二次稳定 PRESS 是否在窗口内，不要求第二次 RELEASE 也在窗口内。

长按：

```text
stable PRESS
 -> hold >= 3000 ms
 -> emit LONG immediately once
 -> suppress click candidate until RELEASE
```

规则：

```text
2999 ms -> not LONG
3000 ms -> LONG
LONG only once per hold
LONG release -> no SINGLE
first click + second press held >= 3000 ms -> LONG only, no DOUBLE
```

所有超时判断使用可自然处理 `uint32_t` 回绕的 elapsed-time 差值形式。

## 13.4 Host Test 冻结方向

至少新增 / 覆盖：

```text
Tests/platform_button/
Tests/platform_bsp_button/
Tests/service_button/
```

Platform Button：

```text
active-low and active-high translation
INPUT + pull configuration
init / read / deinit lifecycle
error propagation
```

BSP Button：

```text
User Key active level = LOW
User Key pull = UP
correct GPIO binding
```

Button Service：

```text
initial released / initial pressed
press / release bounce
single only
DOUBLE only, no preceding SINGLE
2999 / 3000 ms long boundary
very long hold emits LONG once
LONG release does not emit SINGLE
second press long -> LONG only
double-click 300 ms boundary
double-click expired path
uint32_t time wraparound
irregular process intervals
```

Host Test 使用逻辑时间输入，不真实 sleep 3 s。

## 13.5 FreeRTOS 目标板 Smoke

Phase 5 目标板验证必须运行在 FreeRTOS Scheduler 已启动的真实 Task Context 中，不采用 scheduler 前阻塞式 Harness。

测试结构冻结为：

```text
Temporary Button Smoke Task
    -> platform_button_read()
    -> platform_time_get_ms()
    -> service_button_process()
    -> Button event
    -> temporary Queue

Temporary Indicator Smoke Task
    -> consume mapped indicator event
    -> service_indicator_handle_event()
    -> Platform LED
```

Button Smoke Task 约每 10 ms 调用一次，延时使用 `platform_time_delay_ms()`；不得直接调用 `HAL_Delay()` / `osDelay()` / `vTaskDelay()` 绕过 Platform OS。

由于 `ONCE_SUCCESS` 三闪会在调用 Task 中阻塞约 600 ms，Button 与 Indicator 使用两个临时 Task，使 LED 闪烁不打断 Button 10 ms sampling。

目标板观察同时使用：

```text
USART1 Serial Assistant
RTT / EasyLogger
LED visual behavior
```

Smoke-only 映射：

```text
Button SINGLE -> SERVICE_INDICATOR_EVENT_RUNNING      -> LED ON
Button DOUBLE -> SERVICE_INDICATOR_EVENT_ONCE_SUCCESS -> blink 3 times -> OFF
Button LONG   -> SERVICE_INDICATOR_EVENT_STOPPED      -> LED OFF
```

注意：`DOUBLE -> ONCE_SUCCESS` 只用于 Phase 5 Smoke 验证 Button + Indicator 集成，不代表正式业务链已经变成直接成功反馈。正式系统仍是：

```text
DOUBLE -> APP SAMPLE_ONCE -> sensor acquisition -> UART TX success -> ONCE_SUCCESS
```

推荐人工验证序列：

```text
startup -> LED OFF
single  -> SINGLE log + LED ON
double  -> DOUBLE log + 3 blinks + final OFF
single  -> LED ON
long >= 3 s -> LONG log + LED OFF
```

串口助手输出结构化事件，RTT 输出初始化 / stable edge / gesture event / error 等诊断；禁止每 10 ms polling 刷日志。

还必须确认：

```text
Button + Indicator + UART + RTT can coexist under FreeRTOS
no HardFault
no obvious scheduling stall
no duplicate gesture event
existing UART communication regression PASS
```

Smoke 完成后必须移除临时 Task / Queue / 测试入口并恢复正常固件路径。

## 13.6 Phase 5 不冻结的内容

以下仍留到 Phase 9：

```text
permanent Button Task ownership
permanent Button Task priority
permanent Button Task stack
final Button -> APP IPC mechanism
final event buffering / queue policy
```

Phase 5 的临时 Button / Indicator Smoke Task 与 Queue 仅用于目标板验证，不得反向成为永久 RTOS 架构合同。

---

# 14. 最终按键业务映射

正式系统业务映射继续冻结：

```text
Button single -> START
Button double -> SAMPLE_ONCE
Button long   -> STOP
```

APP Control FSM 是唯一 `STOPPED / RUNNING` 状态源；Button Service 只识别 gesture，不判断当前 APP 状态，不直接控制 LED / Sensor。

---

# 15. 当前执行计划状态

当前文件：

```text
00_Doc/04_Agent/implementation_plan.md
```

目前保存：

```text
LED Phase 4 Implementation Plan — COMPLETED
```

不要直接把该计划用于 Button 编码。

Phase 5 下一步：

```text
Button design discussion               FROZEN
    ↓
Write formal Button Phase 5 design doc NEXT
    ↓
Replace / update implementation_plan.md with Phase 5 plan
    ↓
Codex implementation
    ↓
Host / Keil / FreeRTOS target smoke
    ↓
Review + handoff update
```

---

# 16. 当前后续路线

```text
Phase 3  Software I2C                   COMPLETED
Phase 4  LED Module                     COMPLETED
Phase 5  Button Module                  CURRENT / DESIGN FROZEN / IMPLEMENTATION PENDING
Phase 6  DHT20 Environment Module
Phase 7  MPU6050 Motion Module
Phase 8  UART Application Communication
Phase 9  RTOS Task / Event Design
Phase 10 Final APP Integration
Final Integrated Board Test
```

---

# 17. 当前暂缓范围

```text
SPI / LCD / GUI
W25Q64
AT24C02
Bluetooth
Roll / Pitch / Yaw
DMP
Kalman / Complementary Filter
复杂二进制 UART Protocol
无需求驱动的框架扩展
```

不得主动加入当前开发主线。
