# Button Phase 1 设计

> 文档类型：专项架构 / 模块设计  
> 状态：FROZEN FOR IMPLEMENTATION  
> 日期：2026-09-03  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 设计目的

本设计用于冻结 Phase 5 — Button Module 的第一阶段实现边界。

本阶段在已经验证的 Platform GPIO + STM32 GPIO Impl + User Key Board Binding 基线上，增加轻量 Platform Button 能力，并在 Service 层实现可 Host Test 的消抖与 SINGLE / DOUBLE / LONG 手势识别。

最终正式业务链为：

```text
PA0 electrical level
    ↓
Platform GPIO HIGH / LOW
    ↓
Platform Button PRESSED / RELEASED
    ↓
Button Service SINGLE / DOUBLE / LONG
    ↓
APP Control FSM                     (Phase 10)
    ↓
START / SAMPLE_ONCE / STOP
```

本阶段不提前实现最终 APP Control FSM，也不冻结永久 Button Task / priority / stack / final IPC。这些永久 RTOS 组织细节留到 Phase 9。

---

# 2. Phase 5 范围

第一阶段实现：

```text
Platform Button lightweight object
Platform Button init / read / deinit
User Key BSP Button construction
User Key active-level + pull configuration
Button Service init / process / deinit
Time-based debounce
SINGLE
DOUBLE
LONG >= 3000 ms
Host Test
Keil Full Rebuild
FreeRTOS target-board Button + Indicator smoke
Serial Assistant + RTT + LED observation
```

第一阶段明确不实现：

```text
Final APP Control FSM
Permanent Button Task ownership
Permanent Button Task priority / stack
Final Button -> APP Queue / Notification
Button EXTI
Low-power wake strategy
Button manager / registry
Dynamic allocation
platform_device_t inheritance for Button
New Button-specific STM32 Impl pass-through layer
DHT20 / MPU6050 acquisition
UART START / STOP / ONCE application command integration
```

---

# 3. 分层与职责冻结

## 3.1 Platform GPIO

Platform GPIO 继续只表达：

```text
INPUT / OUTPUT
PULL
HIGH / LOW
read / write / configure / deinit
```

Platform GPIO 不知道：

```text
pressed / released
single / double / long
User Key business meaning
```

## 3.2 Platform Button

Platform Button 表达“按键输入设备能力”，负责：

```text
own one platform_gpio_t
configure GPIO as INPUT
apply board-provided pull
read physical HIGH / LOW
translate active level to PRESSED / RELEASED
lifecycle validation
```

Platform Button 不负责：

```text
debounce
single click
double click
long press
APP START / STOP / ONCE
RTOS Task ownership
```

Platform Button 第一阶段不接入 `platform_device_t`，不建立 registry / manager，也不新增 `impl_button.c`。

STM32 硬件行为继续复用：

```text
Platform Button
    -> Platform GPIO
    -> STM32 GPIO Impl
    -> HAL GPIO
```

## 3.3 Button Service

Button Service 是纯时间驱动 gesture recognizer。

输入：

```text
logical PRESSED / RELEASED
caller-provided monotonic uint32_t nowMs
```

内部：

```text
raw input history
    ↓
time-based debounce
    ↓
stable PRESS / RELEASE edge
    ↓
gesture state machine
```

输出：

```text
NONE
SINGLE
DOUBLE
LONG
```

Button Service 不持有 `platform_button_t *`，不主动调用 `platform_button_read()`，也不直接调用 `platform_time_get_ms()`。这样 Host Test 可以直接注入状态与时间。

## 3.4 APP

正式系统中 APP 才负责：

```text
SINGLE -> START
DOUBLE -> SAMPLE_ONCE
LONG   -> STOP
```

Button Service 不读取或保存 APP `RUNNING / STOPPED` 状态。

---

# 4. Platform Button 对象模型

建议第一版公共类型冻结为：

