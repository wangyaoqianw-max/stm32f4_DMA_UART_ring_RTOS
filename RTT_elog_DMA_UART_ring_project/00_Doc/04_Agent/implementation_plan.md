# Button Phase 5 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> 当前执行计划 / Current Active Plan  
> 状态：READY FOR IMPLEMENTATION  
> 日期：2026-09-03

**Goal:** 在已验证 Platform GPIO / Board User Key / LED / Indicator / Platform OS 基线上，实现轻量 Platform Button 与可 Host Test 的 Button Service，并完成 FreeRTOS 下 Button + Indicator 联动目标板验证。

**Architecture:** 正式能力链为 `Platform GPIO -> Platform Button -> Button Service -> future APP`。Platform Button 只完成有效电平转换，Button Service 只完成 time-based debounce 与 SINGLE / DOUBLE / LONG；本 Phase 不新增 `impl_button`、不实现最终 APP FSM、不冻结永久 Button Task。目标板 Smoke 使用两个临时 Platform Thread + Platform Queue，将 Button gesture 映射到现有 Indicator Service，避免三闪阻塞 10 ms 按键采样。

**Tech Stack:** C、STM32F411CEU6、Platform GPIO、Platform Button、Platform OS Thread / Queue / Time、CMSIS-RTOS2 + FreeRTOS、Indicator Service、Platform LED、EasyLogger + SEGGER RTT、USART1 Serial Assistant、Host C Test、Keil MDK-ARM。

**Spec:**
- `00_Doc/02_架构设计/Button_Phase1设计.md`
- `00_Doc/00_项目需求/最终功能需求.md`
- `00_Doc/04_Agent/requirements.md`
- `00_Doc/04_Agent/architecture.md`
- `00_Doc/04_Agent/development_roadmap.md` Phase 5
- `00_Doc/04_Agent/handoff.md`

---

# Global Constraints

- 固定依赖方向保持 `APP -> Service -> Platform -> Impl -> Vendor`。
- 本 Phase 不新增正式 APP Control FSM。
- Button 第一阶段使用 polling；不启用 EXTI。
- `platform_button_t` 为 caller-owned lightweight object，拥有一个 `platform_gpio_t`，不使用 malloc/free、`platform_device_t`、registry、manager。
- 禁止新增 `impl_button.c`；STM32 硬件继续复用 Platform GPIO + STM32 GPIO Impl。
- User Key 物理绑定继续复用 `platform_bsp_gpio_construct_user_key()`；不得重复定义 PA0 / GPIOA / GPIO_PIN_0。
- 固定 User Key：`PROJECT_USER_KEY_ACTIVE_LEVEL = PLATFORM_GPIO_LEVEL_LOW`、`PROJECT_USER_KEY_PULL = PLATFORM_GPIO_PULL_UP`。
- 固定时间：sample = 10 ms、debounce = 30 ms、double window = 300 ms、long = 3000 ms。
- Debounce 必须以 elapsed time 判断，不得以固定 N 次采样定义。
- Button Service 不持有 Platform Button，不读取 GPIO，不直接获取 OS tick；`process()` 接收 `PRESSED / RELEASED + nowMs`。
- SINGLE 必须等待双击窗口；DOUBLE 不得先产生 SINGLE。
- second stable PRESS `<= 300 ms` 属于 DOUBLE 候选；`> 300 ms` 时确认前一次 SINGLE，并把当前 PRESS 作为新 FIRST_PRESS。
- LONG 在 stable PRESS elapsed `>= 3000 ms` 时立即且只产生一次；LONG release 不得产生 SINGLE。
- 所有 elapsed-time 判断必须使用 `(uint32_t)(nowMs - startMs)` 形式支持 wraparound。
- Host Test 不真实 sleep 3 s。
- Target Smoke 必须在 Scheduler 已启动的 Task Context 运行。
- Smoke 使用 Platform Thread / Queue / Time；不得直接用 raw FreeRTOS queue、`HAL_Delay()`、`osDelay()`、`vTaskDelay()`。
- Smoke 允许 test-only `printf` 输出结构化 USART1 标记；生产 Platform Button / Button Service 禁止引入 printf。
- Button Smoke Task 负责按键读取 / process / serial+RTT event / Queue send；Indicator Smoke Task 负责消费 Queue 并执行 Indicator Service。
- Smoke 映射固定：SINGLE -> RUNNING、DOUBLE -> ONCE_SUCCESS、LONG -> STOPPED；仅用于测试，不得写成正式 APP 业务。
- 正常运行禁止逐 10 ms Button polling 打日志。
- 临时 Smoke 代码必须集中、可识别、可完整移除；清理后执行正常路径 Keil Full Rebuild。
- 所有自研代码执行前完整读取 `00_Doc/02_架构设计/嵌入式项目C代码设计规范.md`。
- 每个 Task 提交前执行 Coding Standard Review。
- 若冻结设计与仓库现实存在实质冲突：`STOP / BLOCKED`，不得静默重设计。

