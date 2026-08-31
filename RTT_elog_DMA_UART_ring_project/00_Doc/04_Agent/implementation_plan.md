# Platform GPIO Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> 当前执行计划 / Current Active Plan  
> 状态：READY / NOT STARTED  
> 日期：2026-08-31

**Goal:** 在不依赖 STM32 HAL、CubeMX 具体 Pin 配置和 GPIO Impl 的前提下，实现可 Host Test 的普通数字 GPIO Platform Phase 1 抽象。

**Architecture:** GPIO 在本阶段定义为轻量 MCU Resource，而不是完整 `platform_device_t`。Platform 负责公共类型、对象状态、参数校验和 Ops 转发；具体硬件访问通过 opaque `implContext` 和后续 Impl 注入的 `platform_gpio_ops_t` 完成。本阶段只到 Platform Host Verified，不进入目标板验证。

**Tech Stack:** C、现有 `platform_types.h`、`platform_error.h`、Host C tests；禁止引入 STM32 HAL、CMSIS-RTOS2、FreeRTOS 或 Vendor GPIO 类型。

**Spec:** `00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md`

## Global Constraints

- 固定依赖方向保持 `APP -> Service -> Platform -> Impl -> Vendor`；本计划不得制造反向依赖。
- `platform_gpio_t` 不继承 `platform_device_t`，不使用 `platform_lifecycle_t`。
- 本阶段只允许 GPIO INPUT / OUTPUT、PULL、OUTPUT TYPE、INITIAL LEVEL、read / write / configure / deinit。
- 不公开 GPIO Speed。
- 不加入 EXTI / IRQ / NVIC / callback、Toggle、AF、Analog、LED / KEY、Debounce、GPIO Group / Registry。
- 不修改 `04_Impl/`。
- 不修改 CubeMX 具体 GPIO Pin 配置，不新增 `HAL_GPIO_xxx`、`GPIO_TypeDef`、`GPIO_PIN_x` 依赖。
- 不使用动态内存和 RTOS 同步原语。
- 生产代码和测试代码都必须遵守 `00_Doc/02_架构设计/嵌入式项目C代码设计规范.md`。
- 发现仓库现实与冻结设计存在实质冲突时：`STOP / BLOCKED`，不得静默重新设计。

---

# 0. Mandatory Preflight

执行前必须读取：

```text
00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/handoff.md
03_Platform/platform_common/platform_types.h
03_Platform/platform_common/platform_error.h
03_Platform/platform_mcu/uart/platform_uart_types.h
03_Platform/platform_mcu/uart/platform_uart.h
Tests/platform_uart/test_platform_uart_types.c
Tests/platform_uart/test_platform_uart.c
```

Preflight 固定汇报：

```text
GPIO Platform Phase 1 Design: READ
Coding Standard:
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
Status: READ
Agent Execution Rules: READ
Current repository state: INSPECTED
Unrelated user changes: PRESERVED
```

执行前确认：

```text
03_Platform/platform_mcu/gpio/ does not already contain conflicting production code
Tests/platform_gpio/ does not contain conflicting current tests
No current GPIO public contract contradicts the frozen spec
```

如果发现冲突，停止并报告，不覆盖既有用户代码。

---

# 1. Frozen Public Contract

公共类型冻结为：

