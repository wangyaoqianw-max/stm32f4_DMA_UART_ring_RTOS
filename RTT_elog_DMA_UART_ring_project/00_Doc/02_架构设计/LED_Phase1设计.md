# LED Phase 1 设计

> 文档类型：专项架构 / 模块设计  
> 状态：FROZEN FOR IMPLEMENTATION  
> 日期：2026-09-03  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 设计目的

本设计用于冻结 Phase 4 — LED Module 的第一阶段实现边界。

本阶段目标是在已经验证的 Platform GPIO + STM32 GPIO Impl + Board GPIO Binding 基线上，增加一层轻量 LED 设备能力，并在 Service 层建立提示灯事件语义，为后续独立 Indicator Task 和最终 APP Control FSM 提供稳定接口。

本阶段不提前实现最终 APP 控制状态机，也不提前冻结最终 RTOS 事件队列、Task 优先级、栈大小等调度细节。

最终职责链：

```text
APP / Control              (Phase 10)
    ↓ semantic event
Indicator Task             (Phase 9)
    ↓
Indicator Service          (Phase 4)
    ↓
Platform LED               (Phase 4)
    ↓
Platform GPIO              (existing)
    ↓
STM32 GPIO Impl            (existing)
    ↓
HAL / PC13
```

---

# 2. Phase 4 范围

第一阶段实现：

```text
Platform LED lightweight object
Platform LED init / on / off / toggle / deinit
Status LED Board/BSP construction
Indicator Service init / event handling / deinit
STOPPED indication
RUNNING indication
ONCE_SUCCESS three-blink indication
Static LED / blink configuration
Host Test
Keil Full Rebuild
FreeRTOS Task Context target-board smoke verification
```

第一阶段明确不实现：

```text
Final APP Control FSM
Permanent Indicator Task creation
Final Indicator event queue / notification mechanism
Indicator Task stack size / priority
Button integration
UART START / STOP / ONCE command integration
Sensor acquisition
TX Complete to Indicator event wiring
PWM brightness
Breathing LED
LED manager / registry
Dynamic allocation
platform_device_t inheritance for LED
New LED-specific STM32 Impl pass-through layer
```

---

# 3. 分层与职责冻结

## 3.1 Platform LED

Platform LED 表达“LED 设备能力”，负责：

```text
constructable lightweight LED object
hardware initialization through Platform GPIO
ON
OFF
TOGGLE
DEINIT
active-level translation
```

Platform LED 不负责：

```text
RUNNING / STOPPED product state
ONCE success decision
blink count policy
blink timing policy
RTOS Task ownership
APP control state
UART TX status
```

LED 不接入统一 `platform_device_t` / device type 模型。

原因：当前 LED 只需要轻量执行器能力，没有复杂设备生命周期、状态机、设备注册、统一查找或设备管理需求。`platform_led_t` 自身的静态类型已经表达设备身份，再增加 runtime device type 只会形成重复信息。

## 3.2 Indicator Service

Indicator Service 表达“产品提示灯语义”，负责把提示事件转换为 LED 行为：

```text
STOPPED      -> OFF
RUNNING      -> ON
ONCE_SUCCESS -> blink 3 times -> OFF
```

Indicator Service 不维护系统 `RUNNING / STOPPED` 真正业务状态，不决定 ONCE 是否允许执行，也不判断 UART 是否真的发送成功。

未来这些业务决策由 APP Control FSM 负责；Indicator Service 只消费已经确认的提示事件。

## 3.3 Impl

Phase 4 不新增 `impl_led.c`。

STM32 相关硬件实现已经由：

```text
Platform LED
    -> Platform GPIO
    -> existing STM32 GPIO Impl
```

承担。

禁止增加如下无实际职责的透传链：

```text
platform_led_on()
    -> impl_led_on()
    -> platform_gpio_write()
    -> impl_gpio_write()
```

---

# 4. Platform LED 对象模型

`platform_led_t` 定位为 caller-owned、静态、轻量对象。

对象应至少保存：

```text
embedded platform_gpio_t
active level
object / hardware lifecycle state required for validation
```

第一版不保存：

```text
device type
platform_device_t
ops table
registry id
RTOS handle
blink state
blink counter
business running state
```

LED 与 GPIO 为一对一关系，因此 `platform_led_t` 直接拥有自己的 `platform_gpio_t` 存储，不要求上层额外长期维护一个独立 GPIO 对象指针。

对象必须支持静态零初始化，不使用 malloc / free。

---

# 5. Board / BSP 与有效电平

当前真实板级资源：

```text
PC13 -> Status LED
```

具体 PC13 / HAL Port / Pin 绑定继续复用当前已有：

```text
platform_bsp_gpio_construct_status_led()
    -> impl_bsp GPIO binding
```

Phase 4 不在 Config 中重复定义 PC13，也不新建第二套 GPIO Port / Pin 配置源。

Status LED 的有效电平属于板级静态硬件属性，其配置值统一进入：

