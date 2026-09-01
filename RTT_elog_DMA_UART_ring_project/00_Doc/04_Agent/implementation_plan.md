# GPIO STM32 Impl Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> 当前执行计划 / Current Active Plan  
> 状态：HOST VERIFIED / BLOCKED ON KEIL
> 日期：2026-09-01

> 实际执行记录：Task 1-4 已完成并通过 Host / 边界验证；Task 5 已完成 Keil 工程集成，环境未发现 `UV4.exe`，真实 Keil Full Rebuild 未执行；Task 6 记录为 Host Verified，Phase 1 Closure BLOCKED ON KEIL GATE。

**Goal:** 为已 Host Verified 的 Platform GPIO 提供通用 STM32F411 + HAL 实现，并完成 Host Fake-HAL 验证、Platform 回归、Keil 编译和代码规范审查。

**Architecture:** 使用 Generic STM32 GPIO Impl + caller-owned Context。每个 `platform_gpio_t` 通过 `implContext` 单向引用 `{GPIO_TypeDef *port, uint16_t pin}`；Impl 不维护 Registry / Context Pool，不知道 LED / KEY / SCL / SDA。GPIO Port RCC 由 CubeMX / Board Bootstrap 负责，本 Phase 不做具体 Board Binding 或目标板 GPIO Smoke Test。

**Tech Stack:** C、STM32F411 HAL GPIO、现有 Platform GPIO、Host C test + Fake HAL、Keil MDK-ARM。

**Spec:** `00_Doc/02_架构设计/GPIO_STM32_Impl_Phase1设计.md`

## Global Constraints

- 固定依赖方向保持 `APP -> Service -> Platform -> Impl -> Vendor`。
- 不修改 Platform GPIO 已冻结公共 API / 类型。
- `platform_gpio_t` 继续是轻量 MCU Resource，不继承 `platform_device_t`。
- Context 必须 caller-owned；禁止 Context Pool、Registry、动态内存和固定 per-pin 全局实例。
- 一个 Context 只能绑定一个物理 GPIO Pin；multi-bit Pin mask 必须拒绝。
- GPIO Impl 不负责 RCC Enable / Disable；对应 Port Clock 是调用 `platform_gpio_configure()` 前置条件。
- STM32 GPIO Speed 固定为 Impl-private `GPIO_SPEED_FREQ_LOW`，不得增加 Platform Speed API。
- OUTPUT 配置必须先写 `initialLevel`，再调用 `HAL_GPIO_Init()`。
- INPUT 配置不得因 `initialLevel` 字段而调用 `HAL_GPIO_WritePin()`。
- 不加入 EXTI / IRQ / NVIC / callback / Toggle / AF / Analog。
- 不加入 LED / KEY / Debounce / Software I2C / Sensor / APP FSM。
- 不修改具体 CubeMX GPIO Pin 配置；Board Resource + CubeMX 属于 Phase 2。
- Production 和 Test 自研代码均遵守 `00_Doc/02_架构设计/嵌入式项目C代码设计规范.md`。
- 发现冻结设计与仓库现实存在实质冲突时：`STOP / BLOCKED`，不得静默重设计。

---

# 0. Mandatory Preflight

执行前必须完整读取：

```text
00_Doc/00_项目需求/最终功能需求.md
00_Doc/02_架构设计/GPIO_STM32_Impl_Phase1设计.md
00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/implementation_plan.md
```

检查参考实现：

```text
03_Platform/platform_mcu/gpio/platform_gpio_types.h
03_Platform/platform_mcu/gpio/platform_gpio.h
03_Platform/platform_mcu/gpio/platform_gpio.c
04_Impl/impl_mcu/impl_platform_uart.h
04_Impl/impl_mcu/impl_platform_uart.c
Tests/platform_gpio/
Tests/impl_platform_uart/
Core/Src/gpio.c
Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_gpio.h
```

Preflight 固定汇报：

```text
GPIO STM32 Impl Phase 1 Design: READ
Platform GPIO Frozen Design: READ
Development Roadmap: READ
Coding Standard:
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
Status: READ
Agent Execution Rules: READ
Current repository state: INSPECTED
Unrelated user changes: PRESERVED
```

