# APP Phase 1 Implementation Plan

> **Current execution plan.** Steps use checkbox (`- [ ]`) syntax for tracking.  
> Design authority: `00_Doc/02_架构设计/APP_Phase1设计.md`

**Goal:** Replace the temporary UART Service consumer used for board verification with the first production `01_APP/` vertical slice: a product Composition Root plus one Communication Task that owns product-level UART/Service/Task resources, drains the UART Service byte stream, and performs the frozen DATA_LOSS / UART ERROR recovery policy without exposing Impl, HAL, CMSIS, or FreeRTOS concrete dependencies to APP.

**Architecture:** APP may depend directly on Service and Platform. `app_system` owns static composition and pre-scheduler setup. `app_communication` owns the post-scheduler Communication Task behavior and UART/Service runtime recovery. Platform BSP keeps `Communication UART -> USART1` mapping below APP.

**Tech Stack:** C, STM32F411CEU6, UART Service, Platform BSP UART, Platform UART lifecycle, Platform OS Thread/Time, Platform Log, GCC Host Test, Keil MDK-ARM, STM32 target board.

**Spec:** `00_Doc/02_架构设计/APP_Phase1设计.md`

---

## Mandatory References

Before modifying project C/H files, read:

```text
00_Doc/02_架构设计/APP_Phase1设计.md
00_Doc/02_架构设计/Platform_BSP_UART_Binding_Phase1设计.md
00_Doc/02_架构设计/UART_Service_Phase1设计.md
00_Doc/02_架构设计/Platform_UART抽象层设计.md
00_Doc/02_架构设计/RTOS_Platform_OS设计.md
00_Doc/02_架构设计/RingBuffer_SPSC设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/handoff.md
01_APP/README.md
02_Service/service_uart/service_uart.h
03_Platform/platform_bsp/platform_bsp_uart.h
03_Platform/platform_mcu/uart/platform_uart.h
03_Platform/platform_mcu/uart/platform_uart_types.h
03_Platform/platform_os/platform_thread.h
03_Platform/platform_os/platform_time.h
Core/Src/freertos.c
Core/Src/main.c
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

APP Phase 1 Design:
00_Doc/02_架构设计/APP_Phase1设计.md
Status: READ / FROZEN
```

---

## Global Constraints

Dependency rules are frozen:

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

APP production code must not include or reference:

```text
impl_platform_uart.h
impl_freertos_*.h
usart.h
UART_HandleTypeDef
DMA_HandleTypeDef
STM32 HAL UART / DMA API
cmsis_os2.h
FreeRTOS.h
task.h
TaskHandle_t
USART1 / DMA Stream / IRQ concrete resources
```

Phase 1 scope is limited to:

```text
app_system
app_communication
Communication Task
UART Service byte-stream drain
DATA_LOSS recovery
UART ERROR recovery
minimal APP statistics/status
thin CubeMX startup integration
Host / Keil / Board verification
```

Do not add:

```text
Protocol Parser
Frame Queue
Command Dispatcher
UART Echo
Async TX Service
multi-UART
multi-consumer
malloc/free
Device Registry
Factory / IoC
Event Bus
System Supervisor
Watchdog policy
runtime shutdown framework
```

Additional frozen rules:

- `platform_uart_t`, `service_uart_t`, `platform_thread_t`, DMA RX backing storage, and RingBuffer backing storage are APP-owned static resources.
- Active RX Session is owned by UART Service. APP must never call `platform_uart_cancel(RX)` for a Service-owned Session.
- `service_uart_start()` begins a new Session and resets the Service RingBuffer; drain valid buffered data before restart.
- Combined events are not mutually exclusive. Recovery precedence is `ERROR > DATA_LOSS` after RX drain.
- `PLATFORM_ERR_TIMEOUT` from `service_uart_wait_event()` is normal idle.
- No infinite fast retry on start/recovery failure.
- CubeMX `defaultTask`, `USART1_mutex_Init()`, and UART `fputc()` legacy glue remain out of scope unless a real compile/runtime blocker proves otherwise.
- CubeMX generated files may be changed only in USER CODE sections unless the frozen design explicitly requires a project-file integration change.
- No frozen Platform UART / BSP UART / UART Service / RingBuffer / Platform Notify / Platform Thread public contract changes are allowed in this phase.

If implementation proves such a contract change is necessary:

```text
STOP / BLOCKED
```

Return to design review.

---