---

# 0. Mandatory Preflight

执行前完整读取：

```text
00_Doc/00_项目需求/最终功能需求.md
00_Doc/02_架构设计/Button_Phase1设计.md
00_Doc/02_架构设计/LED_Phase1设计.md
00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md
00_Doc/02_架构设计/GPIO_STM32_Impl_Phase1设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/implementation_plan.md
```

重点检查生产基线：

```text
00_Config/project_config.h
03_Platform/platform_mcu/gpio/platform_gpio.h
03_Platform/platform_mcu/gpio/platform_gpio_types.h
03_Platform/platform_mcu/gpio/platform_gpio.c
03_Platform/platform_bsp/platform_bsp_gpio.h
04_Impl/impl_mcu/impl_platform_gpio.c
04_Impl/impl_bsp/impl_platform_bsp_gpio.c
03_Platform/platform_bsp/led/platform_led.h
03_Platform/platform_bsp/led/platform_bsp_led.h
02_Service/service_indicator/service_indicator.h
03_Platform/platform_os/platform_time.h
03_Platform/platform_os/platform_thread.h
03_Platform/platform_os/platform_queue.h
02_Service/service_log/service_log.h
01_APP/app_system.c
Core/Src/freertos.c
RTT_elog_DMA_UART_ring_project.uvprojx
```

Preflight 必须确认：

```text
PA0 User Key target behavior       released HIGH / pressed LOW
platform_bsp_gpio_construct_user_key() PRESENT
Platform GPIO contract             STABLE
Platform Time get/delay            PRESENT
Platform Thread / Queue             PRESENT
Indicator Service                  VERIFIED
Platform LED                       VERIFIED
FreeRTOS normal scheduler           ACTIVE
Existing UART communication         PRESERVED
```

若任何冻结前提不成立，停止并报告，不自行改架构。

---

### Task 1: Implement Platform Button Contract and Static Configuration

**Files:**
- Create: `03_Platform/platform_bsp/button/platform_button.h`
- Create: `03_Platform/platform_bsp/button/platform_button.c`
- Modify: `00_Config/project_config.h`
- Create: `Tests/platform_button/test_platform_button.c`

**Interfaces — Produces:**

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

#define PLATFORM_BUTTON_INITIALIZER {0}

platform_error_t platform_button_init(platform_button_t *button);
platform_error_t platform_button_read(platform_button_t *button,
                                      platform_button_state_t *state);