执行前确认：

```text
04_Impl/impl_mcu/impl_platform_gpio.* does not contain conflicting user code
Tests/impl_platform_gpio/ does not contain conflicting current tests
Platform GPIO public contract matches frozen design
Core/Src/gpio.c remains CubeMX/bootstrap-owned
```

若有冲突，停止并报告。

---

# 1. Frozen Interfaces

本 Phase 生产接口冻结为：

```c
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} impl_platform_gpio_context_t;

platform_error_t impl_platform_gpio_construct(
    platform_gpio_t *gpio,
    const char *name,
    impl_platform_gpio_context_t *context);
```

冻结的内部 Ops：

```text
configure
write
read
deinit
```

`impl_platform_gpio_construct()` 只绑定对象，不调用任何 HAL GPIO API。

生产文件：

```text
04_Impl/impl_mcu/impl_platform_gpio.h
04_Impl/impl_mcu/impl_platform_gpio.c
```

测试文件：

```text
Tests/impl_platform_gpio/test_impl_platform_gpio.c
Tests/impl_platform_gpio/stm32f4xx_hal.h   # Host Fake HAL if this matches test include strategy
```

如果现有 Host Test 基础设施要求不同的最小 Fake HAL 文件名，可沿用仓库既有测试模式，但不得修改 Production Header 来迎合测试。

---

### Task 1: Define STM32 GPIO Context and Construct Binding

**Files:**
- Create: `04_Impl/impl_mcu/impl_platform_gpio.h`
- Create: `04_Impl/impl_mcu/impl_platform_gpio.c`
- Create: `Tests/impl_platform_gpio/test_impl_platform_gpio.c`
- Create as required by host build: `Tests/impl_platform_gpio/stm32f4xx_hal.h`

**Interfaces:**
- Consumes: `platform_gpio_t`, `platform_gpio_init_params_t`, `platform_gpio_init()`.
- Produces: `impl_platform_gpio_context_t` and `impl_platform_gpio_construct()`.

- [x] **Step 1: Write failing construct/context tests**

Tests must verify at least:

```c
/* valid single-pin context */
impl_platform_gpio_context_t context = {
    fakePort,
    GPIO_PIN_3
};
platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;

/* construct succeeds */
result = impl_platform_gpio_construct(&gpio, "test_gpio", &context);
ASSERT_EQ(PLATFORM_ERR_OK, result);
ASSERT_TRUE(gpio.initialized != 0U);
ASSERT_TRUE(gpio.configured == 0U);
ASSERT_EQ(&context, gpio.implContext);

/* construct performs no HAL operation */
ASSERT_EQ(0U, fakeHalInitCount);
ASSERT_EQ(0U, fakeHalWriteCount);
ASSERT_EQ(0U, fakeHalReadCount);
ASSERT_EQ(0U, fakeHalDeinitCount);
```

Also cover:

```text
NULL gpio          -> PLATFORM_ERR_INVALID_PARAM
NULL context       -> PLATFORM_ERR_INVALID_PARAM
context->port NULL -> PLATFORM_ERR_INVALID_PARAM
pin = 0            -> PLATFORM_ERR_INVALID_PARAM
multi-bit pin      -> PLATFORM_ERR_INVALID_PARAM
single valid pin   -> success
```

- [x] **Step 2: Run the focused Host Test and verify RED**

Use the repository's existing Host C test build pattern. Expected failure: missing `impl_platform_gpio.h` / missing construct implementation.

- [x] **Step 3: Implement the minimal private validation and construct path**

Header must define exactly:

```c
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} impl_platform_gpio_context_t;

platform_error_t impl_platform_gpio_construct(
    platform_gpio_t *gpio,
    const char *name,
    impl_platform_gpio_context_t *context);
```

Production `.c` must include a private single-pin check equivalent to:

```c
static platform_bool_t stm32_gpio_is_single_pin(uint16_t pin)
{
    if (pin == 0U) {
        return PLATFORM_FALSE;
    }

    return (((uint16_t)(pin & (uint16_t)(pin - 1U))) == 0U) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}
```