```c
typedef struct platform_gpio platform_gpio_t;

typedef enum
{
    PLATFORM_GPIO_LEVEL_LOW = 0,
    PLATFORM_GPIO_LEVEL_HIGH,
    PLATFORM_GPIO_LEVEL_MAX
} platform_gpio_level_t;

typedef enum
{
    PLATFORM_GPIO_DIRECTION_INPUT = 0,
    PLATFORM_GPIO_DIRECTION_OUTPUT,
    PLATFORM_GPIO_DIRECTION_MAX
} platform_gpio_direction_t;

typedef enum
{
    PLATFORM_GPIO_PULL_NONE = 0,
    PLATFORM_GPIO_PULL_UP,
    PLATFORM_GPIO_PULL_DOWN,
    PLATFORM_GPIO_PULL_MAX
} platform_gpio_pull_t;

typedef enum
{
    PLATFORM_GPIO_OUTPUT_PUSH_PULL = 0,
    PLATFORM_GPIO_OUTPUT_OPEN_DRAIN,
    PLATFORM_GPIO_OUTPUT_MAX
} platform_gpio_output_type_t;

typedef struct
{
    platform_gpio_direction_t direction;
    platform_gpio_pull_t pull;
    platform_gpio_output_type_t outputType;
    platform_gpio_level_t initialLevel;
} platform_gpio_config_t;
```

公共对象与 Ops 冻结为：

```c
typedef struct
{
    platform_error_t (*configure)(platform_gpio_t *gpio,
                                  const platform_gpio_config_t *config);
    platform_error_t (*write)(platform_gpio_t *gpio,
                              platform_gpio_level_t level);
    platform_error_t (*read)(platform_gpio_t *gpio,
                             platform_gpio_level_t *level);
    platform_error_t (*deinit)(platform_gpio_t *gpio);
} platform_gpio_ops_t;

struct platform_gpio
{
    const char *name;
    platform_gpio_config_t config;
    const platform_gpio_ops_t *ops;
    void *implContext;
    platform_bool_t initialized;
    platform_bool_t configured;
};

#define PLATFORM_GPIO_INITIALIZER {0}

typedef struct
{
    const char *name;
    const platform_gpio_ops_t *ops;
    void *implContext;
} platform_gpio_init_params_t;
```

公共 API 冻结为：

```c
platform_error_t platform_gpio_init(
    platform_gpio_t *gpio,
    const platform_gpio_init_params_t *params);

platform_error_t platform_gpio_configure(
    platform_gpio_t *gpio,
    const platform_gpio_config_t *config);

platform_error_t platform_gpio_write(
    platform_gpio_t *gpio,
    platform_gpio_level_t level);

platform_error_t platform_gpio_read(
    platform_gpio_t *gpio,
    platform_gpio_level_t *level);

platform_error_t platform_gpio_deinit(
    platform_gpio_t *gpio);
```

不得在实施过程中擅自增加公共 API。

---

# 2. Target Files

## Create

```text
03_Platform/platform_mcu/gpio/platform_gpio_types.h
03_Platform/platform_mcu/gpio/platform_gpio.h
03_Platform/platform_mcu/gpio/platform_gpio.c
Tests/platform_gpio/test_platform_gpio_types.c
Tests/platform_gpio/test_platform_gpio.c
```

## Modify

```text
00_Doc/04_Agent/handoff.md   # only at final handoff after verified implementation
```

## Forbidden in this plan

```text
04_Impl/**
Core/** GPIO behavior changes
Drivers/**
Middlewares/**
01_APP/**
02_Service/**
03_Platform/platform_bsp/**
CubeMX concrete pin configuration
```

---

# 3. Task 1 — Public GPIO Types and Header Isolation

**Files:**
- Create: `03_Platform/platform_mcu/gpio/platform_gpio_types.h`
- Create: `Tests/platform_gpio/test_platform_gpio_types.c`

**Interfaces:**
- Consumes: `platform_types.h`, `platform_error.h`
- Produces: `platform_gpio_t` forward declaration, four enum types, `platform_gpio_config_t`

- [ ] **Step 1: Write the failing public-type Host Test**

`Tests/platform_gpio/test_platform_gpio_types.c` 必须验证：

```c
#include "platform_gpio_types.h"

int main(void)
{
    platform_gpio_config_t config = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_LOW
    };

    return ((PLATFORM_GPIO_DIRECTION_OUTPUT == config.direction) &&
            (PLATFORM_GPIO_LEVEL_LOW == config.initialLevel)) ? 0 : 1;
}
```