platform_error_t platform_button_deinit(platform_button_t *button);
```

Config additions:

```c
#define PROJECT_USER_KEY_ACTIVE_LEVEL            PLATFORM_GPIO_LEVEL_LOW
#define PROJECT_USER_KEY_PULL                    PLATFORM_GPIO_PULL_UP
#define PROJECT_BUTTON_SAMPLE_PERIOD_MS          (10U)
#define PROJECT_BUTTON_DEBOUNCE_MS               (30U)
#define PROJECT_BUTTON_DOUBLE_CLICK_MS           (300U)
#define PROJECT_BUTTON_LONG_PRESS_MS             (3000U)
```

- [ ] **Step 1: Write failing Platform Button Host tests**

Test exactly:

```text
NULL init/read/deinit
unbound zero object init -> NOT_INITIALIZED or existing GPIO lifecycle-consistent error
invalid activeLevel -> INVALID_PARAM
invalid pull -> INVALID_PARAM
init -> GPIO INPUT + configured pull
active-low LOW -> PRESSED
active-low HIGH -> RELEASED
active-high HIGH -> PRESSED
active-high LOW -> RELEASED
read before Button init -> NOT_INITIALIZED
GPIO configure/read/deinit errors propagate
successful deinit clears Button initialized while preserving GPIO binding
```

Use the same fake `platform_gpio_ops_t` pattern already used by `Tests/platform_led/test_platform_led.c`.

- [ ] **Step 2: Run the new test and verify it fails before implementation**

Build the focused host executable with the same host compiler / include conventions used by existing Platform LED / GPIO tests. Expected failure: missing `button/platform_button.h` or missing symbols.

- [ ] **Step 3: Add the six frozen Button macros to `project_config.h`**

Keep them beside existing Software I2C / Status LED product parameters. Do not add PA0 / GPIOA / pin macros.

- [ ] **Step 4: Implement `platform_button.h/.c` minimally**

`platform_button_init()` must configure:

```c
platform_gpio_config_t config = {
    PLATFORM_GPIO_DIRECTION_INPUT,
    button->pull,
    PLATFORM_GPIO_OUTPUT_PUSH_PULL,
    PLATFORM_GPIO_LEVEL_LOW
};
```

`outputType / initialLevel` are only legal placeholder fields for the existing unified GPIO config; INPUT mode must not drive the pin.

`platform_button_read()` must call `platform_gpio_read()` and compare physical level with `activeLevel`.

- [ ] **Step 5: Run Platform Button test until PASS with `-Wall -Wextra -Werror`**

- [ ] **Step 6: Run existing Platform GPIO and Platform LED focused regressions**

Expected: all existing tests still PASS; no public GPIO contract changes.

- [ ] **Step 7: Coding Standard Review and focused commit**

Suggested commit:

```text
feat: add platform button abstraction
```

---

### Task 2: Add User Key BSP Button Composition

**Files:**
- Create: `03_Platform/platform_bsp/button/platform_bsp_button.h`
- Create: `03_Platform/platform_bsp/button/platform_bsp_button.c`
- Create: `Tests/platform_bsp_button/test_platform_bsp_button.c`

**Consumes:**

```c
platform_button_t
platform_bsp_gpio_construct_user_key()
PROJECT_USER_KEY_ACTIVE_LEVEL
PROJECT_USER_KEY_PULL
```

**Produces:**

```c
platform_error_t platform_bsp_button_construct_user_key(
    platform_button_t *button);
```

- [ ] **Step 1: Write failing BSP composition test**

Verify exactly:

```text
NULL -> NULL_POINTER
calls User Key GPIO constructor exactly once
successful constructor leaves GPIO bound but not configured
activeLevel == PLATFORM_GPIO_LEVEL_LOW
pull == PLATFORM_GPIO_PULL_UP
GPIO constructor failure propagates
no HAL symbol / PA0 macro exposed by platform_bsp_button
```

- [ ] **Step 2: Run the BSP test and verify failure before implementation**

Expected failure: missing `platform_bsp_button` symbols.

- [ ] **Step 3: Implement minimal BSP Button composition**

Order:

```text
validate pointer
construct user-key GPIO binding
if failure -> return failure
set activeLevel / pull
return OK
```

Do not call `platform_button_init()` inside constructor.

- [ ] **Step 4: Run BSP Button + Platform BSP GPIO + Platform Button regressions**

Expected: PASS with warnings treated as errors.

- [ ] **Step 5: Coding Standard Review and focused commit**

Suggested commit:

```text
feat: add user key button bsp binding
```

---

### Task 3: Implement Button Service Time State Machine with TDD

**Files:**
- Create: `02_Service/service_button/service_button.h`
- Create: `02_Service/service_button/service_button.c`
- Create: `Tests/service_button/test_service_button.c`

**Interfaces — Produces:**

```c
typedef enum
{
    SERVICE_BUTTON_EVENT_NONE = 0,
    SERVICE_BUTTON_EVENT_SINGLE,
    SERVICE_BUTTON_EVENT_DOUBLE,
    SERVICE_BUTTON_EVENT_LONG,
    SERVICE_BUTTON_EVENT_MAX
} service_button_event_t;