Construct logic:

```c
if ((gpio == NULL) || (context == NULL) ||
    (context->port == NULL) ||
    (stm32_gpio_is_single_pin(context->pin) != PLATFORM_TRUE)) {
    return PLATFORM_ERR_INVALID_PARAM;
}

params.name = name;
params.ops = &g_stm32GpioOps;
params.implContext = context;
return platform_gpio_init(gpio, &params);
```

Do not mutate `context->port` or `context->pin` inside construct.

- [x] **Step 4: Run construct/context tests and verify GREEN**

Expected: all Task 1 tests PASS and HAL fake call counts remain zero after construct.

- [x] **Step 5: Coding-standard micro-review**

Check file headers, include order, naming, static/private function placement, Doxygen on public Impl API, no TAB, no unrelated code.

- [x] **Step 6: Commit**

Suggested commit:

```bash
git add RTT_elog_DMA_UART_ring_project/04_Impl/impl_mcu/impl_platform_gpio.* \
        RTT_elog_DMA_UART_ring_project/Tests/impl_platform_gpio/
git commit -m "feat: add STM32 GPIO impl binding"
```

---

### Task 2: Implement Platform Config to HAL Mapping and Configure Ordering

**Files:**
- Modify: `04_Impl/impl_mcu/impl_platform_gpio.c`
- Modify: `Tests/impl_platform_gpio/test_impl_platform_gpio.c`
- Modify Fake HAL stub only as required for call capture.

**Interfaces:**
- Consumes: `platform_gpio_config_t`, caller-owned Context.
- Produces: STM32 implementation of `platform_gpio_ops_t.configure`.

- [x] **Step 1: Write failing mapping tests**

Capture `GPIO_InitTypeDef` passed to Fake `HAL_GPIO_Init()` and verify:

```text
INPUT                  -> GPIO_MODE_INPUT
OUTPUT + PUSH_PULL     -> GPIO_MODE_OUTPUT_PP
OUTPUT + OPEN_DRAIN    -> GPIO_MODE_OUTPUT_OD
PULL_NONE              -> GPIO_NOPULL
PULL_UP                -> GPIO_PULLUP
PULL_DOWN              -> GPIO_PULLDOWN
Speed                   = GPIO_SPEED_FREQ_LOW for all supported configs
Pin                     = context->pin
Alternate               = deterministic safe value (0U)
```

- [x] **Step 2: Write failing initial-level ordering tests**

Fake HAL must record call sequence.

For OUTPUT LOW:

```text
call[0] = WRITE(port, pin, GPIO_PIN_RESET)
call[1] = INIT(port, mapped GPIO_InitTypeDef)
```

For OUTPUT HIGH:

```text
call[0] = WRITE(port, pin, GPIO_PIN_SET)
call[1] = INIT(...)
```

For INPUT:

```text
INIT only
WRITE count = 0
```

- [x] **Step 3: Run focused tests and verify RED**

Expected: configure Ops absent or mapping/order assertions fail.

- [x] **Step 4: Implement context retrieval and mapping helpers**

Use focused private helpers such as:

```c
static platform_error_t stm32_gpio_get_context(
    platform_gpio_t *gpio,
    impl_platform_gpio_context_t **context);

static platform_error_t stm32_gpio_map_mode(
    const platform_gpio_config_t *config,
    uint32_t *mode);

static platform_error_t stm32_gpio_map_pull(
    platform_gpio_pull_t pull,
    uint32_t *halPull);

static GPIO_PinState stm32_gpio_map_level(platform_gpio_level_t level);
```

`stm32_gpio_get_context()` minimum behavior:

```text
gpio == NULL or out-context == NULL -> PLATFORM_ERR_INVALID_PARAM
implContext == NULL                 -> PLATFORM_ERR_NOT_INITIALIZED
context->port == NULL               -> PLATFORM_ERR_NOT_INITIALIZED
context->pin not single-bit         -> PLATFORM_ERR_INVALID_PARAM
```

Do not copy Platform lifecycle/state checks already owned by `platform_gpio.c`.

