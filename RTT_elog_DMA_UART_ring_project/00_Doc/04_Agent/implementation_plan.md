# LED Phase 4 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> 当前执行计划 / Current Active Plan  
> 状态：COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED  
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

## Completion Record

Phase 4 已于 2026-09-03 完成。目标板 LED 行为与 RTT 由实际板测确认；用户随后确认本次 Phase 4 计划全部完成，因此既有 UART / PC Serial Assistant 通信回归记为 PASS。该确认只表示原有通信基线未被 Phase 4 破坏，不新增任何 UART 协议或测试路径。

下一阶段：`Phase 5 — Button Module (planning)`。在 Button 专项设计冻结并重新生成本文件前，不得直接开始 Phase 5 编码。

---

# Global Constraints

- 固定依赖方向保持 `APP -> Service -> Platform -> Impl -> Vendor`。
- 本 Phase 不新增正式 APP Control FSM 代码。
- Platform LED 是轻量对象，不使用 `platform_device_t`、device type、registry、manager 或 dynamic allocation。
- `platform_led_t` 与底层 GPIO 一对一，直接拥有自己的 `platform_gpio_t` 存储。
- LED STM32 硬件行为复用现有 Platform GPIO + STM32 GPIO Impl；禁止新增无真实职责的 `impl_led.c`。
- Status LED 物理 GPIO 绑定复用 `platform_bsp_gpio_construct_status_led()`，不得重复定义 PC13 / HAL Port / Pin。
- Status LED 有效电平和 Indicator 闪烁参数进入 `00_Config/project_config.h`。
- 静态参数：Status LED active level、blink count = 3、blink ON = 100 ms、blink OFF = 100 ms。
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

执行前已要求完整读取：

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

生产基线：

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

Preflight 结果：

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

---

### Task 1: Define Platform LED Contract and Static Configuration

**Files:**
- Create: `03_Platform/platform_bsp/led/platform_led.h`
- Create: `03_Platform/platform_bsp/led/platform_led.c`
- Modify: `00_Config/project_config.h`
- Create: `Tests/platform_led/test_platform_led.c`

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
- [x] **Step 2: Add Phase 4 static configuration**
- [x] **Step 3: Implement minimal lightweight Platform LED object**
- [x] **Step 4: Run focused Platform LED Host tests**
- [x] **Step 5: Run existing Platform GPIO regression**
- [x] **Step 6: Coding Standard Review and focused commit**

Result: PASS.

---

### Task 2: Add Status LED Board/BSP Construction

**Files:**
- Create: `03_Platform/platform_bsp/led/platform_bsp_led.h`
- Create: `03_Platform/platform_bsp/led/platform_bsp_led.c`
- Create: `Tests/platform_bsp_led/test_platform_bsp_led.c`

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
- [x] **Step 2: Implement minimal BSP LED composition**
- [x] **Step 3: Run BSP LED Host test and GPIO BSP regression**
- [x] **Step 4: Coding Standard Review and focused commit**

Result: PASS.

---

### Task 3: Implement Indicator Service Event Semantics

**Files:**
- Create: `02_Service/service_indicator/service_indicator.h`
- Create: `02_Service/service_indicator/service_indicator.c`
- Create: `Tests/service_indicator/test_service_indicator.c`

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
- [x] **Step 2: Implement minimal event-driven Indicator Service**
- [x] **Step 3: Run Indicator Service Host tests**
- [x] **Step 4: Run Platform LED + Platform GPIO regression set**
- [x] **Step 5: Coding Standard Review and focused commit**

Result: PASS.

---

### Task 4: Keil Integration and Compile Verification

**Files:**
- Modify: `RTT_elog_DMA_UART_ring_project.uvprojx` only for required new production source/header groups.

- [x] **Step 1: Add production LED and Indicator sources to Keil project**
- [x] **Step 2: Keil Full Rebuild**
- [x] **Step 3: Resolve integration-only issues without changing frozen architecture**
- [x] **Step 4: Coding Standard Review and focused commit**

Result:

```text
Keil Full Rebuild = PASS / 0 errors
Phase 4 source warning scan = PASS
```

---

### Task 5: FreeRTOS Target-Board Indicator Smoke Verification

**Verification context:**

```text
FreeRTOS scheduler started
Task Context
platform_time_delay_ms()
```

Smoke sequence：

```text
START
STOPPED / OFF      hold about 1 s
RUNNING / ON       hold about 2 s
STOPPED / OFF      hold about 1 s
ONCE_SUCCESS       3 x (100 ms ON + 100 ms OFF)
FINAL OFF
PASS / FAIL
```

- [x] **Step 1: Add an isolated temporary smoke path**
- [x] **Step 2: Add low-frequency RTT smoke observability**
- [x] **Step 3: Keil Full Rebuild with smoke path**
- [x] **Step 4: Target board LED visual verification**
- [x] **Step 5: RTT observation**
- [x] **Step 6: Existing communication regression observation**
- [x] **Step 7: Record target evidence before cleanup**

Target evidence：

```text
LED visual OFF / ON / 3 blink / final OFF      PASS — user-confirmed target observation
RTT stage sequence                              PASS — user-provided RTT observation
Existing UART / PC Serial Assistant regression  PASS — user confirmed Phase 4 plan fully completed
Logic Analyzer                                  NOT REQUIRED
```

---

### Task 6: Remove Smoke Harness, Final Regression, and Handoff

- [x] **Step 1: Remove all temporary smoke-only paths**
- [x] **Step 2: Run normal-path Keil Full Rebuild**
- [x] **Step 3: Run final Host regression set**
- [x] **Step 4: Final Coding Standard Review**
- [x] **Step 5: Update handoff with actual implementation / verification evidence**
- [x] **Step 6: Stop after Phase 4**

Result：

```text
Temporary smoke path                  REMOVED
Normal-path Keil Full Rebuild         PASS / 0 errors
Final Host regression                 PASS
Coding Standard Review                PASS
Phase 4 final status                  COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

---

# Final Acceptance Checklist

```text
LED_Phase1 design read                         PASS
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
Platform BSP LED Host Test                     PASS
Indicator Service Host Test                    PASS
Platform GPIO / BSP GPIO regression            PASS
Keil Full Rebuild                              PASS
Target OFF / ON / 3 blink / OFF                PASS
RTT target smoke                               PASS
Communication regression                       PASS — user confirmed Phase 4 plan complete
Logic Analyzer                                 NOT REQUIRED
Temporary smoke removed                        PASS
Normal-path Keil Full Rebuild                  PASS
Coding Standard Review                         PASS
No Final APP Control FSM introduced            PASS
No permanent Indicator Task introduced         PASS
```

Phase 4 正式关闭。下一步回到设计流程，进入 `Phase 5 — Button Module` 专项设计；在设计冻结前不修改 Button 生产代码。