typedef enum
{
    SERVICE_BUTTON_GESTURE_IDLE = 0,
    SERVICE_BUTTON_GESTURE_FIRST_PRESS,
    SERVICE_BUTTON_GESTURE_WAIT_SECOND,
    SERVICE_BUTTON_GESTURE_SECOND_PRESS,
    SERVICE_BUTTON_GESTURE_LONG_HOLD
} service_button_gesture_state_t;

typedef struct
{
    platform_button_state_t rawState;
    platform_button_state_t stableState;
    service_button_gesture_state_t gestureState;
    uint32_t rawChangedMs;
    uint32_t pressStartedMs;
    uint32_t firstReleaseMs;
    platform_bool_t baselineValid;
    platform_bool_t initialized;
} service_button_t;

#define SERVICE_BUTTON_INITIALIZER {0}

platform_error_t service_button_init(service_button_t *service);
platform_error_t service_button_process(service_button_t *service,
                                        platform_button_state_t buttonState,
                                        uint32_t nowMs,
                                        service_button_event_t *event);
platform_error_t service_button_deinit(service_button_t *service);
```

- [ ] **Step 1: Write lifecycle / first-sample failing tests**

Exact cases:

```text
NULL init/process/deinit
process before init
invalid input enum
init twice
first RELEASED -> NONE + IDLE baseline
first PRESSED -> NONE + FIRST_PRESS baseline
first PRESSED held to 2999 -> NONE
first PRESSED held to 3000 -> LONG
```

- [ ] **Step 2: Run focused service test and verify failure**

Expected failure: missing service contract.

- [ ] **Step 3: Implement lifecycle + baseline only**

Do not implement gestures beyond what the current tests require.

- [ ] **Step 4: Add failing debounce tests**

Feed timestamped bounce sequences where raw PRESS / RELEASE toggles faster than 30 ms. Verify stable state changes only after a continuous >=30 ms candidate and no public event is duplicated.

- [ ] **Step 5: Implement elapsed-time debounce**

Required arithmetic:

```c
(uint32_t)(nowMs - service->rawChangedMs)
```

No sample counters.

- [ ] **Step 6: Add failing SINGLE tests**

Exact behavior:

```text
short stable PRESS -> stable RELEASE -> no immediate event
elapsed == 300 ms after first release -> still NONE
first process with elapsed > 300 ms -> exactly one SINGLE
later RELEASED calls -> NONE
```

- [ ] **Step 7: Implement `FIRST_PRESS -> WAIT_SECOND -> SINGLE`**

- [ ] **Step 8: Add failing DOUBLE tests**

Exact behavior:

```text
first short click
second stable PRESS at 299 ms -> candidate
second stable RELEASE -> exactly one DOUBLE
no SINGLE before or after
second stable PRESS at exactly 300 ms -> DOUBLE candidate
```

- [ ] **Step 9: Implement DOUBLE path**

Window is measured from first stable RELEASE to second stable PRESS; second RELEASE may occur after 300 ms.

- [ ] **Step 10: Add failing expired-window + new-press test**

At a second stable PRESS with elapsed >300 ms, verify the same process call:

```text
outputs previous SINGLE
transitions current press to FIRST_PRESS
```

Then release that new press and verify it can later produce a new SINGLE.

- [ ] **Step 11: Implement expired-window/new-press preservation**

Do not discard the new stable PRESS edge.

- [ ] **Step 12: Add failing LONG conflict tests**

Exact cases:

```text
2999 ms -> NONE
3000 ms -> LONG
5000 ms same hold -> no second LONG
LONG release -> NONE, never SINGLE
first short click + second press held 3000 ms -> LONG only
second release after LONG -> NONE
```

- [ ] **Step 13: Implement LONG / LONG_HOLD suppression**

- [ ] **Step 14: Add irregular interval + uint32 wraparound tests**

Use timestamps around `0xFFFFFFFFU`, crossing wrap while checking debounce, double timeout, and long threshold with subtraction arithmetic.

- [ ] **Step 15: Run Button Service test with warnings as errors**

Expected: all cases PASS without real delay or RTOS dependency.

- [ ] **Step 16: Run Platform Button / BSP Button / existing Indicator regression set**

- [ ] **Step 17: Coding Standard Review and focused commit**

Suggested commit:

```text
feat: add button gesture service
```

---

### Task 4: Keil Production Integration and Full Host Regression

**Files:**
- Modify: `RTT_elog_DMA_UART_ring_project.uvprojx`

**Production sources to add:**

```text
03_Platform/platform_bsp/button/platform_button.c
03_Platform/platform_bsp/button/platform_bsp_button.c
02_Service/service_button/service_button.c
```

Include paths must cover the new `platform_bsp/button` and `service_button` directories using the same project convention as existing LED / Indicator directories.

- [ ] **Step 1: Add only production Button sources / include paths to Keil**

Do not add `Tests/button_smoke` yet.

- [ ] **Step 2: Keil Full Rebuild normal production path**

Expected:

```text
0 errors
no new warnings from platform_button.c
no new warnings from platform_bsp_button.c
no new warnings from service_button.c
```

Pre-existing unrelated warnings may be recorded but must not be attributed to Phase 5.

- [ ] **Step 3: Run final Host regression before target smoke**

At minimum:

```text
platform_gpio
platform_bsp_gpio
platform_led
platform_bsp_led
service_indicator
platform_button
platform_bsp_button
service_button
Platform OS focused tests used by smoke
existing app / UART baseline tests normally run by the repository
```

- [ ] **Step 4: Confirm dependency scan**

Production Button files must contain no:

```text
HAL_
GPIO_TypeDef
GPIO_PIN_
osDelay
vTaskDelay
FreeRTOS handle
malloc
free
printf
```

`service_button.c` must not call `platform_button_read()` or `platform_time_get_ms()`.

- [ ] **Step 5: Coding Standard Review and focused commit**

Suggested commit:

```text
build: integrate button phase 5 production sources
```

---

### Task 5: Add Isolated FreeRTOS Button + Indicator Smoke Harness

**Files:**
- Create: `Tests/button_smoke/button_smoke.h`
- Create: `Tests/button_smoke/button_smoke.c`
- Create: `Tests/button_smoke/README.md`
- Temporarily Modify: `Core/Src/freertos.c` USER CODE sections only
- Temporarily Modify: `RTT_elog_DMA_UART_ring_project.uvprojx`

**Smoke-only public entry:**

```c
platform_error_t button_smoke_start(void);
```

**Smoke-owned static objects:**

```text
platform_button_t userButton
service_button_t buttonService
platform_led_t statusLed
service_indicator_t indicatorService
platform_queue_t indicatorQueue
platform_thread_t buttonThread
platform_thread_t indicatorThread
```

Smoke test constants remain local to `button_smoke.c`; do not add permanent Task stack / priority / queue depth to `project_config.h`.

- [ ] **Step 1: Implement `button_smoke_start()` initialization order**

Required order:

```text
construct User Key Button
platform_button_init
service_button_init
construct Status LED
platform_led_init -> OFF
service_indicator_init
platform_queue_create(queue, 4 items, sizeof(service_indicator_event_t))
platform_thread_create(Button Smoke Task)
platform_thread_create(Indicator Smoke Task)
print/log START + READY
```

If any step fails, log/print one failure record and return the actual error. Clean up objects that were already created where safe; do not invent a production lifecycle framework for smoke-only recovery.

- [ ] **Step 2: Implement Button Smoke Task**

Loop:

```text
platform_button_read
platform_time_get_ms
service_button_process
if event == NONE -> no log
if event != NONE:
    printf BUTTON_SMOKE,EVENT,<name>
    RTT Service Log event
    map event to Indicator event
    platform_queue_send