- [x] **Step 5: Implement configure Ops**

Use deterministic zero initialization:

```c
GPIO_InitTypeDef halConfig = {0};
```

Then:

```text
Pin       = context->pin
Mode      = mapped mode
Pull      = mapped pull
Speed     = GPIO_SPEED_FREQ_LOW
Alternate = 0U
```

OUTPUT sequence must be exactly:

```c
HAL_GPIO_WritePin(context->port,
                  context->pin,
                  stm32_gpio_map_level(config->initialLevel));
HAL_GPIO_Init(context->port, &halConfig);
```

INPUT must call only `HAL_GPIO_Init()`.

HAL API returns `void`; successful call path returns `PLATFORM_ERR_OK`.

- [x] **Step 6: Run Task 2 tests and verify GREEN**

Expected: all config mappings and call-order assertions PASS.

- [x] **Step 7: Run existing Platform GPIO Host Regression**

Run the existing `Tests/platform_gpio/` suite with no changes to frozen Platform behavior. Expected: PASS.

- [x] **Step 8: Commit**

Suggested commit:

```bash
git add RTT_elog_DMA_UART_ring_project/04_Impl/impl_mcu/impl_platform_gpio.c \
        RTT_elog_DMA_UART_ring_project/Tests/impl_platform_gpio/
git commit -m "feat: map Platform GPIO config to STM32 HAL"
```

---

### Task 3: Implement GPIO Write, Read, and Deinit Ops

**Files:**
- Modify: `04_Impl/impl_mcu/impl_platform_gpio.c`
- Modify: `Tests/impl_platform_gpio/test_impl_platform_gpio.c`
- Modify Fake HAL stub as required.

**Interfaces:**
- Consumes: validated Platform calls and STM32 Context.
- Produces: STM32 implementation of `write`, `read`, `deinit` Ops.

- [x] **Step 1: Write failing write tests**

Verify:

```text
PLATFORM_GPIO_LEVEL_LOW
    -> HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET)

PLATFORM_GPIO_LEVEL_HIGH
    -> HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET)
```

Also verify correct `port` and `pin` forwarding.

- [x] **Step 2: Write failing read tests**

Configure Fake HAL return values and verify:

```text
GPIO_PIN_RESET -> PLATFORM_GPIO_LEVEL_LOW
GPIO_PIN_SET   -> PLATFORM_GPIO_LEVEL_HIGH
```

Verify correct `port / pin` passed to `HAL_GPIO_ReadPin()`.

- [x] **Step 3: Write failing deinit tests**

Verify:

```text
HAL_GPIO_DeInit(context->port, context->pin)
```

and through public Platform API:

```text
platform_gpio_deinit() success
    -> gpio.configured == 0U
    -> gpio.initialized remains true
```

No RCC Fake API must be invoked.

- [x] **Step 4: Run focused tests and verify RED**

Expected: missing Ops implementations or forwarding assertions fail.

- [x] **Step 5: Implement minimal write/read/deinit Ops**

Write:

```c
HAL_GPIO_WritePin(context->port,
                  context->pin,
                  stm32_gpio_map_level(level));
return PLATFORM_ERR_OK;
```

Read:

```c
halLevel = HAL_GPIO_ReadPin(context->port, context->pin);
*level = (halLevel == GPIO_PIN_SET) ?
         PLATFORM_GPIO_LEVEL_HIGH : PLATFORM_GPIO_LEVEL_LOW;
return PLATFORM_ERR_OK;
```

Deinit:

```c
HAL_GPIO_DeInit(context->port, context->pin);
return PLATFORM_ERR_OK;
```

Do not modify `gpio->configured` inside Impl; Platform owns that state transition.

- [x] **Step 6: Run all Impl GPIO Host Tests and verify GREEN**

Expected: construct/configure/write/read/deinit tests PASS.

- [x] **Step 7: Run Platform GPIO Regression again**

Expected: PASS.

- [x] **Step 8: Commit**

Suggested commit:

```bash
git add RTT_elog_DMA_UART_ring_project/04_Impl/impl_mcu/impl_platform_gpio.c \
        RTT_elog_DMA_UART_ring_project/Tests/impl_platform_gpio/
git commit -m "feat: implement STM32 GPIO data operations"
```

---

### Task 4: Dependency Boundary, RCC Guard, and Host Test Completeness

**Files:**
- Modify tests only if gaps are found.
- Do not modify unrelated production modules.

**Interfaces:**
- Consumes: completed GPIO STM32 Impl.
- Produces: verified Phase 1 architecture boundary.

- [x] **Step 1: Run a source scan for forbidden board semantics**

Production GPIO Impl must not contain identifiers / semantics for:

```text
PC13
PA0
LED
KEY
SCL
SDA
DHT20
MPU6050
ACTIVE_LOW
```

- [x] **Step 2: Run a source scan for forbidden RCC ownership**

`impl_platform_gpio.*` must not contain:

```text
__HAL_RCC_GPIO
CLK_ENABLE
CLK_DISABLE
```

except comments that explain the explicit non-ownership contract if such comments are useful.

- [x] **Step 3: Verify no Platform public HAL leakage**

`03_Platform/platform_mcu/gpio/*.h` must remain free of:

```text
stm32f4xx_hal
GPIO_TypeDef
GPIO_InitTypeDef
GPIO_PIN_
HAL_GPIO_
```

- [x] **Step 4: Verify no reverse dependency**

No APP / Service source should newly include `impl_platform_gpio.h`.

- [x] **Step 5: Run all GPIO Host Tests**

Run:

```text
Tests/impl_platform_gpio/
Tests/platform_gpio/
```

Expected: PASS.

- [x] **Step 6: Run relevant existing regressions if current Host harness makes them inexpensive**

At minimum ensure the new include/header structure does not break existing Platform/Impl compilation boundaries. Do not broaden into unrelated refactoring if failures are pre-existing and unrelated.

- [x] **Step 7: Commit any test-only fixes required by this gate**

Suggested commit if needed:

```bash
git commit -m "test: verify STM32 GPIO impl boundaries"
```

---

### Task 5: Keil Integration and Real Build Verification

**Files:**
- Modify the existing Keil project file only as necessary to add `04_Impl/impl_mcu/impl_platform_gpio.c` and required include path already consistent with project organization.
- Do not modify CubeMX concrete GPIO Pin configuration.

**Interfaces:**
- Consumes: completed Production GPIO Impl.
- Produces: STM32 target compile/link evidence.

- [x] **Step 1: Inspect current Keil source groups and include paths**

Follow existing placement used by `impl_platform_uart.c`; do not reorganize unrelated groups.

- [x] **Step 2: Add only the new production GPIO Impl source to the Keil project**

Do not add Host Fake HAL test files to target project.

- [ ] **Step 3: Perform an actual Keil Full Rebuild (NOT EXECUTED: UV4.exe unavailable)**

Required evidence:

```text
Keil Full Rebuild: PASS / FAIL
Error count
Warning count
```

Do not report static inspection as Keil verification.

- [x] **Step 4: If build fails, fix only Phase 1 integration defects**

未执行构建失败修复路径；当前没有 Keil 编译错误证据。

Allowed examples:

```text
missing production include path
incorrect HAL include
signature/type mismatch
new source not added to project
```

Forbidden shortcuts:

```text
changing Platform API to make Impl compile
adding board pin bindings
adding CubeMX pin config
adding RCC ownership to Impl
```

- [x] **Step 5: Re-run Host GPIO Tests after any Keil-driven code change**

Expected: PASS.

- [x] **Step 6: Commit Keil integration**

Suggested commit:

```bash
git add <existing-keil-project-file>
git commit -m "build: integrate STM32 GPIO impl"
```

If the environment cannot execute Keil, report `KEIL NOT VERIFIED` rather than fabricating a result; do not mark Phase 1 complete until user supplies actual Build evidence.

---

### Task 6: Formal Coding Standard Review and Phase Closure

**Files:**
- Review: all files created/modified in Tasks 1-5.
- Modify: `00_Doc/04_Agent/handoff.md` only after all attainable verification gates are known.
- Modify: `00_Doc/04_Agent/implementation_plan.md` task checkboxes/status as execution record if current workflow requires it.