```c
typedef enum
{
    PLATFORM_BUTTON_STATE_RELEASED = 0,
    PLATFORM_BUTTON_STATE_PRESSED,
    PLATFORM_BUTTON_STATE_MAX
} platform_button_state_t;

typedef struct platform_button
{
    platform_gpio_t gpio;
    platform_gpio_level_t activeLevel;
    platform_gpio_pull_t pull;
    platform_bool_t initialized;
} platform_button_t;
```

初始化宏：

```c
#define PLATFORM_BUTTON_INITIALIZER {0}
```

对象特征：

```text
caller-owned
static / zero-initializable
no malloc/free
one Button owns one GPIO object
```

---

# 5. Platform Button 公共接口

第一阶段公共接口冻结为：

```c
platform_error_t platform_button_init(platform_button_t *button);

platform_error_t platform_button_read(
    platform_button_t *button,
    platform_button_state_t *state);

platform_error_t platform_button_deinit(platform_button_t *button);
```

## 5.1 init

前置条件：

```text
button != NULL
button->gpio 已通过 BSP GPIO constructor 完成 binding
activeLevel 为 LOW / HIGH 合法枚举
pull 为 NONE / UP / DOWN 合法枚举
button 尚未完成 Button hardware init
```

GPIO 配置：

```text
direction    = PLATFORM_GPIO_DIRECTION_INPUT
pull         = button->pull
outputType   = PLATFORM_GPIO_OUTPUT_PUSH_PULL
initialLevel = PLATFORM_GPIO_LEVEL_LOW
```

`outputType / initialLevel` 在 INPUT 模式没有电气意义，但现有统一 `platform_gpio_config_t` 会校验这些字段，因此必须填入合法确定值。现有 STM32 GPIO Impl 在 INPUT 模式不会执行 initial-level 写操作。

成功后：

```text
button->initialized = PLATFORM_TRUE
button->gpio.configured = true
```

## 5.2 read

读取 Platform GPIO 物理电平，然后进行有效电平转换：

```text
level == activeLevel -> PRESSED
level != activeLevel -> RELEASED
```

不得把 LOW / HIGH 极性泄漏给 Service / APP。

## 5.3 deinit

调用 `platform_gpio_deinit()` 反配置硬件，并把 Button `initialized` 清零；保留既有 GPIO binding，使对象可以在同一绑定上重新 `platform_button_init()`。

---

# 6. Board / BSP Button

当前已验证真实硬件：

```text
PA0 -> User Key
GPIO_MODE_INPUT
GPIO_PULLUP
released = HIGH
pressed  = LOW
no EXTI
```

编译期静态配置进入：

```text
00_Config/project_config.h
```

冻结宏：

```c
#define PROJECT_USER_KEY_ACTIVE_LEVEL            PLATFORM_GPIO_LEVEL_LOW
#define PROJECT_USER_KEY_PULL                    PLATFORM_GPIO_PULL_UP
```

BSP Button constructor：

```c
platform_error_t platform_bsp_button_construct_user_key(
    platform_button_t *button);
```

职责：

```text
validate button pointer
reuse platform_bsp_gpio_construct_user_key(&button->gpio)
attach PROJECT_USER_KEY_ACTIVE_LEVEL
attach PROJECT_USER_KEY_PULL
```

BSP constructor 只 construct / bind / compose，不执行 hardware configure。

建议文件：

```text
03_Platform/platform_bsp/button/platform_button.h
03_Platform/platform_bsp/button/platform_button.c
03_Platform/platform_bsp/button/platform_bsp_button.h
03_Platform/platform_bsp/button/platform_bsp_button.c
```

---

# 7. Button Service 公共合同

建议第一版事件：

```c
typedef enum
{
    SERVICE_BUTTON_EVENT_NONE = 0,
    SERVICE_BUTTON_EVENT_SINGLE,
    SERVICE_BUTTON_EVENT_DOUBLE,
    SERVICE_BUTTON_EVENT_LONG,
    SERVICE_BUTTON_EVENT_MAX
} service_button_event_t;
```

内部 gesture state 建议显式表达：