同时添加 compile-time assertions，确认各 `*_MAX` 值位于合法枚举之后，公共头可以在没有 STM32 HAL Header 的 Host 环境独立编译。

- [ ] **Step 2: Run the type test and verify RED**

使用仓库现有 Host Test 编译方式；如果 Tests 没有统一脚本，则沿用 `Tests/platform_uart` 当前编译参数和 include path。

预期：因 `platform_gpio_types.h` 不存在而失败。

- [ ] **Step 3: Implement `platform_gpio_types.h`**

要求：

```text
- 文件头 / include guard / 注释符合 C 代码规范
- include platform_types.h
- include platform_error.h only if needed by the public declarations in this header
- 不 include platform_device.h
- 不 include platform_lifecycle.h
- 不 include stm32f4xx_hal.h
- 不出现 GPIO_TypeDef / GPIO_PIN_x / HAL_GPIO_xxx
- enum / struct 名称与冻结设计完全一致
```

- [ ] **Step 4: Run type/header isolation test and verify GREEN**

预期：PASS。

- [ ] **Step 5: Perform Task 1 Coding Standard Review**

检查命名、文件头、注释、Header isolation、无 STM32 依赖。

- [ ] **Step 6: Commit Task 1**

建议提交：

```text
feat: define Platform GPIO public types
```

---

# 4. Task 2 — GPIO Object, Ops and Init Contract

**Files:**
- Create: `03_Platform/platform_mcu/gpio/platform_gpio.h`
- Create: `03_Platform/platform_mcu/gpio/platform_gpio.c`
- Create/Modify: `Tests/platform_gpio/test_platform_gpio.c`

**Interfaces:**
- Consumes: Task 1 public types, `platform_error_t`
- Produces: `platform_gpio_ops_t`, `platform_gpio_t`, `platform_gpio_init_params_t`, `PLATFORM_GPIO_INITIALIZER`, `platform_gpio_init()`

- [ ] **Step 1: Write failing tests for object construction**

Fake Ops 可使用 file-local functions，不依赖 HAL。

必须覆盖：

```text
PLATFORM_GPIO_INITIALIZER -> initialized=false / configured=false
platform_gpio_init(NULL, params) -> PLATFORM_ERR_NULL_POINTER
platform_gpio_init(gpio, NULL) -> PLATFORM_ERR_NULL_POINTER
params->ops == NULL -> PLATFORM_ERR_INVALID_PARAM
first init -> PLATFORM_ERR_OK
first init binds name / ops / implContext
first init -> initialized=true / configured=false
second init -> PLATFORM_ERR_ALREADY_INITIALIZED
individual ops may be NULL at construction time
```

- [ ] **Step 2: Run tests and verify RED**

预期：因 `platform_gpio.h/.c` 尚未实现而失败。

- [ ] **Step 3: Implement public object declarations in `platform_gpio.h`**

严格使用 Frozen Public Contract，不增加 Device/Lifecycle、currentLevel、mutex、registry 等字段。

- [ ] **Step 4: Implement minimal `platform_gpio_init()` in `platform_gpio.c`**

行为顺序：

```text
validate gpio
validate params
reject gpio->initialized
validate params->ops
bind fields
initialized = true
configured = false
return OK
```

`name == NULL` 和 `implContext == NULL` 均允许；它们不参与当前硬件语义。

- [ ] **Step 5: Run tests and verify GREEN**

预期：Task 2 construction tests PASS。

- [ ] **Step 6: Coding Standard Review**

特别确认：

```text
No platform_device_t inheritance
No platform_lifecycle_t
No HAL / STM32 symbols
No hidden hardware initialization in init()
```

- [ ] **Step 7: Commit Task 2**

建议提交：

```text
feat: add Platform GPIO object construction
```

---

# 5. Task 3 — Configure Contract and State Preservation