**Interfaces:**
- Consumes: tested and target-integrated GPIO Impl.
- Produces: closure status and next Phase handoff.

- [x] **Step 1: Perform mandatory Coding Standard Review**

Answer explicitly:

```text
1. Naming / file organization compliant?
2. File headers / Doxygen / comments compliant?
3. NULL / pin / context validation complete?
4. Caller-owned Context lifetime clear?
5. Platform state ownership preserved?
6. Initial-level ordering correct?
7. RCC ownership kept outside Impl?
8. Any HAL type leaked into Platform public headers?
9. Any unrelated files changed?
```

`[必须]` violations must be fixed and tests re-run before closure.

- [x] **Step 2: Re-run final Host gates**

Required final evidence:

```text
GPIO STM32 Impl Host Tests       PASS
Platform GPIO Regression         PASS
Dependency Boundary              PASS
RCC Ownership Scan               PASS
Coding Standard Review           PASS
```

- [x] **Step 3: Record Keil status accurately**

Only one of:

```text
Keil Build                       PASS (actual build evidence)
Keil Build                       NOT YET VERIFIED
```

- [x] **Step 4: Update handoff status**

If Host + Coding Review pass but Keil cannot be run:

```text
GPIO STM32 Impl Implementation   COMPLETED / HOST VERIFIED
Keil Build                       NOT YET VERIFIED
Phase 1 Closure                  BLOCKED ON KEIL GATE
Target Board GPIO                NOT YET VERIFIED
```

If actual Keil Build passes:

```text
GPIO STM32 Impl Phase 1          COMPLETED / HOST + KEIL VERIFIED
Target Board GPIO                NOT YET VERIFIED
Next Phase                       Board Resource + CubeMX Configuration
```

- [x] **Step 5: Do not enter Phase 2 automatically**

Phase 2 requires a new design discussion and a new `implementation_plan.md`.

- [x] **Step 6: Commit closure documentation**

Suggested commit:

```bash
git add RTT_elog_DMA_UART_ring_project/00_Doc/04_Agent/handoff.md \
        RTT_elog_DMA_UART_ring_project/00_Doc/04_Agent/implementation_plan.md
git commit -m "docs: close GPIO STM32 Impl Phase1"
```

---

# Final Acceptance Checklist

Phase 1 cannot be declared complete until all required items below are satisfied:

```text
[x] impl_platform_gpio.h created
[x] impl_platform_gpio.c created
[x] caller-owned Context implemented
[x] no Context Pool / Registry / dynamic allocation
[x] one-object-one-pin validation implemented
[x] construct performs no HAL hardware operation
[x] INPUT mapping verified
[x] OUTPUT Push-Pull mapping verified
[x] OUTPUT Open-Drain mapping verified
[x] Pull mapping verified
[x] GPIO Speed fixed privately to LOW
[x] OUTPUT initial level written before HAL_GPIO_Init
[x] INPUT configure performs no initial-level write
[x] write LOW/HIGH mapping verified
[x] read RESET/SET mapping verified
[x] deinit mapping verified
[x] Impl does not modify Platform configured state directly
[x] no RCC ownership in Impl
[x] no LED / KEY / Soft-I2C board semantics in Impl
[x] GPIO STM32 Impl Host Tests PASS
[x] Platform GPIO Regression PASS
[x] Dependency Boundary PASS
[x] Coding Standard Review PASS
[ ] actual Keil Build PASS
```

Phase 1 completion state:

```text
GPIO STM32 Impl Phase 1 = IMPLEMENTED / HOST VERIFIED
Phase 1 Closure          = BLOCKED ON KEIL GATE
Keil Build               = NOT YET VERIFIED
Target Board GPIO        = NOT YET VERIFIED
```

Next Phase only after Phase 1 closure:

```text
Phase 2 — Board Resource + CubeMX Configuration
```

Phase 2 may then bind actual resources such as PC13 LED and PA0 KEY and perform target-board GPIO input/output Smoke Tests.