```c
typedef enum
{
    SERVICE_BUTTON_GESTURE_IDLE = 0,
    SERVICE_BUTTON_GESTURE_FIRST_PRESS,
    SERVICE_BUTTON_GESTURE_WAIT_SECOND,
    SERVICE_BUTTON_GESTURE_SECOND_PRESS,
    SERVICE_BUTTON_GESTURE_LONG_HOLD
} service_button_gesture_state_t;
```

Context 至少保存：

```text
rawState
stableState
gestureState
rawChangedMs
pressStartedMs
firstReleaseMs
baselineValid
initialized
```

建议接口：

```c
platform_error_t service_button_init(service_button_t *service);

platform_error_t service_button_process(
    service_button_t *service,
    platform_button_state_t buttonState,
    uint32_t nowMs,
    service_button_event_t *event);

platform_error_t service_button_deinit(service_button_t *service);
```

初始化宏：

```c
#define SERVICE_BUTTON_INITIALIZER {0}
```

`service_button_process()` 每次成功调用必须明确输出一个事件；没有事件时输出 `SERVICE_BUTTON_EVENT_NONE`。

---

# 8. 静态时间参数

冻结到 `00_Config/project_config.h`：

```c
#define PROJECT_BUTTON_SAMPLE_PERIOD_MS          (10U)
#define PROJECT_BUTTON_DEBOUNCE_MS               (30U)
#define PROJECT_BUTTON_DOUBLE_CLICK_MS           (300U)
#define PROJECT_BUTTON_LONG_PRESS_MS             (3000U)
```

含义：

```text
10 ms   = 推荐调用周期，不是算法正确性的固定前提
30 ms   = raw state 必须连续稳定达到该时间才更新 stable state
300 ms  = 第一次稳定 RELEASE 后等待第二次稳定 PRESS 的窗口
3000 ms = 从稳定 PRESS 开始计算的长按阈值
```

所有时间判断统一使用：

```c
(uint32_t)(nowMs - startMs)
```

不得使用：

```c
nowMs >= startMs + timeout
```

以保证 `uint32_t` 毫秒计数自然回绕时仍正确。

---

# 9. Debounce 设计

Button Service 同时维护：

```text
rawState
stableState
rawChangedMs
```

规则：

```text
input != rawState
    -> rawState = input
    -> rawChangedMs = nowMs

rawState != stableState
and elapsed(rawChangedMs) >= 30 ms
    -> stableState = rawState
    -> produce one stable edge for gesture FSM
```

禁止把消抖定义为：

```text
连续 3 次采样
连续 N 次相同
```

因为这会把行为绑定到 Task 调度周期。

---

# 10. 初始样本规则

`service_button_init()` 只建立 Service 生命周期，不假设按键上电时一定 RELEASED。

第一次成功 `process()` 用于建立输入 baseline：

```text
rawState = input
stableState = input
baselineValid = true
```

若第一次状态为 RELEASED：

```text
gestureState = IDLE
```

若第一次状态为 PRESSED：

```text
gestureState = FIRST_PRESS
pressStartedMs = nowMs
```

第一次采样本身不产生事件；如果上电时持续按住，达到 3000 ms 后允许识别 LONG。

---

# 11. Gesture 状态机

状态：

```text
IDLE
FIRST_PRESS
WAIT_SECOND
SECOND_PRESS
LONG_HOLD
```

主要转换：

| Current | Condition | Next | Event |
| --- | --- | --- | --- |
| IDLE | stable PRESS | FIRST_PRESS | NONE |
| FIRST_PRESS | stable RELEASE before long | WAIT_SECOND | NONE |
| FIRST_PRESS | hold >= 3000 ms | LONG_HOLD | LONG |
| WAIT_SECOND | second stable PRESS, elapsed <= 300 ms | SECOND_PRESS | NONE |
| WAIT_SECOND | elapsed > 300 ms, no valid second press | IDLE | SINGLE |
| SECOND_PRESS | stable RELEASE before long | IDLE | DOUBLE |
| SECOND_PRESS | hold >= 3000 ms | LONG_HOLD | LONG |
| LONG_HOLD | stable RELEASE | IDLE | NONE |