```text
00_Config/project_config.h
```

计划静态配置：

```text
PROJECT_STATUS_LED_ACTIVE_LEVEL
```

当前开发板应配置为低电平有效。

职责关系：

```text
00_Config
    -> active level value

Platform BSP LED constructor
    -> compose Status LED object
    -> reuse existing Status LED GPIO binding

Platform LED
    -> ON means write active level
    -> OFF means write inactive level
```

Service / APP 不得出现“LOW = ON”或“PC13”语义。

---

# 6. Platform LED 公共能力

第一阶段公共能力冻结为：

```text
platform_led_init
platform_led_on
platform_led_off
platform_led_toggle
platform_led_deinit
```

同时提供 Status LED Board/BSP constructor，用于把当前板级 GPIO Binding 与静态 active-level 配置组合成 `platform_led_t`。

建议文件职责：

```text
03_Platform/platform_bsp/led/platform_led.h
03_Platform/platform_bsp/led/platform_led.c
    -> generic LED object + actions

03_Platform/platform_bsp/led/platform_bsp_led.h
03_Platform/platform_bsp/led/platform_bsp_led.c
    -> current board Status LED construction
```

`platform_bsp_led` 只做 Platform 对象组合和板级静态属性装配，不引入 STM32 HAL 依赖。

LED 初始化后必须建立安全默认状态：

```text
LED OFF
```

`on / off / toggle` 仅在 LED 已完成硬件初始化后有效。

---

# 7. Indicator Service 事件模型

第一阶段采用统一事件接口，而不是为每个语义分别建立一套外部函数。

事件冻结为：

```text
SERVICE_INDICATOR_EVENT_STOPPED
SERVICE_INDICATOR_EVENT_RUNNING
SERVICE_INDICATOR_EVENT_ONCE_SUCCESS
```

Service 对外能力：

```text
service_indicator_init
service_indicator_handle_event
service_indicator_deinit
```

Indicator Service 应保持轻量 caller-owned Context，至少引用一个已经初始化的 `platform_led_t`，不拥有 Platform LED 对象存储。

事件行为：

| Event | 行为 |
| --- | --- |
| `STOPPED` | LED OFF |
| `RUNNING` | LED ON |
| `ONCE_SUCCESS` | ON/OFF 共 3 次，完成后 OFF |

`ONCE_SUCCESS` 的输入前提由未来 APP / Communication 负责。Phase 4 不负责建立 UART TX Complete 到该事件的正式链路。

---

# 8. 闪烁时序与 Platform Time

静态提示参数统一进入：

```text
00_Config/project_config.h
```

计划配置：

```text
PROJECT_INDICATOR_BLINK_COUNT      = 3
PROJECT_INDICATOR_BLINK_ON_MS      = 100 ms
PROJECT_INDICATOR_BLINK_OFF_MS     = 100 ms
```

Phase 4 不使用 `HAL_Delay()`、`osDelay()` 或 `vTaskDelay()` 直接实现 Service 延时。

Indicator Service 使用现有 Platform OS 时间接口：

```text
platform_time_delay_ms()
```

该接口明确用于 Task Context 调度延时。

当前已经确定最终系统会为提示灯建立独立 Indicator Task，因此三闪允许采用顺序阻塞式灯效实现。阻塞范围只影响 Indicator Task，不应阻塞 Acquisition / UART 等其他任务。

禁止：

```text
ISR / HAL Callback -> blink delay
busy-wait millisecond blink
Service -> HAL_Delay()
```

`platform_time_get_ms()` 不作为第一版三闪的必要实现方案。

---

# 9. Indicator Task 边界

最终架构意图已经冻结：

```text
独立 Indicator Task
```

其价值是：

```text
serialize indicator events
isolate blocking blink timing
avoid blocking APP / UART / acquisition execution contexts
```

但 Phase 4 不正式创建该 Task。

以下内容统一留到 Phase 9 — RTOS Task / Event Design：

```text
Indicator Task permanent creation
stack size
priority
event queue / notification mechanism
event overwrite / queueing policy
interaction with APP Control FSM
```

Phase 4 只保证 Indicator Service 的行为可以在 Task Context 中正确执行。

---

# 10. 日志要求

正式模块遵循现有 Service Log / RTT 架构。

建议日志粒度：

```text
INFO  -> Indicator Service init / deinit failure or major lifecycle result if useful
DEBUG -> semantic indicator event if needed
WARN  -> recoverable LED / delay failure
ERROR -> initialization failure that prevents indicator use
```

正常三闪期间禁止逐 ON / OFF 边沿打印日志。

Platform LED / Platform GPIO 正常 on/off 路径不重复打印成功日志。

---

# 11. Host Test 范围

Host Test 至少覆盖：

## 11.1 Platform LED

```text
NULL / invalid state validation
construct / init lifecycle
init establishes OFF state
active-low ON mapping
active-low OFF mapping
toggle behavior
operation before init rejected
deinit lifecycle
underlying GPIO error propagation
```

