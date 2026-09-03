# LED Phase 4 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> 当前执行计划 / Current Active Plan  
> 状态：READY FOR IMPLEMENTATION  
> 日期：2026-09-03

**Goal:** 在现有 Platform GPIO / STM32 GPIO Impl / Board GPIO Binding 基线上，实现轻量 Platform LED 与 Indicator Service，并完成 Host、Keil 和 FreeRTOS Task Context 目标板 Smoke 验证。

**Architecture:** Phase 4 只实现 `Indicator Service -> Platform LED -> Platform GPIO -> STM32 GPIO Impl`。LED 不接入 `platform_device_t`，不新增 `impl_led` 透传层；最终独立 Indicator Task 已确定为后续 RTOS 架构方向，但其永久创建、优先级、栈大小和事件投递机制留到 Phase 9。

**Tech Stack:** C、STM32F411CEU6、STM32 HAL GPIO（仅既有 Impl 使用）、CMSIS-RTOS2 + FreeRTOS、Platform GPIO、Platform Time、EasyLogger + SEGGER RTT、Host C Test、Keil MDK-ARM、PC Serial Assistant。

**Spec:**
- `00_Doc/02_架构设计/LED_Phase1设计.md`
- `00_Doc/00_项目需求/最终功能需求.md`
- `00_Doc/04_Agent/architecture.md`
- `00_Doc/04_Agent/development_roadmap.md` Phase 4
- `00_Doc/04_Agent/handoff.md`

---

# Global Constraints

- 固定依赖方向保持 `APP -> Service -> Platform -> Impl -> Vendor`。
- 本 Phase 不新增正式 APP Control FSM 代码。
- Platform LED 是轻量对象，不使用 `platform_device_t`、device type、registry、manager 或 dynamic allocation。
- `platform_led_t` 与底层 GPIO 一对一，直接拥有自己的 `platform_gpio_t` 存储。
- LED STM32 硬件行为复用现有 Platform GPIO + STM32 GPIO Impl；禁止新增无真实职责的 `impl_led.c`。
- Status LED 物理 GPIO 绑定复用 `platform_bsp_gpio_construct_status_led()`，不得重复定义 PC13 / HAL Port / Pin。
- Status LED 有效电平和 Indicator 闪烁参数进入 `00_Config/project_config.h`。
- 计划静态参数：Status LED active level、blink count = 3、blink ON = 100 ms、blink OFF = 100 ms。
- Indicator Service 只消费 `STOPPED / RUNNING / ONCE_SUCCESS` 提示语义，不维护 APP 真实采集状态。
- `ONCE_SUCCESS` 是否成立由未来 APP / UART TX completion 语义决定，本 Phase 不建立正式 TX Complete 接线。
- Indicator Service 三闪使用 `platform_time_delay_ms()`；不得直接调用 `HAL_Delay()`、`osDelay()`、`vTaskDelay()`。
- LED 闪烁只允许在 Task Context；ISR / HAL Callback 禁止阻塞闪烁。
- 本 Phase 不正式创建永久 Indicator Task；永久 Task 设计留到 Phase 9。
- Host Test 不真实等待 100 ms × 6，必须 fake / stub Platform Time。
- 正常运行路径禁止逐 LED ON/OFF 边沿 RTT 日志。
- 目标板 Smoke 不要求逻辑分析仪；使用 LED 肉眼、RTT、串口助手通信回归观察。
- 临时 Smoke 代码必须集中、可识别、可完整移除；清理后再执行正常路径 Keil Full Rebuild。
- 所有自研代码执行前必须完整读取 `00_Doc/02_架构设计/嵌入式项目C代码设计规范.md`。
- 每个 Task 提交前执行 Coding Standard Review。
- 若冻结设计与仓库现实存在实质冲突：`STOP / BLOCKED`，不得静默重设计。

---

# 0. Mandatory Preflight

执行前必须完整读取：