## Approved APP Public Surface for Phase 1

The implementation plan resolves the design's optional observability decision by approving read-only APP Communication getters for Host/board verification.

`app_system.h`:

```c
platform_error_t app_system_init(void);
```

`app_communication.h` must define the frozen data model and at least:

```c
#define APP_COMMUNICATION_INITIALIZER {0}

platform_error_t app_communication_init(
    app_communication_t *communication,
    const app_communication_config_t *config);

platform_error_t app_communication_start(
    app_communication_t *communication);

platform_error_t app_communication_process(
    app_communication_t *communication,
    uint32_t timeoutMs);

platform_error_t app_communication_get_status(
    const app_communication_t *communication,
    app_communication_status_t *status);

platform_error_t app_communication_get_statistics(
    const app_communication_t *communication,
    app_communication_statistics_t *statistics);

void app_communication_task_entry(void *argument);
```

The getters are observational only. Do not add setters, direct Service exposure APIs, restart commands, or global object getters.

Recommended status snapshot:

```c
typedef struct
{
    app_communication_state_t state;
    platform_error_t lastError;
} app_communication_status_t;
```

APP statistics remain exactly the APP-owned semantics from the frozen design:

```text
processedChunkCount
processedByteCount
dataLossRecoveryCount
uartErrorRecoveryCount
fatalErrorCount
```

Do not duplicate UART Service RX statistics.

---

## Task 0: Preflight, Repository Sync, and Scope Gate

**Files:** read all Mandatory References before source changes.

- [ ] Run:

```bash
git status --short
git log --oneline -n 15
```

- [ ] Confirm prerequisites from source + handoff:

```text
Platform BSP UART Binding Phase 1   COMPLETED
Communication UART Binding          VERIFIED
UART Service Phase 1                COMPLETED
Base RX Vertical Slice              VERIFIED
Production APP Layer                NOT IMPLEMENTED
APP Phase 1 Design                   FROZEN
```

- [ ] Confirm production APP sources are absent before implementation:

```text
01_APP/app_system.h
01_APP/app_system.c
01_APP/app_communication.h
01_APP/app_communication.c
```

If files already exist, inspect them before any overwrite and reconcile with the frozen design.

- [ ] Confirm `Core/Src/main.c` startup order is still:

```text
MX_GPIO_Init
MX_DMA_Init
MX_USART1_UART_Init
osKernelInitialize
MX_FREERTOS_Init
osKernelStart
```

- [ ] Confirm current `Core/Src/freertos.c` still has USER CODE sections available for a thin `app_system_init()` call.

- [ ] Confirm Platform Thread creation still uses the current Platform API and UART Service still requires a valid `consumerThread` during `service_uart_init()`.

- [ ] Search `01_APP/` and confirm no direct Impl/HAL/CMSIS/FreeRTOS dependency exists.

- [ ] Run:

```bash
git diff --check
```

**Gate:** material mismatch with the frozen design -> `STOP / BLOCKED`.

---

## Task 1: APP Communication Public Contract and Initialization

**Files:**

- Create: `01_APP/app_communication.h`
- Create: `01_APP/app_communication.c`
- Create: `Tests/app_communication/test_app_communication.c`

### 1.1 Public data model

Implement the frozen APP Communication model:

```text
State:
UNINITIALIZED
INITIALIZED
RUNNING
ERROR
```

Config contains only:

```text
platform_uart_t *uart
service_uart_t *service
```

Context contains only APP runtime state / lastError.
Statistics contain only the five approved APP counters.

- [ ] `APP_COMMUNICATION_INITIALIZER` must produce a valid UNINITIALIZED zero state.
- [ ] Public header may depend on Service and Platform public headers only.
- [ ] Public header must not expose Impl/HAL/CMSIS/FreeRTOS concrete types.

### 1.2 Init tests

Add RED tests for:

```text
NULL communication                -> INVALID_PARAM
NULL config                       -> INVALID_PARAM
NULL config.uart                  -> INVALID_PARAM
NULL config.service               -> INVALID_PARAM
valid init                        -> INITIALIZED
valid init                        -> config copied
valid init                        -> lastError OK
valid init                        -> statistics zero
second init                       -> ALREADY_INITIALIZED
```

Use repository error-code conventions consistently; do not invent new APP-specific error enums.

### 1.3 Observability getters

Add tests for:

```text
get_status NULL checks
get_statistics NULL checks
UNINITIALIZED behavior
initialized snapshot
snapshot does not expose mutable internal pointers
```

- [ ] Implement only enough production code to make init/getter tests GREEN.
- [ ] `git diff --check`.
- [ ] Coding Standard Review.

Do not implement UART start/recovery in this task.

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/01_APP/app_communication.h \
        RTT_elog_DMA_UART_ring_project/01_APP/app_communication.c \
        RTT_elog_DMA_UART_ring_project/Tests/app_communication/test_app_communication.c
git commit -m "feat(app): add communication object"
```

---

## Task 2: Post-Scheduler Runtime Start

**Files:**

- Modify: `01_APP/app_communication.c`
- Modify: `Tests/app_communication/test_app_communication.c`

### 2.1 Fake Platform UART lifecycle

Host test must construct a fake `platform_uart_t` whose public lifecycle table records call order for:

```text
init
start
stop
```

Do not link STM32 UART Impl in APP host tests.

Fake UART Service functions must record:

```text
service_uart_start call count/order/result
```

### 2.2 Start RED tests

Add tests for:

```text
UNINITIALIZED -> NOT_INITIALIZED / invalid state per project convention
INITIALIZED success:
    UART lifecycle init
    UART lifecycle start
    service_uart_start
    APP -> RUNNING
    exact order preserved

UART lifecycle init failure:
    no UART start
    no Service start
    APP -> ERROR
    lastError = original init error
    fatalErrorCount++

UART lifecycle start failure:
    no Service start
    APP -> ERROR
    lastError = original start error
    fatalErrorCount++

Service start failure after UART STARTED:
    best-effort UART lifecycle stop once
    APP -> ERROR
    lastError remains original Service start error
    rollback failure does not overwrite original error
    fatalErrorCount++

start while RUNNING / ERROR:
    INVALID_STATE
```

- [ ] Validate required lifecycle function pointers before dereference. Missing required lifecycle operation must fail safely rather than crash.
- [ ] Implement `app_communication_start()` minimally.
- [ ] Do not start DMA or call Platform UART async RX directly; only UART Service may open its RX Session.
- [ ] Run GREEN + `git diff --check` + Coding Standard Review.

**Commit:** `feat(app): add communication runtime start`

---

## Task 3: RX Drain, Combined Events, and Recovery Policy

**Files:**

- Modify: `01_APP/app_communication.c`
- Modify: `Tests/app_communication/test_app_communication.c`

### 3.1 Test-double behavior

Fake Service must support scripted sequences for:

```text
service_uart_wait_event
service_uart_read
service_uart_get_status
service_uart_start
service_uart_stop
```

Tests must be able to verify call order, especially `drain -> recovery`.

### 3.2 RX drain

Add RED tests:

```text
RX_AVAILABLE
    -> service_uart_read repeatedly
    -> multiple successful chunks allowed
    -> stop only at PLATFORM_ERR_EMPTY
    -> processedChunkCount += successful read calls
    -> processedByteCount += total bytes

single wake does not imply frame boundary
read error other than EMPTY -> APP ERROR
```

Production byte consumer in Phase 1 is deliberately minimal: successful read chunks are considered consumed after APP statistics update. Keep the private consumption seam obvious so a later Protocol Parser can replace/extend it without changing Service.

Use an APP-local read buffer of the frozen initial size:

```text
128 bytes
```

Do not add it to UART Service storage.

### 3.3 Timeout

Add test:

```text
service_uart_wait_event -> PLATFORM_ERR_TIMEOUT
    -> app_communication_process returns normal/non-fatal result according to chosen API convention
    -> APP remains RUNNING
    -> no restart
    -> no fatal counter increment
```

Prefer returning `PLATFORM_ERR_OK` for a handled idle timeout so the task loop does not need to treat normal idle as an error. If implementation chooses to propagate TIMEOUT instead, the task entry must explicitly treat it as normal; test the chosen behavior consistently. Do not change Service timeout semantics.

### 3.4 Combined events

Add ordered-call tests:

```text
RX_AVAILABLE | ERROR
    drain all valid data first
    get Service status / lastError
    direct service_uart_start
    no service_uart_stop
    uartErrorRecoveryCount++

RX_AVAILABLE | DATA_LOSS
    drain first
    service_uart_stop
    service_uart_start
    dataLossRecoveryCount++

