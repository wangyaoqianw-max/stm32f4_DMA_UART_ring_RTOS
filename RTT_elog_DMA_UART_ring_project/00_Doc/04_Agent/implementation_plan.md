# Platform BSP UART Binding Phase 1 Implementation Plan

> **Current execution plan.** Steps use checkbox (`- [ ]`) syntax for tracking.  
> Design authority: `00_Doc/02_架构设计/Platform_BSP_UART_Binding_Phase1设计.md`

**Goal:** Add one minimal Platform BSP contract that lets APP construct the product-level Communication UART without directly depending on Impl, while preserving the existing APP-owned `platform_uart_t`, Platform UART lifecycle, UART Service callback ownership, and verified USART1 runtime behavior.

**Architecture:** APP may depend directly on Service and Platform. APP/Service must not depend directly on Impl. `03_Platform/platform_bsp/platform_bsp_uart.h` exposes the logical Communication UART contract; `04_Impl/impl_bsp/impl_platform_bsp_uart.c` implements the current-board binding by delegating to `impl_platform_uart_usart1_construct()`.

**Tech Stack:** C, STM32F411CEU6, Platform UART, STM32 UART Impl, GCC Host Test, Keil MDK-ARM.

**Scope:** Binding only. No APP production code, no UART Service changes, no Platform UART API changes, no lifecycle redesign, no Device Registry, no multi-UART abstraction.

---

## Mandatory References

Before modifying project C/H files, read:

```text
00_Doc/02_架构设计/Platform_BSP_UART_Binding_Phase1设计.md
00_Doc/02_架构设计/Platform_UART抽象层设计.md
00_Doc/02_架构设计/UART_Service_Phase1设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/handoff.md
03_Platform/platform_mcu/uart/platform_uart.h
03_Platform/platform_mcu/uart/platform_uart_types.h
04_Impl/impl_mcu/impl_platform_uart.h
04_Impl/impl_mcu/impl_platform_uart.c
MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
```

Preflight report must explicitly contain:

```text
Coding Standard:
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
Status: READ

Agent Execution Rules:
00_Doc/04_Agent/execution_rules.md
Status: READ

Platform BSP UART Binding Phase 1 Design:
00_Doc/02_架构设计/Platform_BSP_UART_Binding_Phase1设计.md
Status: READ / FROZEN
```

---

## Global Constraints

- Dependency rules for this phase:

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

- APP continues to own `platform_uart_t` storage.
- APP / Caller provides `platform_uart_config_t` behavior configuration.
- Current board mapping is exactly `Communication UART -> USART1`.
- `platform_bsp_uart_construct_communication()` performs construction/binding only; it must not execute UART lifecycle operations.
- BSP construction must pass `callback = NULL` and `callbackContext = NULL`; UART Service later owns callback binding through `platform_uart_set_callback()`.
- Do not modify existing Platform UART public API or UART Service public API.
- Do not modify existing UART DMA RX behavior, HAL callbacks, RingBuffer, Platform Notify, or Consumer semantics.
- No dynamic allocation.
- No Device Registry, Factory, IoC container, role enum table, reference count, or runtime resource manager.
- Phase 1 Composition Contract: the Communication UART is constructed once during system composition.
- Do not clean up `USART1_mutex_Init()`, `fputc()`, CubeMX UART legacy glue, or unrelated technical debt in this phase.
- Preserve unrelated local changes; do not use destructive reset/clean operations.
- Existing `platform_types.h -> board_types.h` dependency is known technical debt. Do not expand it and do not use this phase to migrate fundamental types.

If implementation appears to require any frozen Platform UART / UART Service contract change:

```text
STOP / BLOCKED
```

Return to design review.

---

## Task 0: Preflight and Current-State Gate

**Files:** read only initially.

- [ ] Run repository state checks:

```bash
git status --short
git log --oneline -n 15
```

- [ ] Read every Mandatory Reference.