platform_time_delay_ms(PROJECT_BUTTON_SAMPLE_PERIOD_MS)
```

Mapping:

```text
SINGLE -> SERVICE_INDICATOR_EVENT_RUNNING
DOUBLE -> SERVICE_INDICATOR_EVENT_ONCE_SUCCESS
LONG   -> SERVICE_INDICATOR_EVENT_STOPPED
```

Only this task emits structured `printf` gesture markers so USART output is not produced concurrently by both smoke tasks.

- [ ] **Step 3: Implement Indicator Smoke Task**

Loop:

```text
platform_queue_receive(..., 1000 ms)
TIMEOUT -> continue
OK -> service_indicator_handle_event
error -> RTT ERROR + one structured failure path
```

The 600 ms ONCE_SUCCESS blink blocks only this Indicator task.

- [ ] **Step 4: Add README board-test procedure**

README must specify:

```text
USART1 = 115200 8N1
open Serial Assistant
open RTT Viewer
observe PC13 LED
startup expected OFF
single -> SINGLE + ON
double -> DOUBLE + 3 blinks + OFF
single -> SINGLE + ON
long >=3s -> LONG + OFF
```

Also list failure conditions: duplicate event, DOUBLE preceded by SINGLE, LONG followed by SINGLE, HardFault, RTT loss, UART regression.

- [ ] **Step 5: Add temporary smoke source / include path to Keil**

Keep the production group distinct from temporary test group if current project structure supports it.

- [ ] **Step 6: Add temporary `button_smoke_start()` hook to `freertos.c`**

Use a `USER CODE` section after normal Service Log / APP composition and USART1 mutex initialization, before scheduler start. Thread creation may happen before scheduler; actual smoke task execution must occur only after `osKernelStart()`.

Do not place gesture processing logic in `freertos.c`.

- [ ] **Step 7: Keil Full Rebuild with smoke path**

Expected: 0 errors, no new Phase 5 source warnings.

- [ ] **Step 8: Commit smoke harness separately**

Suggested commit:

```text
test: add button freertos smoke harness
```

This commit is temporary-test infrastructure and must later be cleaned from normal startup path.

---

### Task 6: Execute Target-Board Verification

**Required tools:**

```text
STM32 target board
Keil / J-Link
USART1 Serial Assistant at 115200 8N1
SEGGER RTT Viewer / RTT log window
PC13 LED visual observation
```

- [ ] **Step 1: Flash smoke-enabled firmware and verify startup**

Expected:

```text
BUTTON_SMOKE,START
BUTTON_SMOKE,READY
RTT smoke start / ready
LED OFF
no HardFault
```

- [ ] **Step 2: Single-click verification**

Action: one normal short click.

Expected:

```text
Serial: BUTTON_SMOKE,EVENT,SINGLE exactly once
RTT: single event exactly once
LED: ON and remains ON
no DOUBLE
no LONG
```

- [ ] **Step 3: Double-click verification**

Action: two normal short clicks within the recognition window.

Expected:

```text
Serial: BUTTON_SMOKE,EVENT,DOUBLE exactly once
no preceding/new SINGLE for this gesture
RTT: double event
LED: 3 x blink using existing Indicator Service, final OFF
```

- [ ] **Step 4: Long-press verification**

Action: press and hold >= 3 s, optionally continue to about 5 s, then release.

Expected:

```text
Serial: BUTTON_SMOKE,EVENT,LONG exactly once near threshold
RTT: long event exactly once
LED: OFF
no repeated LONG while held
no SINGLE after release
```

- [ ] **Step 5: Repeat practical bounce / rapid-operation smoke**

Perform several normal / light / rapid clicks. Expected: no obvious duplicate gesture events. Precise bounce boundaries remain Host-Test evidence.

- [ ] **Step 6: Existing UART communication regression**

Use the existing normal USART1 receive workflow during smoke and confirm the current communication path remains alive. Do not create a new UART protocol merely for Button testing.

- [ ] **Step 7: Record actual evidence**

Only mark target items PASS after real human observation. Record Serial Assistant, RTT and LED results in `Tests/button_smoke/README.md` or the implementation-plan completion section.

If board observation is not performed, status must remain:

```text
IMPLEMENTED / TARGET BOARD VERIFICATION PENDING
```

---

### Task 7: Remove Smoke Hook, Final Regression, and Handoff

**Files:**
- Modify: `Core/Src/freertos.c` — remove smoke include/start hook
- Modify: `RTT_elog_DMA_UART_ring_project.uvprojx` — remove temporary smoke source/include path
- Preserve: `Tests/button_smoke/*` as documented reusable test harness unless project convention requires removal; it must not be in normal product build/startup.
- Modify after real results: `00_Doc/04_Agent/handoff.md`
- Modify after real results: `00_Doc/04_Agent/development_roadmap.md`
- Update: `00_Doc/04_Agent/implementation_plan.md` checkboxes / completion record

- [ ] **Step 1: Remove all temporary normal-startup smoke hooks**

Normal `freertos.c` returns to production composition only.

- [ ] **Step 2: Remove smoke-only Keil production inclusion**

`Tests/button_smoke` must not compile into normal firmware.

- [ ] **Step 3: Normal-path Keil Full Rebuild**

Expected: 0 errors; no new Button production warnings.

- [ ] **Step 4: Run final Host regression set**

All new Button tests plus existing Platform GPIO / LED / Indicator / OS / UART / APP baselines must PASS.

- [ ] **Step 5: Final Coding Standard Review**

Check dependency boundaries, no temporary macros, no dead smoke hook, no raw HAL / RTOS calls in Button production modules.

- [ ] **Step 6: Update handoff with evidence, not assumptions**

If all target observations completed:

```text
Phase 5 — COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

Otherwise:

```text
Phase 5 — IMPLEMENTED / TARGET BOARD VERIFICATION PENDING
```

- [ ] **Step 7: Stop after Phase 5**

Do not begin DHT20 Phase 6 in the same implementation run.

Suggested final commit after evidence and cleanup:

```text
docs: complete button phase 5 handoff
```

---

# Final Acceptance Checklist

```text
Button_Phase1 design read                         [ ]
Platform Button lightweight object                [ ]
No platform_device_t for Button                   [ ]
No impl_button pass-through                       [ ]
User Key LOW active + PULL_UP config               [ ]
Button timing config 10/30/300/3000               [ ]
Platform Button Host Test                         [ ]
Platform BSP Button Host Test                     [ ]
Button Service Host Test                          [ ]
Debounce time-based                               [ ]
SINGLE delayed until double window expires        [ ]
DOUBLE has no preceding SINGLE                    [ ]
second PRESS <=300 ms boundary                    [ ]
expired second PRESS preserved as new gesture     [ ]
LONG >=3000 ms exactly once                       [ ]
LONG release no SINGLE                            [ ]
second press LONG conflict handled                 [ ]
uint32 wraparound tests                            [ ]
No real sleep in Host Service test                 [ ]
Keil production rebuild                           [ ]
FreeRTOS Button Smoke Task                         [ ]
FreeRTOS Indicator Smoke Task                      [ ]
Platform Queue used for smoke event delivery       [ ]
Platform Time used in smoke                        [ ]
Serial Assistant SINGLE                            [ ]
Serial Assistant DOUBLE                            [ ]
Serial Assistant LONG                              [ ]
RTT target smoke                                   [ ]
SINGLE -> LED ON                                   [ ]
DOUBLE -> 3 blinks -> OFF                          [ ]
LONG -> LED OFF                                    [ ]
No duplicate target gesture                        [ ]
Existing UART regression                           [ ]
No HardFault / obvious scheduling stall            [ ]
Temporary startup smoke removed                    [ ]
Normal-path Keil Full Rebuild                      [ ]
Final Host regression                              [ ]
Coding Standard Review                             [ ]
No Final APP Control FSM introduced                [ ]
No permanent Button Task frozen                    [ ]
No Button EXTI                                     [ ]
```

Phase 5 在完成真实板测与 cleanup 之前不得宣称完成；执行结束后停止在 Button Module，等待人工进入 Phase 6 设计流程。