RX_AVAILABLE | DATA_LOSS | ERROR
    drain first
    ERROR recovery only
    no DATA_LOSS stop/start path
    uartErrorRecoveryCount++
    dataLossRecoveryCount unchanged
```

Never code event handling as one mutually exclusive `if / else if` chain that loses combined bits.

### 3.5 DATA_LOSS recovery

Add tests:

```text
stop OK
    -> restart

stop error + get_status == STOPPED
    -> restart still allowed

stop error + actual state != STOPPED
    -> APP ERROR
    -> original/relevant recovery error recorded

restart failure
    -> APP ERROR
    -> fatalErrorCount++
```

APP must not directly call `platform_uart_cancel()`.

### 3.6 UART ERROR recovery

Add tests:

```text
ERROR
    -> get Service status
    -> record/log Service lastError
    -> service_uart_start directly
    -> no service_uart_stop
    -> no cancel

restart success
    -> APP remains RUNNING
    -> uartErrorRecoveryCount++

restart failure
    -> APP ERROR
    -> fatalErrorCount++
```

### 3.7 Unexpected STOPPED

Add test:

```text
standalone STOPPED observed in normal loop
    -> APP ERROR
    -> lastError = PLATFORM_ERR_CANCELED or frozen equivalent
    -> fatalErrorCount++
    -> no automatic restart storm
```

### 3.8 Process state gate

`app_communication_process()` is valid only in RUNNING. ERROR does not continue the normal wait/recovery loop.

- [ ] Run all APP Communication tests GREEN.
- [ ] `git diff --check`.
- [ ] Coding Standard Review.

**Commit:** `feat(app): consume uart service events`

---

## Task 4: Communication Task Entry and Fatal Error Idle

**Files:**

- Modify: `01_APP/app_communication.c`
- Modify: `Tests/app_communication/test_app_communication.c` as practical for helper behavior.

Task entry contract:

```text
argument = app_communication_t *
        ↓
app_communication_start()
        ↓ success
loop app_communication_process(APP_COMMUNICATION_WAIT_TIMEOUT_MS)
        ↓ fatal APP state
leave normal process loop
log once / low frequency
platform_time_delay_ms(error idle interval)
```

Frozen product values for initial implementation:

```text
APP read buffer          128 bytes
wait timeout             1000 ms
error idle delay         choose a low-frequency value >= 1000 ms
```

- [ ] Do not busy-loop on fatal error.
- [ ] Do not print every RX chunk.
- [ ] Log only low-frequency lifecycle/recovery/fatal events through Platform Log.
- [ ] No direct CMSIS/FreeRTOS API in task entry.
- [ ] If task-entry infinite-loop behavior is awkward to unit-test directly, keep `start()` and `process()` fully unit-testable and test the task-entry helpers/state transitions rather than adding production-only loop escape APIs.
- [ ] Coding Standard Review.

**Commit:** `feat(app): add communication task entry`

---

## Task 5: APP System Composition Root

**Files:**

- Create: `01_APP/app_system.h`
- Create: `01_APP/app_system.c`
- Create: `Tests/app_system/test_app_system.c`

### 5.1 Static product resources

`app_system.c` owns static resources equivalent to:

```text
platform_uart_t       Communication UART
service_uart_t        UART Service
platform_thread_t     Communication Thread
app_communication_t   APP Communication
uint8_t[128]           DMA RX buffer
uint8_t[512]           RingBuffer storage
```

Use project initializer macros / zero initialization according to the coding standard.

Product UART config is fixed initially to:

```text
baudRate       115200
dataBits       PLATFORM_UART_DATA_BITS_8
stopBits       PLATFORM_UART_STOP_BITS_1
parity         PLATFORM_UART_PARITY_NONE
flowControl    PLATFORM_UART_FLOW_CONTROL_NONE
defaultTimeout choose existing project-appropriate finite value; do not invent WAIT_FOREVER unless required
```

Communication Thread config:

```text
name           "communication"
entry          app_communication_task_entry
argument       &APP Communication object
stackSizeBytes 1024
priority       PLATFORM_THREAD_PRIORITY_NORMAL
```

### 5.2 Pre-scheduler composition order

Frozen order:

```text
1 platform_bsp_uart_construct_communication
2 app_communication_init
3 platform_thread_create
4 service_uart_init
5 return OK
```

Service config must pass:

```text
uart                  -> APP Communication UART
dmaRxBuffer           -> APP 128-byte DMA storage
dmaRxBufferSize       -> 128
ringBufferStorage     -> APP 512-byte storage
ringBufferStorageSize -> 512
consumerThread        -> APP Communication Thread
```

### 5.3 Host tests

Provide fakes for BSP UART, APP Communication init, Platform Thread, and UART Service init.

Add ordered-call tests:

```text
all success
    -> exact 1-2-3-4 order
    -> exact pointers/sizes/thread config
    -> OK