```text
00_Doc/00_项目需求/最终功能需求.md
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

检查生产基线：

```text
00_Config/project_config.h
03_Platform/platform_mcu/gpio/platform_gpio.h
03_Platform/platform_mcu/gpio/platform_gpio_types.h
03_Platform/platform_mcu/gpio/platform_gpio.c
03_Platform/platform_bsp/platform_bsp_gpio.h
04_Impl/impl_mcu/impl_platform_gpio.c
04_Impl/impl_bsp/impl_platform_bsp_gpio.c
03_Platform/platform_os/platform_time.h
02_Service/service_log/service_log.h
Core/Src/freertos.c
01_APP/app_system.c
RTT_elog_DMA_UART_ring_project.uvprojx
```

Preflight 必须确认：

```text
Phase 3                                   COMPLETED / TARGET SMOKE VERIFIED
PC13 Status LED                           VERIFIED
Status LED GPIO constructor               PRESENT
Platform GPIO public contract             STABLE
platform_time_delay_ms()                  PRESENT / Task Context
FreeRTOS scheduler                        ACTIVE IN NORMAL FIRMWARE
Current APP communication                 PRESERVED
Unrelated user changes                    PRESERVED
```

固定汇报：

```text
LED Frozen Design: READ
Coding Standard: READ
Agent Execution Rules: READ
Current repository state: INSPECTED
Phase 3 baseline: VERIFIED
Unrelated user changes: PRESERVED
```

---

### Task 1: Define Platform LED Contract and Static Configuration

**Files:**
- Create: `03_Platform/platform_bsp/led/platform_led.h`
- Create: `03_Platform/platform_bsp/led/platform_led.c`
- Modify: `00_Config/project_config.h`
- Create: `Tests/platform_led/test_platform_led.c`
- Modify host test build script / project only as required by the existing test pattern.

**Produces:**

```text
platform_led_t lightweight object
platform_led_init
platform_led_on
platform_led_off
platform_led_toggle
platform_led_deinit
PROJECT_STATUS_LED_ACTIVE_LEVEL
PROJECT_INDICATOR_BLINK_COUNT
PROJECT_INDICATOR_BLINK_ON_MS
PROJECT_INDICATOR_BLINK_OFF_MS
```

- [x] **Step 1: Write failing Host contract tests**

Test coverage must include:

```text
zero-initialized object lifecycle
NULL parameter rejection
operation before hardware init rejection
initialization configures GPIO output
initialization establishes LED OFF
active-low ON mapping
active-low OFF mapping
toggle changes physical output appropriately
deinit behavior
underlying GPIO configure/write/read/deinit error propagation as applicable
```

Expected RED condition: Platform LED files / symbols do not yet exist.

- [x] **Step 2: Add Phase 4 static configuration**

Add only the frozen LED settings to `project_config.h`.

Requirements:

```text
Status LED active level = LOW on current board
blink count = 3
blink ON = 100 ms
blink OFF = 100 ms
```

Do not add PC13, GPIOC or HAL GPIO pin macros to Config.

- [x] **Step 3: Implement minimal lightweight Platform LED object**

Implementation must:

```text
own embedded platform_gpio_t storage
store active-level semantics
avoid platform_device_t
avoid ops table / registry
avoid malloc/free
translate semantic ON/OFF to GPIO level
establish OFF during init
```

Do not add blink policy or product states here.

- [x] **Step 4: Run focused Platform LED Host tests**

Expected:

```text
all Platform LED tests PASS
no HAL dependency in Platform LED test build
```

- [x] **Step 5: Run existing Platform GPIO regression**

Expected:

```text
existing Platform GPIO Host tests PASS
```

- [x] **Step 6: Coding Standard Review and focused commit**

Review: naming, object lifecycle, ownership, active-level semantics, no device-model overbuild, no HAL leakage.

Suggested commit scope:

```text
platform_led files
project_config.h
Tests/platform_led
required host test build metadata
```

---

### Task 2: Add Status LED Board/BSP Construction

**Files:**
- Create: `03_Platform/platform_bsp/led/platform_bsp_led.h`
- Create: `03_Platform/platform_bsp/led/platform_bsp_led.c`
- Create: `Tests/platform_bsp_led/test_platform_bsp_led.c`
- Modify host test build script / project only as required.

**Consumes:**

```text
platform_led_t
PROJECT_STATUS_LED_ACTIVE_LEVEL
platform_bsp_gpio_construct_status_led()
```

**Produces:**

```text
platform_bsp_led_construct_status_led()
```

- [x] **Step 1: Write failing BSP composition test**

Test must prove that Status LED construction:

```text
uses existing Status LED GPIO constructor
applies configured active level
constructs only the abstract object
performs no direct HAL access
performs no product-state behavior
```

Expected RED condition: BSP LED constructor does not yet exist.

- [x] **Step 2: Implement minimal BSP LED composition**

Requirements:

```text
reuse platform_bsp_gpio_construct_status_led()
no duplicated PC13 binding
no new impl_led layer
no GPIO hardware configure in constructor unless frozen Platform LED lifecycle explicitly requires it
```

Keep “construct / bind” and “hardware init” semantics distinct.

- [x] **Step 3: Run BSP LED Host test and GPIO BSP regression**

Expected:

```text
Platform BSP LED test PASS
existing Platform BSP GPIO test PASS
```

- [x] **Step 4: Coding Standard Review and focused commit**

Review: Board/BSP boundary, no HAL leakage, no duplicate resource source, constructor lifecycle consistency.

---

### Task 3: Implement Indicator Service Event Semantics

**Files:**
- Create: `02_Service/service_indicator/service_indicator.h`
- Create: `02_Service/service_indicator/service_indicator.c`
- Create: `Tests/service_indicator/test_service_indicator.c`
- Modify host test build script / project only as required.

**Consumes:**

```text
platform_led_t
platform_led_on / off
platform_time_delay_ms
PROJECT_INDICATOR_BLINK_COUNT
PROJECT_INDICATOR_BLINK_ON_MS
PROJECT_INDICATOR_BLINK_OFF_MS
```

**Produces:**

```text
SERVICE_INDICATOR_EVENT_STOPPED
SERVICE_INDICATOR_EVENT_RUNNING
SERVICE_INDICATOR_EVENT_ONCE_SUCCESS
service_indicator_init
service_indicator_handle_event
service_indicator_deinit
```

- [x] **Step 1: Write failing Indicator Service Host tests**

Cover:

```text
NULL / invalid lifecycle
STOPPED -> OFF
RUNNING -> ON
ONCE_SUCCESS -> exactly 3 ON phases
ONCE_SUCCESS -> exactly 3 OFF phases
ONCE_SUCCESS -> final OFF
configured ON/OFF delay values requested
Platform LED error propagation
Platform Time error propagation
no APP running-state ownership
```

Time must be fake / stubbed; Host tests must not wait real 600 ms.

- [x] **Step 2: Implement minimal event-driven Indicator Service**

Requirements:

```text
single handle_event entry for semantic events
no duplicate set_running/set_stopped public API family
no RTOS queue inside Service
no permanent Task inside Service
no UART TX decision inside Service
no HAL / FreeRTOS direct calls
```

`ONCE_SUCCESS` may use sequential `platform_time_delay_ms()` because final design assigns this behavior to a dedicated Indicator Task Context.

- [x] **Step 3: Run Indicator Service Host tests**

Expected:

```text
all Indicator Service tests PASS
no real wall-clock blink delay in Host test
```

- [x] **Step 4: Run Platform LED + Platform GPIO regression set**

Expected all PASS.

- [x] **Step 5: Coding Standard Review and focused commit**

Review: Service/Platform boundary, error handling, event semantics, no hidden APP state, Platform Time use.

---

### Task 4: Keil Integration and Compile Verification

**Files:**
- Modify: `RTT_elog_DMA_UART_ring_project.uvprojx` only for new production source/header groups required by the build.
- Do not add permanent Phase 9 Indicator Task code.

- [x] **Step 1: Add production LED and Indicator sources to the existing Keil grouping convention**

Preserve unrelated project settings.

- [x] **Step 2: Keil Full Rebuild**

Expected:

```text
0 errors
no new warnings from Phase 4 sources
existing communication firmware still builds
```

- [x] **Step 3: Resolve integration-only issues without changing frozen architecture**

If resolution would require any of the following, STOP and return to design review:

```text
new impl_led layer
platform_device_t for LED
APP Control FSM implementation
permanent Indicator Task implementation
HAL_Delay inside Service
rewriting existing GPIO public contract
```

- [x] **Step 4: Coding Standard Review and focused commit**

---

### Task 5: FreeRTOS Target-Board Indicator Smoke Verification

**Files:**
- Create a dedicated temporary smoke source / hook only if it keeps validation isolated.
- Modify `Core/Src/freertos.c` USER CODE region only if needed for the temporary test entry.
- Modify Keil project only if a temporary smoke source is added.
- Do not modify CubeMX-generated non-USER CODE sections.

**Verification context:**

```text
FreeRTOS scheduler started
Task Context
platform_time_delay_ms()
```

- [ ] **Step 1: Add an isolated temporary smoke path**

Smoke sequence:

```text
START
STOPPED / OFF      hold about 1 s
RUNNING / ON       hold about 2 s
STOPPED / OFF      hold about 1 s
ONCE_SUCCESS       3 x (100 ms ON + 100 ms OFF)
FINAL OFF
PASS / FAIL
```

Do not use `HAL_Delay()`.

- [ ] **Step 2: Add low-frequency RTT smoke observability**

Log only major stages:

```text
indicator smoke start
STOPPED
RUNNING
STOPPED
ONCE_SUCCESS
indicator smoke pass / fail
```

Do not log every ON/OFF edge.

- [ ] **Step 3: Keil Full Rebuild with smoke path**

Expected: 0 errors.

- [ ] **Step 4: Target board LED visual verification**

Must confirm:

```text
STOPPED = physically OFF
RUNNING = physically ON
ONCE_SUCCESS = visible 3 blinks
final state = OFF
```

- [ ] **Step 5: RTT observation**

RTT stage log must match the visible LED sequence.

- [ ] **Step 6: Existing communication regression observation**

Use PC Serial Assistant and/or existing communication behavior to confirm LED smoke does not break the current UART communication baseline.

Do not create a new UART protocol or echo path solely for LED testing if the existing firmware does not require one.

- [ ] **Step 7: Record target evidence before cleanup**

Record:

```text
Keil smoke build result
visual OFF / ON / 3-blink / final OFF result
RTT stage result
communication regression result
```

No logic analyzer evidence required for this Phase.

---

### Task 6: Remove Smoke Harness, Final Regression, and Handoff

**Files:**
- Remove / revert temporary smoke-only source and hook.
- Restore `Core/Src/freertos.c` normal USER CODE path if modified.
- Remove temporary smoke source from Keil project if added.
- Modify: `00_Doc/04_Agent/handoff.md`
- Update `00_Doc/04_Agent/implementation_plan.md` checkboxes / status as execution progresses.

- [ ] **Step 1: Remove all temporary smoke-only paths**

Confirm no test loop remains in normal production execution.

- [ ] **Step 2: Run normal-path Keil Full Rebuild**

Expected:

```text
0 errors
normal communication startup restored
```

- [ ] **Step 3: Run final Host regression set**

At minimum:

```text
Platform LED
Platform BSP LED
Indicator Service
Platform GPIO
Platform BSP GPIO
```

- [ ] **Step 4: Final Coding Standard Review**

Must report:

```text
Coding Standard Review: PASS / NEEDS_FIX / EXCEPTION
```

- [ ] **Step 5: Update handoff with actual implementation / verification evidence**

Only after evidence exists, record Phase 4 status.

If target verification is incomplete:

```text
Phase 4 — IMPLEMENTED / TARGET BOARD VERIFICATION PENDING
```

If all completion gates pass:

```text
Phase 4 — COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