测试使用 fake Platform GPIO，不依赖 HAL。

## 11.2 Indicator Service

```text
NULL / invalid state validation
STOPPED -> LED OFF
RUNNING -> LED ON
ONCE_SUCCESS -> exactly configured blink count
ONCE_SUCCESS ends in OFF
configured on/off delay is requested
Platform LED failure propagation
Platform Time failure handling
Service does not maintain APP running state
```

Host Test 可以替换 / fake `platform_time_delay_ms()`，不得真实等待数百毫秒来完成单元测试。

---

# 12. 目标板 Smoke Test

目标板验证必须运行在 FreeRTOS Scheduler 已启动后的 Task Context。

临时验证代码必须集中、可识别、可完整移除；不得把 smoke 流程长期留在 APP / Service 生产路径。

临时测试延时统一使用：

```text
platform_time_delay_ms()
```

不得继续使用以前 smoke 中常见的 `HAL_Delay()`。

推荐观察顺序：

```text
Smoke Start
    ↓
STOPPED / OFF      1 s
    ↓
RUNNING / ON       2 s
    ↓
STOPPED / OFF      1 s
    ↓
ONCE_SUCCESS
    -> 100 ms ON / 100 ms OFF
    -> repeat 3 times
    -> final OFF
    ↓
Smoke PASS
```

验证工具：

```text
1. LED 肉眼观察                PRIMARY
2. RTT / EasyLogger             PRIMARY
3. PC Serial Assistant          AUXILIARY / regression observation
```

Phase 4 不要求逻辑分析仪。LED 为 100 ms 级人眼可直接观察行为，没有必要为本阶段引入波形仪器验收。

串口助手主要用于确认现有通信功能在 LED smoke 期间仍可继续运行；若当前基线没有可见 echo / response，不应为了 LED smoke 新建第二套 UART 验证架构，最终以 LED 行为 + RTT + 现有通信回归结果为准。

RTT smoke 日志建议只记录阶段：

```text
indicator smoke start
STOPPED
RUNNING
STOPPED
ONCE_SUCCESS
indicator smoke pass / fail
```

不逐次打印 6 个亮灭边沿。

Smoke 完成后必须：

```text
remove temporary smoke hook
remove temporary smoke source from Keil if created
restore normal firmware path
Keil Full Rebuild again
```

---

# 13. 第一阶段计划文件结构

生产代码计划：

```text
02_Service/service_indicator/
├── service_indicator.h
└── service_indicator.c

03_Platform/platform_bsp/
└── led/
    ├── platform_led.h
    ├── platform_led.c
    ├── platform_bsp_led.h
    └── platform_bsp_led.c

00_Config/project_config.h
```

复用且原则上不修改公共合同：

```text
03_Platform/platform_mcu/gpio/platform_gpio.h
03_Platform/platform_mcu/gpio/platform_gpio.c
03_Platform/platform_bsp/platform_bsp_gpio.h
04_Impl/impl_mcu/impl_platform_gpio.c
04_Impl/impl_bsp/impl_platform_bsp_gpio.c
03_Platform/platform_os/platform_time.h
```

测试计划：

```text
Tests/platform_led/
Tests/service_indicator/
Tests/indicator_smoke/       // only if a dedicated temporary smoke source is useful
```

Phase 4 不新增正式 `01_APP` 业务代码。

---

# 14. 完成门槛

必须至少满足：

```text
LED frozen design                            PASS
Platform LED Host Test                       PASS
Indicator Service Host Test                  PASS
Existing Platform GPIO regression            PASS
Coding Standard Review                       PASS
Keil Full Rebuild                            PASS
Target LED OFF observation                   PASS
Target LED ON observation                    PASS
Target ONCE three-blink observation           PASS
Target final OFF observation                 PASS
RTT smoke observation                        PASS
Existing communication regression            PASS
Temporary smoke path removed                 PASS
Normal-path Keil Full Rebuild                PASS
No APP Control FSM introduced                PASS
No permanent Indicator Task introduced       PASS
No HAL_Delay in Service / smoke Task path    PASS
No new impl_led pass-through layer           PASS
No LED platform_device_t / registry          PASS
```

若 Host / Keil 已通过但真实板测未完成：

```text
Phase 4 — IMPLEMENTED / TARGET BOARD VERIFICATION PENDING
```

全部通过后：

```text
Phase 4 — COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

---

# 15. 后续阶段接口关系

Phase 4 完成后：

```text
Phase 5 Button
    -> only produces button events

Phase 8 UART Application Communication
    -> only produces command / TX result semantics

Phase 9 RTOS Task / Event Design
    -> create permanent Indicator Task
    -> freeze indicator event delivery

Phase 10 Final APP Integration
    -> APP decides RUNNING / STOPPED / ONCE_SUCCESS semantics
    -> submits indicator events
```

最终 APP 仍是唯一业务状态源，Indicator Service 和 Indicator Task 都不得自行维护第二套采集状态。