- [ ] Confirm prerequisites:

```text
UART Phase 2A                 COMPLETED
Platform OS Phase 1           COMPLETED
RingBuffer Phase 1            COMPLETED
UART Service Phase 1          COMPLETED
Base RX Vertical Slice        VERIFIED
01_APP production layer       NOT IMPLEMENTED
```

- [ ] Confirm the new production files do not already exist, or if they do exist, compare them to the frozen design before modifying anything:

```text
03_Platform/platform_bsp/platform_bsp_uart.h
04_Impl/impl_bsp/impl_platform_bsp_uart.c
Tests/platform_bsp_uart/test_platform_bsp_uart.c
```

- [ ] Confirm current concrete constructor remains:

```c
platform_error_t impl_platform_uart_usart1_construct(
    platform_uart_t *uart,
    const char *name,
    uint32_t caps,
    const platform_uart_config_t *config,
    platform_uart_callback_t callback,
    void *callbackContext);
```

- [ ] Confirm no APP production source currently includes:

```text
impl_platform_uart.h
usart.h
STM32 HAL UART headers
```

- [ ] Run:

```bash
git diff --check
```

**Gate:** Any material mismatch between repository reality and the frozen BSP design is `STOP / BLOCKED` before source implementation.

---

## Task 1: Add Platform BSP UART Public Contract and RED Host Test

**Files:**

- Create: `03_Platform/platform_bsp/platform_bsp_uart.h`
- Create: `Tests/platform_bsp_uart/test_platform_bsp_uart.c`

**Public API:**

```c
platform_error_t platform_bsp_uart_construct_communication(
    platform_uart_t *uart,
    const platform_uart_config_t *config);
```

### 1.1 Public Header

- [ ] Add the project-standard self-written C header/file banner.
- [ ] Include only the Platform public dependency required by the function contract, normally:

```c
#include "platform_uart.h"
```

- [ ] Do **not** directly include:

```text
impl_platform_uart.h
usart.h
stm32f4xx_hal.h
cmsis_os2.h
FreeRTOS.h
```

- [ ] Add Doxygen for the public API describing:

```text
caller-owned platform_uart_t
caller-provided config
construction/binding only
no lifecycle execution
Communication UART logical role
```

- [ ] Do not add role enums, registration tables, opaque factory objects, or global getters.

### 1.2 Host Fake

In `test_platform_bsp_uart.c`, provide a fake implementation of:

```c
impl_platform_uart_usart1_construct(...)
```

The fake must record:

```text
call count
uart pointer
name
caps
config pointer
callback
callbackContext
configured return value
```

This lets the host test compile the real `impl_platform_bsp_uart.c` later without linking STM32 HAL.

### 1.3 RED Tests

- [ ] Add tests for the frozen behavior:

```text
NULL uart
    -> PLATFORM_ERR_INVALID_PARAM
    -> fake constructor call count == 0

NULL config
    -> PLATFORM_ERR_INVALID_PARAM
    -> fake constructor call count == 0

valid uart + config
    -> forwards exact uart pointer
    -> forwards exact config pointer
    -> name == "communication_uart"
    -> caps == PLATFORM_DEVICE_CAP_NONE
    -> callback == NULL
    -> callbackContext == NULL
    -> fake constructor called exactly once

fake constructor error
    -> exact platform_error_t propagated
```

- [ ] Add a header-isolation translation-unit check that includes `platform_bsp_uart.h` without HAL/CMSIS/FreeRTOS/UART Impl headers in the test source.

Known exception: the repository currently has inherited `platform_types.h -> board_types.h` technical debt, so host include paths may still need `04_Impl/impl_board`. The BSP header must not introduce any **new direct** Impl/HAL include dependency.