## 11.1 SINGLE

第一次稳定 RELEASE 后不能立即发 SINGLE。

```text
first stable RELEASE
    -> WAIT_SECOND
    -> elapsed > 300 ms
    -> SINGLE
```

刚好 `300 ms` 仍属于双击有效窗口；无第二击时 SINGLE 会在第一次 `> 300 ms` 的 process 调用中确认。

## 11.2 DOUBLE

双击窗口从第一次稳定 RELEASE 开始。

只有第二次稳定 PRESS 必须满足：

```text
elapsed <= 300 ms
```

第二次 RELEASE 不要求仍在 300 ms 内；第二次短按完成后产生 DOUBLE。

必须保证：

```text
DOUBLE -> only DOUBLE
no preceding SINGLE
```

## 11.3 LONG

从稳定 PRESS 开始：

```text
elapsed >= 3000 ms -> LONG immediately once
```

产生 LONG 后进入 `LONG_HOLD`，直到稳定 RELEASE 才回 IDLE。

必须保证：

```text
2999 ms -> no LONG
3000 ms -> LONG
one hold -> exactly one LONG
LONG release -> no SINGLE
```

如果第一次短按后进入 `WAIT_SECOND`，第二次 PRESS 最终持续到 >= 3000 ms，则：

```text
LONG only
no DOUBLE
no SINGLE
```

## 11.4 过期窗口同时出现新 PRESS

若第二次稳定 PRESS 到达时已经：

```text
elapsed > PROJECT_BUTTON_DOUBLE_CLICK_MS
```

则本次 `process()` 应：

```text
confirm previous SINGLE
and treat current PRESS as a new FIRST_PRESS
```

不得因为确认前一次 SINGLE 而丢失新的稳定 PRESS edge。

---

# 12. Process 顺序

建议每次 `service_button_process()`：

```text
1. validate pointers / lifecycle / input enum
2. event = NONE
3. if no baseline -> establish baseline and return
4. update raw state + rawChangedMs
5. if debounce threshold reached -> update stable state and handle stable edge
6. evaluate gesture timeouts / long threshold
7. return one event at most
```

`WAIT_SECOND` 边界必须先处理已经形成的 stable PRESS，再判断 `elapsed > 300 ms` timeout，从而保证刚好 300 ms 的第二击仍可进入 DOUBLE 路径。

单次 `process()` 最多对外产生一个 public event。

---

# 13. Host Test 范围

## 13.1 Platform Button

至少覆盖：

```text
NULL validation
zero-initialized / unbound init rejected
activeLevel invalid rejected
pull invalid rejected
init configures INPUT + requested pull
active-low LOW -> PRESSED
active-low HIGH -> RELEASED
active-high HIGH -> PRESSED
active-high LOW -> RELEASED
read before init rejected
underlying GPIO read error propagation
deinit lifecycle
underlying GPIO configure / deinit error propagation
```

测试使用 fake GPIO，不依赖 HAL。

## 13.2 BSP Button

至少覆盖：

```text
NULL validation
User Key GPIO constructor called
active level = LOW
pull = UP
no hardware configure during constructor
binding error propagation
```

## 13.3 Button Service

至少覆盖：

```text
NULL / lifecycle / invalid input
first sample RELEASED -> NONE
first sample PRESSED -> no immediate event; 3000 ms -> LONG
press bounce does not produce duplicate edge
release bounce does not produce duplicate click
single -> exactly one SINGLE
single not emitted immediately on first release
double -> exactly one DOUBLE and no SINGLE
second stable PRESS exactly 300 ms -> DOUBLE path
second stable PRESS > 300 ms -> previous SINGLE + new first press
2999 ms -> no LONG
3000 ms -> exactly one LONG
5000 ms hold -> still one LONG
LONG release -> no SINGLE
first click + second long press -> LONG only
irregular process intervals preserve time semantics
uint32_t wraparound preserves debounce / double / long elapsed checks
```