BSP construct fails
    -> nothing later called

APP Communication init fails
    -> no Thread / Service init

Thread create fails
    -> no Service init

Service init fails after Thread create
    -> best-effort platform_thread_terminate
    -> return original Service init error
    -> rollback error does not overwrite original error
```

Do not invent a Platform UART “destruct CREATED object” API. On fatal pre-scheduler composition failure, return the original error; CubeMX glue will stop startup.

`app_system_init()` is a one-shot startup API. A second call must fail safely (`ALREADY_INITIALIZED` / `INVALID_STATE` per implementation data model) rather than reconstruct the same UART or create a second thread.

### 5.4 No hidden Impl dependency

Host compile/source review must prove:

```text
app_system.c does not include Impl/HAL/CMSIS/FreeRTOS headers
app_system.c obtains Communication UART only through platform_bsp_uart_construct_communication()
```

- [ ] GREEN Host Test.
- [ ] `git diff --check`.
- [ ] Coding Standard Review.

**Commit:** `feat(app): add system composition root`

---

## Task 6: CubeMX Thin Integration and Keil Project Integration

**Files:**

- Modify USER CODE sections only: `Core/Src/freertos.c`
- Modify: `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx`

### 6.1 freertos.c

In USER CODE Includes add:

```c
#include "app_system.h"
```

In `MX_FREERTOS_Init()` USER CODE Init:

```text
result = app_system_init()
if result != OK:
    Error_Handler()
```

Use the smallest local declaration allowed by CubeMX USER CODE structure.

Do not:

```text
move Communication Task into defaultTask
remove defaultTask
remove USART1_mutex_Init()
move UART Service logic into freertos.c
add HAL UART code to APP glue
```

### 6.2 Keil project

Add include path for:

```text
01_APP
```

Add production sources:

```text
01_APP/app_system.c
01_APP/app_communication.c
```

Use an APP group consistent with the existing project structure; do not reorganize unrelated groups.

### 6.3 Host regression gate

Run new APP tests plus relevant existing regressions, at minimum:

```text
app_communication
app_system
platform_bsp_uart
service_uart
platform_uart
platform_os relevant tests
ring_buffer
```

Prefer the repository's existing aggregate test scripts if available.

Required:

```text
all selected tests PASS
```

### 6.4 Keil Full Rebuild

Perform the established Keil Full Rebuild.

Required evidence:

```text
0 Error(s)
```

Review new warnings. Do not hide warnings by weakening compiler settings.

- [ ] Verify no APP source directly includes Impl/HAL/CMSIS/FreeRTOS headers.
- [ ] Verify no frozen lower-layer source/API was modified for APP convenience.
- [ ] `git diff --check`.
- [ ] Coding Standard Review.

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/Core/Src/freertos.c \
        RTT_elog_DMA_UART_ring_project/MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
git commit -m "build: integrate app phase1"
```

If APP source commits and project integration are easier to review as one coherent commit, preserve task boundaries in notes but do not create artificial broken intermediate commits.

---

## Task 7: Target Board Production-APP Verification

**Goal:** prove the verified ISR -> Service path is now consumed by the production `01_APP/` Communication Task rather than a temporary task in CubeMX code.

### 7.1 Baseline runtime

Flash the APP-integrated build and confirm:

```text
APP Communication state = RUNNING
UART Service state       = RUNNING
no immediate fatal error
no restart storm
```

### 7.2 RX payload

Reuse the verified raw payload:

```text
00..FF repeated
1280 bytes total
```

Expected after one complete transfer:

```text
APP processedByteCount        = 1280
Service rxBytesReceived       = 1280
Service rxBytesDropped        = 0
Service DATA_LOSS             = false
Service ERROR                 = not observed
```

### 7.3 Temporary content-integrity hook

Statistics prove quantity, not byte identity. Add one localized temporary board-verification hook at the APP byte-consumption seam to compare the expected `00..FF` repeating sequence.

Temporary hook requirements:

```text
centralized and clearly marked
Task Context only
no lower-layer changes
no ISR logging
counts mismatch / compared bytes
can report summary through Platform Log / RTT
```

Expected:

```text
compared bytes = 1280
mismatch       = 0
```

Do not leave the test-pattern assumption in production APP behavior.

### 7.4 Cleanup

After PASS:

- [ ] Remove temporary content-integrity hook.
- [ ] Re-run Keil Full Rebuild.
- [ ] Required: `0 Error(s)`.
- [ ] Optionally run a short normal RX smoke after cleanup if practical; record only actual evidence.

If target hardware is not available to the executing agent, stop with Board Verification explicitly `PENDING MANUAL TEST`; do not mark APP Phase 1 COMPLETED.

---

## Task 8: Final Boundary Review and Handoff

**Files:**

- Modify: `00_Doc/04_Agent/handoff.md`

### 8.1 Dependency review

Confirm:

```text
01_APP -> Service / Platform only
01_APP -> Impl                    ABSENT
01_APP -> HAL                     ABSENT
01_APP -> CMSIS/FreeRTOS concrete ABSENT
```

Confirm production path:

```text
CubeMX startup
    ↓
app_system
    ├── Platform BSP Communication UART
    ├── Platform Thread
    ├── UART Service
    └── static backing storage
           ↓
app_communication task
           ↓
UART Service wait/drain/recovery
```

### 8.2 Scope review

Confirm this phase did not introduce:

```text
Protocol Parser
Frame Queue
Async TX
multi-UART
Device Registry
Factory/IoC
runtime shutdown framework
```

### 8.3 Verification record

Record only evidence actually obtained:

```text
APP Communication Host Test       PASS / actual
APP System Host Test              PASS / actual
Lower-layer Regression            PASS / actual
Coding Standard Review            PASS / actual
Keil Full Rebuild                 PASS / actual
Production APP Board RX Test      PASS / PENDING
Content Integrity Hook            PASS / PENDING
Cleanup Rebuild                   PASS / PENDING
```

### 8.4 Completion state

Only when all mandatory Host/Keil/Board gates have passed:

```text
APP Phase 1                    COMPLETED
Production APP RX Vertical     VERIFIED
Production APP Layer           IMPLEMENTED (Phase 1)
Next Phase                     protocol/application behavior design
Next State                     READY_FOR_NEXT_DESIGN
```

If Board Test remains pending, use a truthful intermediate state such as:

```text
APP Phase 1                    IMPLEMENTED / HOST_KEIL_VERIFIED
Production APP RX Vertical     BOARD_VERIFICATION_PENDING
```

Do not mark COMPLETED early.

Also retire this implementation plan in handoff after completion; future phases must replace it rather than append unrelated tasks.

- [ ] Final repository checks:

```bash
git status --short
git diff --check
git log --oneline -n 15
```

**Commit:** `docs: complete app phase1 verification` only after actual completion criteria are met.

---

# Completion Criteria

APP Phase 1 is complete only when all are true:

```text
[ ] Frozen APP design followed without contract drift
[ ] app_system.h/.c implemented
[ ] app_communication.h/.c implemented
[ ] APP owns UART / Service / Thread / DMA / Ring storage
[ ] Communication UART obtained only through Platform BSP
[ ] Communication Task created through Platform Thread
[ ] Pre-scheduler composition order verified
[ ] Post-scheduler UART init/start -> Service start order verified
[ ] RX drain until EMPTY verified
[ ] Combined event precedence verified
[ ] DATA_LOSS stop/status/start recovery verified
[ ] UART ERROR direct restart recovery verified
[ ] timeout normal-idle behavior verified
[ ] unexpected STOPPED fatal behavior verified
[ ] no fast retry / busy-loop fatal path
[ ] APP does not duplicate Service RX statistics
[ ] APP has no direct Impl/HAL/CMSIS/FreeRTOS dependency
[ ] Host APP tests PASS
[ ] relevant lower-layer regressions PASS
[ ] Coding Standard Review PASS
[ ] Keil Full Rebuild 0 Error(s)
[ ] Production APP board RX test PASS
[ ] 1280-byte content comparison mismatch = 0
[ ] temporary board-test hook removed
[ ] cleanup Keil Rebuild 0 Error(s)
[ ] handoff updated with actual evidence
```

After completion, stop. Do not begin Protocol Parser, command processing, async TX, or the next APP feature in the same scope without a new design/plan.