- [ ] Run the intended Host compile before adding production implementation and verify RED because `platform_bsp_uart_construct_communication()` has no definition yet:

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  -I03_Platform/platform_bsp \
  -I03_Platform/platform_mcu/uart \
  -I03_Platform/platform_common \
  -I04_Impl/impl_board \
  -I04_Impl/impl_mcu \
  Tests/platform_bsp_uart/test_platform_bsp_uart.c \
  -o Tests/platform_bsp_uart/test_platform_bsp_uart
```

Expected: link FAIL for the missing BSP production function. If it unexpectedly passes because an implementation already exists, return to Task 0 and inspect repository state.

- [ ] Run `git diff --check`.

**Commit boundary:** do not commit a deliberately RED repository state unless the current development workflow intentionally preserves test-first commits. Default is to continue directly to Task 2, get GREEN, then commit the coherent unit.

---

## Task 2: Implement the Thin Current-Board Binding

**Files:**

- Create: `04_Impl/impl_bsp/impl_platform_bsp_uart.c`
- Modify as needed only for test corrections: `Tests/platform_bsp_uart/test_platform_bsp_uart.c`

### 2.1 Required Includes

Production implementation should depend on:

```c
#include "platform_bsp_uart.h"
#include "impl_platform_uart.h"
```

It may use `PLATFORM_DEVICE_CAP_NONE` through the existing Platform public type graph.

Do not include `usart.h` or HAL directly in the BSP binding source; USART1/HAL knowledge remains inside the existing UART Impl constructor.

### 2.2 Required Behavior

Implement only:

```text
validate uart != NULL
validate config != NULL
call impl_platform_uart_usart1_construct()
return the exact result
```

The delegation parameters are frozen as:

```c
impl_platform_uart_usart1_construct(
    uart,
    "communication_uart",
    PLATFORM_DEVICE_CAP_NONE,
    config,
    NULL,
    NULL);
```

Do not:

```text
call lifecycle init/start
call HAL
start DMA RX
bind Service callback
allocate memory
save APP pointers in new globals
add local resource registry
```

### 2.3 GREEN Host Test

- [ ] Build the host test with the real BSP implementation and fake USART1 constructor:

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  -I03_Platform/platform_bsp \
  -I03_Platform/platform_mcu/uart \
  -I03_Platform/platform_common \
  -I04_Impl/impl_board \
  -I04_Impl/impl_mcu \
  Tests/platform_bsp_uart/test_platform_bsp_uart.c \
  04_Impl/impl_bsp/impl_platform_bsp_uart.c \
  -o Tests/platform_bsp_uart/test_platform_bsp_uart
```

- [ ] Run:

```bash
./Tests/platform_bsp_uart/test_platform_bsp_uart
```

Expected: PASS.

- [ ] Verify the test uses a fake constructor only; it must not link `impl_platform_uart.c`, HAL, CubeMX `usart.c`, or FreeRTOS.

- [ ] Run:

```bash
git diff --check
```

- [ ] Perform Coding Standard Review for the new header/source/test.

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/03_Platform/platform_bsp/platform_bsp_uart.h \
        RTT_elog_DMA_UART_ring_project/04_Impl/impl_bsp/impl_platform_bsp_uart.c \
        RTT_elog_DMA_UART_ring_project/Tests/platform_bsp_uart/test_platform_bsp_uart.c