- [ ] **Step 6: Stop after Phase 4**

Do not automatically begin Button Phase 5. Return implementation result for design/review handoff.

---

# Final Acceptance Checklist

```text
LED_Phase1 design read                         REQUIRED
Platform LED lightweight object               PASS
No platform_device_t for LED                   PASS
No new impl_led pass-through                   PASS
Status LED active level config                 PASS
Indicator blink static config                  PASS
Status LED BSP composition                     PASS
Indicator Service event API                    PASS
STOPPED -> OFF Host behavior                   PASS
RUNNING -> ON Host behavior                    PASS
ONCE_SUCCESS -> 3 blinks -> OFF Host behavior  PASS
Platform Time abstraction used                 PASS
No HAL_Delay in Service/smoke Task             PASS
Platform LED Host Test                         PASS
Indicator Service Host Test                    PASS
GPIO regression                                PASS
Keil Full Rebuild                              PASS
Target OFF / ON / 3 blink / OFF                PASS
RTT target smoke                               PASS
Communication regression                       PASS
Logic Analyzer                                 NOT REQUIRED
Temporary smoke removed                        PASS
Normal-path Keil Full Rebuild                  PASS
Coding Standard Review                         PASS
No Final APP Control FSM introduced            PASS
No permanent Indicator Task introduced         PASS
```
