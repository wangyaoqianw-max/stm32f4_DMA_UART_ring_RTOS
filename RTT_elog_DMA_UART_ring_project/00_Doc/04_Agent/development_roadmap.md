# Final Acquisition System Development Roadmap

> 文档类型：Development Phase Roadmap  
> 状态：Baseline  
> 日期：2026-09-03  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 文档目的

本文档只回答：

```text
后续分成哪些 Phase？
各 Phase 依赖什么？
各 Phase 做到哪里停止？
什么条件下进入下一 Phase？
```

详细业务需求：

```text
00_Doc/00_项目需求/最终功能需求.md
```

长期架构：

```text
00_Doc/04_Agent/architecture.md
```

当前唯一施工计划：

```text
00_Doc/04_Agent/implementation_plan.md
```

每个 Phase 都必须先专项设计，再生成 / 替换当前 implementation plan；不得把多个 Phase 一次性塞进同一实现计划。

---

# 2. 已验证基础能力

```text
UART Platform / STM32 Impl                    VERIFIED
UART DMA RX / TX                              VERIFIED
UART Service                                  VERIFIED
SPSC RingBuffer                               VERIFIED
APP Communication Phase 1                     VERIFIED
Platform OS                                   VERIFIED
Service Log + EasyLogger + RTT                 VERIFIED
Platform GPIO + STM32 Impl + Board Binding     VERIFIED
Software I2C                                  VERIFIED
LED / Indicator Module                        VERIFIED
```

最终闭环仍缺：

```text
Button implementation
DHT20 environment data
MPU6050 basic motion data
UART application commands / report
Permanent RTOS task / event organization
Final APP Control FSM
Integrated target-board verification
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
Phase 5  Button Module                           CURRENT / PLAN READY
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

目标：为 Platform GPIO 提供 STM32F411 + HAL 具体实现。

范围：

```text
configure
write
read
deinit
```

明确不做：LED / KEY 产品语义、Debounce、Soft I2C、EXTI。

状态：

```text
COMPLETED / HOST + KEIL VERIFIED
```

---

# 5. Phase 2 — Board Resource + CubeMX Configuration

冻结资源：

```text
PC13 -> Status LED
PA0  -> User Key
PB6  -> Software I2C SCL
PB7  -> Software I2C SDA
PA9 / PA10 -> USART1
```

User Key 实板基线：

```text
Input / Pull-Up / no EXTI
released HIGH
pressed LOW
```

状态：

```text
COMPLETED / TARGET BOARD VERIFIED
```

---

# 6. Phase 3 — Software I2C

冻结：

```text
Platform communication capability
Master-only
7-bit
synchronous
START / STOP / ACK / NACK
multi-byte read / write
write-read / repeated START
Open-Drain + external pull-up
platform_delay_us()
no internal mutex
```

目标板使用 DHT20 + logic analyzer 验证。

状态：

```text
COMPLETED / HOST + KEIL + TARGET VERIFIED
```

---

# 7. Phase 4 — LED Module

专项设计：

```text
00_Doc/02_架构设计/LED_Phase1设计.md
```

冻结链：

```text
Indicator Service
 -> Platform LED
 -> Platform GPIO
 -> STM32 GPIO Impl
```

Platform LED：轻量、caller-owned、无 `platform_device_t`、无 `impl_led`。

Indicator Service：

```text
STOPPED      -> OFF
RUNNING      -> ON
ONCE_SUCCESS -> 3 blinks -> OFF
```

最终独立 Indicator Task 的方向已冻结；永久 priority / stack / event policy 留到 Phase 9。

状态：

```text
COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

---

# 8. Phase 5 — Button Module

专项设计：

```text
00_Doc/02_架构设计/Button_Phase1设计.md
```

当前执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
Button Phase 5 Implementation Plan
Status: READY FOR IMPLEMENTATION
```

当前状态：

```text
REQUIREMENTS ALIGNED
DESIGN FROZEN
FORMAL DESIGN WRITTEN
IMPLEMENTATION PLAN READY
IMPLEMENTATION PENDING
```

## 8.1 正式能力链

```text
PA0 HIGH / LOW
 -> Platform GPIO
 -> Platform Button PRESSED / RELEASED
 -> Button Service SINGLE / DOUBLE / LONG
 -> Future APP START / SAMPLE_ONCE / STOP
```

Phase 5 不实现最终 APP Control FSM。

## 8.2 Platform Button

第一版：

```text
caller-owned lightweight object
owns one platform_gpio_t
activeLevel + pull + lifecycle
no malloc/free
no platform_device_t
no registry / manager
no impl_button
```

User Key Config：

```text
PROJECT_USER_KEY_ACTIVE_LEVEL = LOW
PROJECT_USER_KEY_PULL = PULL_UP
```

## 8.3 Button Service

输入：

```text
PRESSED / RELEASED
caller-provided uint32_t nowMs
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
 -> stable edge
 -> gesture FSM
```

冻结时间：

```text
sample period = 10 ms
debounce      = 30 ms
double window = 300 ms
long press    = 3000 ms
```

关键规则：

```text
SINGLE waits for double window
second stable PRESS <= 300 ms -> DOUBLE candidate
DOUBLE never emits preceding SINGLE
LONG >= 3000 ms -> exactly once
LONG release -> no SINGLE
second press can become LONG -> LONG only
uint32 elapsed arithmetic supports wraparound
```

## 8.4 Host Test

必须覆盖：

```text
Platform Button active-low / active-high
BSP User Key composition
press / release bounce
single only
double only
2999 / 3000 ms long boundary
very long hold one LONG
LONG release no SINGLE
second-press long conflict
300 ms double boundary
expired press preserved as new gesture
initial pressed
irregular intervals
uint32 wraparound
```

Host Service Test 不真实等待 3 s。

## 8.5 FreeRTOS Target Smoke

运行在 Scheduler 已启动后的真实 Task Context：

```text
Button Smoke Task
 -> Platform Button
 -> Platform Time
 -> Button Service
 -> Serial Assistant + RTT
 -> Platform Queue