git commit -m "feat(bsp): bind communication uart"
```

---

## Task 3: Keil Project Integration and Regression Gate

**Files:**

- Modify: `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx`

### 3.1 Include Path

- [ ] Ensure Keil can resolve:

```text
03_Platform/platform_bsp
```

Do not remove or reorder unrelated include paths unless required by the project format.

### 3.2 Source Integration

- [ ] Add:

```text
04_Impl/impl_bsp/impl_platform_bsp_uart.c
```

Prefer the existing Impl/BSP group if present; otherwise create the smallest consistent group without reorganizing unrelated project files.

### 3.3 Regression

- [ ] Re-run the new Host BSP test.

- [ ] Run relevant existing Host regressions that cover the contracts this phase relies on, at minimum:

```text
Platform UART
UART Service
```

If the repository has a standard aggregate regression script, use it instead of inventing a parallel test runner.

- [ ] Confirm no source changes occurred in:

```text
02_Service/service_uart/
03_Platform/platform_mcu/uart/platform_uart.h
03_Platform/platform_mcu/uart/platform_uart.c
04_Impl/impl_mcu/impl_platform_uart.c
Core/Src/usart.c
Core/Src/freertos.c
```

except for project-file metadata required to compile the new BSP source. If any runtime file needed modification, STOP and review scope.

### 3.4 Keil Build

- [ ] Perform a Keil Full Rebuild using the repository's established build environment.

Required evidence:

```text
0 Error(s)
```

Warnings must be reviewed; do not silently treat new warnings as acceptable.

- [ ] No target-board runtime test is required for this binding-only phase because the runtime USART1 implementation and UART Service behavior are unchanged.

- [ ] Run:

```bash
git diff --check
```

- [ ] Coding Standard Review: PASS required.

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
git commit -m "build: integrate platform bsp uart"
```

---

## Task 4: Final Architecture Boundary Review and Handoff

**Files:**

- Modify: `00_Doc/04_Agent/handoff.md`

### 4.1 Boundary Review

- [ ] Search the APP layer and confirm there is still no direct Impl dependency:

```text
01_APP -> impl_platform_uart.h     ABSENT
01_APP -> usart.h                  ABSENT
01_APP -> HAL UART                 ABSENT
```

- [ ] Confirm new dependency shape:

```text
APP may use platform_bsp_uart.h
platform_bsp_uart.h does not directly expose Impl/HAL/Vendor types
impl_platform_bsp_uart.c is the only new current-board binding source
```

- [ ] Confirm public API surface added in this phase is exactly:

```c
platform_bsp_uart_construct_communication();
```

No extra role enum / registry / getter / factory should remain.

### 4.2 Verification Record

Record only evidence actually obtained:

```text
Platform BSP UART Host Test       PASS
Header Isolation                  PASS
Platform UART Regression          PASS
UART Service Regression           PASS
Coding Standard Review            PASS
Keil Full Rebuild                 PASS / actual result
Target Board Test                 NOT REQUIRED
```

Never convert “not run” into PASS.

### 4.3 Handoff State

After all required gates pass, update current state to approximately:

```text
Platform BSP UART Binding Phase 1   COMPLETED
Communication UART Binding          VERIFIED
Production APP Layer                NOT IMPLEMENTED
Next Phase                          APP Phase 1 Design
Next State                          READY_FOR_APP_DESIGN
```

Add the stable architecture rule:

```text
APP may depend on Service and Platform.
APP / Service must not directly depend on Impl.
Communication UART logical role is obtained through Platform BSP.
Current board binding: Communication UART -> USART1.
```

Do not copy the entire implementation plan into `handoff.md`.

- [ ] Final:

```bash
git status --short
git diff --check
git log --oneline -n 10
```

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/00_Doc/04_Agent/handoff.md
git commit -m "docs: complete platform bsp uart binding"
```

---

# Completion Criteria

Platform BSP UART Binding Phase 1 is complete only when all are true:

```text
[ ] Frozen design followed without contract drift
[ ] platform_bsp_uart.h added
[ ] impl_platform_bsp_uart.c added
[ ] Communication UART binds to USART1 only inside Impl/BSP boundary
[ ] APP owns platform_uart_t storage
[ ] APP/Service have no new direct Impl dependency
[ ] No Platform UART / UART Service public contract changed
[ ] Host BSP UART Test PASS
[ ] Header isolation PASS within known platform_types technical debt
[ ] Relevant regression PASS
[ ] Coding Standard Review PASS
[ ] Keil Full Rebuild 0 Error(s)
[ ] handoff updated with actual evidence
```

After completion, stop. Do not implement APP Phase 1 in the same scope unless a new APP Phase 1 design/plan is explicitly activated.