Host Test 用人工 `nowMs` 推进，不真实 sleep。

---

# 14. FreeRTOS Target Smoke Test

目标板验证必须在 Scheduler 已启动后运行；本阶段明确验证真实 FreeRTOS 调度环境。

临时测试模块建议：

```text
Tests/button_smoke/
├── button_smoke.h
├── button_smoke.c
└── README.md
```

临时执行结构：

```text
Button Smoke Task
    -> platform_button_read()
    -> platform_time_get_ms()
    -> service_button_process()
    -> log / printf event
    -> map to service_indicator_event_t
    -> platform_queue_send()

Indicator Smoke Task
    -> platform_queue_receive()
    -> service_indicator_handle_event()
```

临时 Smoke 使用现有 Platform OS：

```text
platform_thread_create
platform_queue_create / send / receive
platform_time_get_ms
platform_time_delay_ms
```

不得在 smoke Task 中直接用：

```text
HAL_Delay
osDelay
vTaskDelay
raw FreeRTOS queue API
```

测试用 Task / Queue 的 stack / priority / depth 是 Harness 本地参数，只用于验证，不进入产品永久 Config，不代表 Phase 9 已冻结。

---

# 15. Button + Indicator Smoke 映射

为了同时验证现有 LED / Indicator 模块：

```text
SINGLE
    -> SERVICE_INDICATOR_EVENT_RUNNING
    -> LED ON

DOUBLE
    -> SERVICE_INDICATOR_EVENT_ONCE_SUCCESS
    -> LED blink 3 times
    -> final OFF

LONG
    -> SERVICE_INDICATOR_EVENT_STOPPED
    -> LED OFF
```

必须明确：

```text
DOUBLE -> ONCE_SUCCESS
```

只属于 Phase 5 Smoke Test，不是正式产品业务链。

正式系统仍然是：

```text
DOUBLE
 -> APP SAMPLE_ONCE
 -> DHT20 / MPU6050 acquisition
 -> UART TX
 -> successful TX completion
 -> ONCE_SUCCESS
```

Indicator blink 在 Indicator Smoke Task 中阻塞约 600 ms；Button Smoke Task 仍保持约 10 ms polling，所以双击闪灯不能阻塞按键识别。

---

# 16. 串口助手 / RTT / LED 观察合同

目标板必须同时观察：

```text
USART1 Serial Assistant
RTT / EasyLogger
LED visual behavior
```

Smoke-only 串口结构化输出建议：

```text
BUTTON_SMOKE,START
BUTTON_SMOKE,READY
BUTTON_SMOKE,EVENT,SINGLE
BUTTON_SMOKE,EVENT,DOUBLE
BUTTON_SMOKE,EVENT,LONG
BUTTON_SMOKE,FAIL,step=<name>,result=<error>
```

可沿用工程已有 `printf -> USART1` 的测试观察方式，但仅允许测试 Harness 使用；正式 Platform Button / Button Service 不引入 `printf`。

RTT 记录：

```text
smoke lifecycle
stable press / release when useful
gesture event
initialization / queue / read / time error
```

禁止每 10 ms polling 打日志。

推荐人工顺序：

```text
1. startup       -> LED OFF
2. single        -> SINGLE + LED ON
3. double        -> DOUBLE + 3 blinks + final OFF
4. single        -> SINGLE + LED ON
5. long >= 3 s   -> LONG + LED OFF
```

同时确认：

```text
no duplicate event
no DOUBLE preceded by SINGLE
LONG release has no SINGLE
no HardFault
no obvious scheduling stall
existing UART receive path still works
RTT remains available
```

---

# 17. 临时 Smoke 生命周期

Smoke Harness 可以在 `MX_FREERTOS_Init()` 的 `USER CODE` 区完成测试对象、Queue 和两个临时 Thread 的创建，实际 Task 只在 `osKernelStart()` 后运行。

应放在 USART1 mutex / normal APP composition 已建立之后，避免测试日志或串口观察早于现有基础设施初始化。