Indicator Smoke Task
 -> Platform Queue
 -> Indicator Service
 -> Platform LED
```

Smoke-only 映射：

```text
SINGLE -> RUNNING      -> LED ON
DOUBLE -> ONCE_SUCCESS -> 3 blinks -> OFF
LONG   -> STOPPED      -> LED OFF
```

该映射只用于测试。正式 DOUBLE 仍必须经过 APP SAMPLE_ONCE + Sensor + UART TX success 才提交 ONCE_SUCCESS。

观察：

```text
USART1 Serial Assistant
RTT / EasyLogger
PC13 LED
```

完成 smoke 后必须删除 normal startup 中的临时 Task / Queue hook，再执行 normal-path Keil rebuild。

## 8.6 Phase 5 完成门槛

```text
Platform Button Host Test PASS
Platform BSP Button Host Test PASS
Button Service Host Test PASS
Existing regression PASS
Keil Full Rebuild PASS
FreeRTOS Button + Indicator smoke PASS
Serial Assistant event observation PASS
RTT observation PASS
LED mapping PASS
Existing UART regression PASS
Temporary smoke startup path removed PASS
Normal-path Keil rebuild PASS
Coding Standard Review PASS
```

未完成真实板测时最多标记：

```text
IMPLEMENTED / TARGET BOARD VERIFICATION PENDING
```

---

# 9. Phase 6 — DHT20 Environment Module

目标：基于 Software I2C 实现 DHT20：

```text
initialization
status check
temperature
relative humidity
data validity
communication / data error semantics
```

专项设计阶段评估是否复用统一 `platform_device_t` 模型。

不在本 Phase 加 APP Control 状态。

---

# 10. Phase 7 — MPU6050 Motion Module

第一阶段只实现：

```text
WHO_AM_I
initialization
Accel X / Y / Z
Gyro X / Y / Z
raw / physical-unit conversion as designed
```

明确不做：

```text
Roll / Pitch / Yaw
DMP
Kalman Filter
Complementary Filter
high-rate attitude fusion
```

专项设计阶段评估统一 Device 模型。

---

# 11. Phase 8 — UART Application Communication

必须复用现有：

```text
UART DMA RX
 -> UART Service
 -> RingBuffer
 -> Communication Task
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

第一阶段使用文本上报；5 s report 与 ONCE 共用同一报告 / TX 能力。

不得把命令业务写进 UART Service。

---

# 12. Phase 9 — RTOS Task / Event Design

在各模块能力稳定后再冻结永久：

```text
Communication Task
Acquisition Task
Indicator Task
Button processing context
control event delivery
indicator event delivery
priority / stack
queue / notification
5 s scheduling
ONCE execution context
UART TX completion integration
```

已确认：Indicator Task 需要独立执行上下文。

仍未确认：Button 是否永久独立 Task。

Phase 5 临时 Button / Indicator Smoke Task 不能直接成为 Phase 9 结论。

DHT20 / MPU6050 第一版优先由单一 Acquisition Task 串行访问 I2C，不按设备机械拆 Task。

---

# 13. Phase 10 — Final APP Integration

APP 是唯一真实状态源：

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

最终行为：

```text
Startup -> STOPPED -> indicator STOPPED
START -> RUNNING -> indicator RUNNING -> every 5 s acquire + report
STOP -> STOPPED -> stop periodic flow -> indicator STOPPED
ONCE while STOPPED -> acquire once -> UART TX -> success -> ONCE_SUCCESS -> remain STOPPED
```

Button 与 UART 只产生控制输入，不拥有各自的 running 状态。

---

# 14. Final Integrated Board Test

至少验证：

```text
startup STOPPED / LED OFF
Button single -> RUNNING / LED ON
5 s DHT20 + MPU6050 reports
Button long -> STOPPED / LED OFF
Button double while STOPPED -> ONCE -> TX success -> 3 blinks
UART START / STOP / ONCE / STATUS / HELP
Button + UART use same APP state
RTT initialization / control / acquisition / error logs
I2C / Sensor / UART / RingBuffer error visibility
```

---

# 15. 横切约束

日志：

```text
INFO  -> lifecycle / state changes
DEBUG -> acquisition / command / TX summary
WARN  -> recoverable errors
ERROR -> init / critical failures
```

禁止逐 UART byte、逐 I2C bit / ACK、逐 LED edge、逐 Button 10 ms poll 正常刷日志。

Config：

```text
Acquisition = 5000 ms
Button = LOW active / PULL_UP / 10 / 30 / 300 / 3000 ms
LED = LOW active / 3 blinks / 100 ms ON / 100 ms OFF
```

静态配置集中在 `00_Config`；Context 保存运行状态；Data 保存采集结果。

---

# 16. 当前下一步

当前 Phase：

```text
Phase 5 — Button Module
PLAN READY / IMPLEMENTATION PENDING
```

直接执行：

```text
00_Doc/04_Agent/implementation_plan.md
```

执行时必须停在 Phase 5 完成点，不得在同一轮继续 Phase 6。