**Files:**
- Modify: `03_Platform/platform_mcu/gpio/platform_gpio.c`
- Modify: `03_Platform/platform_mcu/gpio/platform_gpio.h`
- Modify: `Tests/platform_gpio/test_platform_gpio.c`

**Interfaces:**
- Consumes: `platform_gpio_ops_t.configure`
- Produces: `platform_gpio_configure()`

- [ ] **Step 1: Add failing configure tests**

覆盖：

```text
not initialized -> PLATFORM_ERR_NOT_INITIALIZED
config == NULL -> PLATFORM_ERR_NULL_POINTER
direction >= MAX -> PLATFORM_ERR_INVALID_PARAM
pull >= MAX -> PLATFORM_ERR_INVALID_PARAM
outputType >= MAX -> PLATFORM_ERR_INVALID_PARAM
initialLevel >= MAX -> PLATFORM_ERR_INVALID_PARAM
configure op == NULL -> PLATFORM_ERR_NOT_SUPPORTED
first configure success -> forwards exact config
first configure success -> configured=true + config copied
first configure Impl failure -> configured=false
Impl error -> returned unchanged
successful reconfigure -> new config replaces old config
failed reconfigure after previous success -> configured stays true
failed reconfigure after previous success -> previous successful config preserved
```

Fake configure op 必须记录调用次数、对象指针和收到的 config，并可配置返回值。

- [ ] **Step 2: Run tests and verify RED**

预期：新 configure tests FAIL。

- [ ] **Step 3: Implement file-local config validation helper**

建议使用单一 file-local helper 校验四个枚举，避免公共 API 重复判断。

合法条件：

```text
direction < PLATFORM_GPIO_DIRECTION_MAX
pull < PLATFORM_GPIO_PULL_MAX
outputType < PLATFORM_GPIO_OUTPUT_MAX
initialLevel < PLATFORM_GPIO_LEVEL_MAX
```

不得因为 INPUT 模式而允许非法 `outputType` 或 `initialLevel`。

- [ ] **Step 4: Implement `platform_gpio_configure()`**

关键顺序：

```text
validate
snapshot is implicit in existing object state
call ops->configure
if failure: return without changing cached successful config/state
if success: gpio->config = *config; gpio->configured = true
```

重新配置失败时不得先清空 `configured`。

- [ ] **Step 5: Run tests and verify GREEN**

预期：所有 configure tests PASS。

- [ ] **Step 6: Coding Standard Review**

重点检查失败路径不会破坏旧成功配置。

- [ ] **Step 7: Commit Task 3**

建议提交：

```text
feat: implement Platform GPIO configuration
```

---

# 6. Task 4 — Write and Read Contracts

**Files:**
- Modify: `03_Platform/platform_mcu/gpio/platform_gpio.c`
- Modify: `03_Platform/platform_mcu/gpio/platform_gpio.h`
- Modify: `Tests/platform_gpio/test_platform_gpio.c`

**Interfaces:**
- Consumes: `platform_gpio_ops_t.write`, `platform_gpio_ops_t.read`
- Produces: `platform_gpio_write()`, `platform_gpio_read()`

- [ ] **Step 1: Add failing write tests**

覆盖：

```text
not initialized -> NOT_INITIALIZED
initialized but not configured -> INVALID_STATE
configured INPUT -> INVALID_STATE
invalid level -> INVALID_PARAM
write op NULL -> NOT_SUPPORTED
configured OUTPUT + valid level -> exact Ops forwarding
Impl write error -> unchanged propagation
successful write does not mutate gpio->config.initialLevel
```

最后一项用于确认 `initialLevel` 是配置时初值，不是 runtime currentLevel cache。

- [ ] **Step 2: Add failing read tests**

覆盖：