Smoke 完成或中止后必须：

```text
remove freertos.c smoke include / start hook
remove temporary smoke source from Keil project
remove temporary smoke include path if added
restore normal firmware path
Keil Full Rebuild again
```

不得把 Smoke Button Task / Indicator Task / Queue 留在正常产品启动路径。

---

# 18. 第一阶段计划文件结构

生产代码：

```text
03_Platform/platform_bsp/button/
├── platform_button.h
├── platform_button.c
├── platform_bsp_button.h
└── platform_bsp_button.c

02_Service/service_button/
├── service_button.h
└── service_button.c

00_Config/project_config.h
```

Host Test：

```text
Tests/platform_button/test_platform_button.c
Tests/platform_bsp_button/test_platform_bsp_button.c
Tests/service_button/test_service_button.c
```

Target Smoke：

```text
Tests/button_smoke/button_smoke.h
Tests/button_smoke/button_smoke.c
Tests/button_smoke/README.md
```

Keil：

```text
RTT_elog_DMA_UART_ring_project.uvprojx
```

临时 Smoke glue：

```text
Core/Src/freertos.c USER CODE sections only
```

原则上不修改公共合同：

```text
03_Platform/platform_mcu/gpio/platform_gpio.h
03_Platform/platform_mcu/gpio/platform_gpio.c
03_Platform/platform_bsp/platform_bsp_gpio.h
04_Impl/impl_mcu/impl_platform_gpio.c
04_Impl/impl_bsp/impl_platform_bsp_gpio.c
03_Platform/platform_os/platform_time.h
03_Platform/platform_os/platform_thread.h
03_Platform/platform_os/platform_queue.h
02_Service/service_indicator/service_indicator.h
02_Service/service_indicator/service_indicator.c
```

---

# 19. 完成门槛

必须至少满足：

```text
Button Phase 1 frozen design                    PASS
Platform Button Host Test                       PASS
Platform BSP Button Host Test                   PASS
Button Service Host Test                        PASS
Platform GPIO regression                        PASS
Indicator / Platform LED regression             PASS
Platform OS relevant regression                 PASS
Coding Standard Review                          PASS
Keil Full Rebuild                               PASS
FreeRTOS scheduler target smoke                 PASS
Serial Assistant SINGLE observation             PASS
Serial Assistant DOUBLE observation             PASS
Serial Assistant LONG observation               PASS
RTT Button smoke observation                    PASS
SINGLE -> LED ON                                PASS
DOUBLE -> 3 blinks -> OFF                       PASS
LONG -> LED OFF                                 PASS
No duplicate gesture                            PASS
No DOUBLE + preceding SINGLE                    PASS
No LONG release SINGLE                          PASS
Existing UART communication regression          PASS
Temporary smoke path removed                    PASS
Normal-path Keil Full Rebuild                   PASS
No Final APP Control FSM introduced             PASS
No permanent Button Task frozen                 PASS
No impl_button pass-through                     PASS
No Button platform_device_t / registry          PASS
No Button EXTI                                  PASS
```

若 Host / Keil 已 PASS 但真实板测未完成，Phase 5 只能记录：

```text
IMPLEMENTED / TARGET BOARD VERIFICATION PENDING
```

只有真实板测与清理完成后才能标记：

```text
COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

---

# 20. 设计结论

Phase 5 第一阶段冻结为：

```text
Platform Button
    = physical level -> PRESSED / RELEASED

Button Service
    = debounce + SINGLE / DOUBLE / LONG

APP
    = future START / SAMPLE_ONCE / STOP decision
```

Button 是轻量输入设备，不新增 `impl_button`，不接入 `platform_device_t`。

Service 使用 caller-provided state + `nowMs`，因此算法可 Host Test、与永久 RTOS Task 组织解耦。

目标板验证必须放在 FreeRTOS Scheduler 环境，并用 Button Task 与 Indicator Task 分离按键采样和阻塞灯效；串口助手、RTT、LED 三通道共同验收。