```text
not initialized -> NOT_INITIALIZED
not configured -> INVALID_STATE
level == NULL -> NULL_POINTER
read op NULL -> NOT_SUPPORTED
configured INPUT -> read allowed
configured OUTPUT -> read allowed
Fake read LOW/HIGH -> exact value returned
Impl read error -> unchanged propagation
```

- [ ] **Step 3: Run tests and verify RED**

预期：write/read tests FAIL。

- [ ] **Step 4: Implement `platform_gpio_write()`**

校验顺序应保证未构造对象不会解引用 Ops；只有 OUTPUT 才允许 write。

- [ ] **Step 5: Implement `platform_gpio_read()`**

INPUT / OUTPUT 均允许，不增加方向限制。

- [ ] **Step 6: Run tests and verify GREEN**

预期：write/read tests 全部 PASS。

- [ ] **Step 7: Coding Standard Review**

重点确认：

```text
No currentLevel state added
No hardware-specific level mapping in Platform
No input/output polarity semantics beyond logical HIGH/LOW
```

- [ ] **Step 8: Commit Task 4**

建议提交：

```text
feat: add Platform GPIO read and write
```

---

# 7. Task 5 — Deinit and Reconfigure Lifecycle

**Files:**
- Modify: `03_Platform/platform_mcu/gpio/platform_gpio.c`
- Modify: `03_Platform/platform_mcu/gpio/platform_gpio.h`
- Modify: `Tests/platform_gpio/test_platform_gpio.c`

**Interfaces:**
- Consumes: `platform_gpio_ops_t.deinit`
- Produces: `platform_gpio_deinit()` and complete lightweight state cycle

- [ ] **Step 1: Add failing deinit tests**

覆盖：

```text
not initialized -> NOT_INITIALIZED
initialized but not configured -> INVALID_STATE
deinit op NULL -> NOT_SUPPORTED
deinit Impl failure -> error unchanged
deinit Impl failure -> configured remains true
deinit success -> configured=false
deinit success -> initialized remains true
deinit success -> ops / implContext binding remains intact
after successful deinit -> configure again succeeds
```

- [ ] **Step 2: Run tests and verify RED**

预期：deinit tests FAIL。

- [ ] **Step 3: Implement `platform_gpio_deinit()`**

成功时只修改：

```text
configured = false
```

不得：

```text
clear initialized
clear ops
clear implContext
clear name
free memory
```

- [ ] **Step 4: Run full Platform GPIO test suite and verify GREEN**

预期：`test_platform_gpio_types` 与 `test_platform_gpio` 全部 PASS。

- [ ] **Step 5: Coding Standard Review**

确认 `deinit()` 语义是 hardware deconfiguration，不是 object destruction。

- [ ] **Step 6: Commit Task 5**

建议提交：

```text
feat: complete Platform GPIO lifecycle
```

---

# 8. Task 6 — Boundary, Regression and Frozen-Design Review

**Files:**
- Inspect only production files created by Tasks 1-5
- Modify only if a defect is found inside current scope

**Interfaces:**
- Produces: verified Phase 1 Platform-only implementation

- [ ] **Step 1: Run complete GPIO Host Tests**

Expected: PASS。

- [ ] **Step 2: Header Isolation Scan**

确认以下生产文件和 Tests 不包含：

```text
stm32f4xx_hal.h
GPIO_TypeDef
GPIO_InitTypeDef
GPIO_PIN_
HAL_GPIO_
FreeRTOS
cmsis_os
```

- [ ] **Step 3: Dependency Boundary Scan**

确认 `03_Platform/platform_mcu/gpio/` 不依赖：

```text
04_Impl
01_APP
02_Service
platform_bsp concrete device semantics
```

允许依赖：

```text
platform_types.h
platform_error.h
```

- [ ] **Step 4: Public Contract Diff Review**

逐项对照 `Platform_GPIO_Phase1设计.md`：

```text
exact enums
exact config fields
exact object fields
exact Ops
exact public APIs
state semantics
error semantics
reconfigure failure preservation
deinit semantics
```

任何公共合同偏差都必须先修复或 STOP，不得自行合理化。

- [ ] **Step 5: Explicit Non-Goal Scan**

确认没有新增：

```text
GPIO speed
Toggle
IRQ / EXTI / callback
AF / Analog
LED / KEY
RTOS lock
Registry
Dynamic memory
STM32 Impl
```

- [ ] **Step 6: Run existing nearby regression tests**

至少重新运行：

```text
Tests/platform_uart
```

如果仓库已有统一 Platform Host Test 脚本，则运行全部 Platform Host Tests。

新增 GPIO 模块不应改变现有 UART / Common 行为。

- [ ] **Step 7: Final Coding Standard Review**

依据 `execution_rules.md` 回答：

```text
1. 命名 / 文件组织 / 注释是否符合规范？
2. NULL / enum / state / return path 是否完整？
3. 是否存在生命周期、所有权或并发问题？
4. 是否修改生成代码 / Vendor / 无关模块？
5. 是否存在冻结设计偏离？
```

结果必须为：

```text
Coding Standard Review: PASS
```

否则不得标记阶段完成。

- [ ] **Step 8: Commit verification fixes if any**

如果 Task 6 发现并修复当前范围缺陷，单独提交：

```text
fix: harden Platform GPIO Phase1 contract
```

若无修改，不制造空提交。

---

# 9. Task 7 — Handoff Update and Phase Closure

**Files:**
- Modify: `00_Doc/04_Agent/handoff.md`

**Interfaces:**
- Produces: current repository truth for the next Agent

- [ ] **Step 1: Update GPIO status only after evidence exists**

只有 Host Test / boundary / coding-standard Gate 都 PASS 后，才允许将：

```text
GPIO Platform Phase 1 Implementation NOT STARTED
```

更新为：

```text
GPIO Platform Phase 1                COMPLETED / HOST VERIFIED
GPIO Platform Host Tests             PASS
Header Isolation                     PASS
Platform Dependency Boundary         PASS
Coding Standard Review               PASS
STM32 Impl                            NOT STARTED
Target Board GPIO                    NOT YET VERIFIED
```

- [ ] **Step 2: Preserve verification boundaries**

不得写：

```text
Keil Build Verified
Target Board Verified
LED Verified
```

除非用户后续真实提供对应证据；这些不属于当前 Platform-only Phase。

- [ ] **Step 3: Set next phase**

下一阶段入口写为：

```text
GPIO STM32 Impl Phase 1 Design
```

强调必须重新设计和重新生成新的 `implementation_plan.md`，不得直接延长当前计划。

- [ ] **Step 4: Commit handoff**

建议提交：

```text
docs: close Platform GPIO Phase1
```

---

# 10. Final Acceptance Gate

只有以下全部满足，GPIO Platform Phase 1 才算完成：

```text
[ ] Frozen spec unchanged or explicitly re-approved
[ ] platform_gpio_types.h implemented
[ ] platform_gpio.h implemented
[ ] platform_gpio.c implemented
[ ] Host type test PASS
[ ] Host behavior test PASS
[ ] configure failure preserves prior successful state
[ ] deinit failure preserves configured state
[ ] Header Isolation PASS
[ ] No STM32 HAL dependency PASS
[ ] Platform dependency boundary PASS
[ ] Existing Platform UART regression PASS
[ ] Coding Standard Review PASS
[ ] No 04_Impl changes
[ ] No CubeMX concrete GPIO pin configuration changes
[ ] handoff.md updated with evidence-backed status
```

阶段允许的最终标签：

```text
GPIO Platform Phase 1
COMPLETED / HOST VERIFIED
```

后续真实硬件链路必须通过独立阶段完成：

```text
GPIO STM32 Impl Phase 1 Design
    -> Impl plan
    -> STM32 GPIO mapping
    -> Board / BSP binding
    -> target-board GPIO verification
```
