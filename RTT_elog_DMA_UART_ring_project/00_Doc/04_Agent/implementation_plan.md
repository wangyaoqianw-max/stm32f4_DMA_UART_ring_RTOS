# APP Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first production `01_APP/` RX vertical slice: centralized product static configuration, a product Composition Root, and one Communication Task that consumes UART Service byte-stream data and applies the frozen DATA_LOSS / ERROR recovery policy without exposing Impl/HAL/RTOS concrete dependencies to APP.

**Architecture:** `00_Config/project_config.h` owns compile-time product parameters only. `app_system` owns APP-level static objects/backing storage and pre-scheduler composition. `app_communication` owns post-scheduler runtime start, wait/drain/recovery, minimal APP statistics, and low-frequency logging. APP may depend on Service and Platform; APP and Service must not directly depend on Impl.

**Tech Stack:** C, STM32F411CEU6, Platform BSP UART, Platform UART lifecycle, Platform OS Thread/Time, Platform Log, UART Service, SPSC RingBuffer, CMSIS-RTOS2 + FreeRTOS behind Platform adapters, GCC Host Test, Keil MDK-ARM.

**Specs:**

```text
00_Doc/02_架构设计/APP_Phase1设计.md
00_Doc/02_架构设计/APP_Phase1_Config补充设计.md
```

---

## Mandatory References

Before modifying project C/H files, read:

```text
00_Doc/02_架构设计/APP_Phase1设计.md
00_Doc/02_架构设计/APP_Phase1_Config补充设计.md
00_Doc/02_架构设计/UART_Service_Phase1设计.md
00_Doc/02_架构设计/Platform_BSP_UART_Binding_Phase1设计.md
00_Doc/02_架构设计/Platform_UART抽象层设计.md
00_Doc/02_架构设计/RTOS_Platform_OS设计.md
00_Doc/02_架构设计/RingBuffer_SPSC设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/handoff.md
00_Config/README.md
01_APP/README.md
02_Service/service_uart/service_uart.h
03_Platform/platform_bsp/platform_bsp_uart.h
03_Platform/platform_mcu/uart/platform_uart.h
03_Platform/platform_mcu/uart/platform_uart_types.h
03_Platform/platform_os/platform_thread.h
03_Platform/platform_os/platform_time.h
03_Platform/platform_middleware/platform_log.h
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

APP Phase 1 Config Addendum:
00_Doc/02_架构设计/APP_Phase1_Config补充设计.md
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

APP production code must not include or use:

```text
impl_platform_uart.h
impl_freertos_*.h
usart.h
UART_HandleTypeDef
DMA_HandleTypeDef
stm32f4xx_hal_uart.h
cmsis_os2.h
FreeRTOS.h
task.h
TaskHandle_t
USART1 / DMA Stream / IRQ concrete resources
```

`00_Config/project_config.h` may depend on public Platform configuration types/enums, but must not include Impl/HAL/CMSIS/FreeRTOS concrete headers.

Frozen product values:

```text
UART baudRate                 115200
UART dataBits                 PLATFORM_UART_DATA_BITS_8
UART stopBits                 PLATFORM_UART_STOP_BITS_1
UART parity                   PLATFORM_UART_PARITY_NONE
UART flowControl              PLATFORM_UART_FLOW_CONTROL_NONE
UART defaultTimeoutMs         1000 ms
DMA RX buffer                 128 bytes
RingBuffer storage            512 bytes (511 usable)
APP read buffer               128 bytes
Communication Task stack      1024 bytes
Communication Task priority   PLATFORM_THREAD_PRIORITY_NORMAL
wait_event timeout            1000 ms
fatal error idle delay        1000 ms
```

These values must have a single product-level definition in `project_config.h`. APP source must not repeat equivalent numeric literals except unavoidable zero/boolean/state values.

Other frozen constraints:

- No dynamic allocation.
- `platform_uart_t`, `service_uart_t`, `platform_thread_t`, `app_communication_t`, DMA RX storage, and RingBuffer storage are APP-owned static resources.
- `00_Config` stores values only; object pointers, storage addresses, runtime state, statistics, handles, callbacks, and Impl mapping do not belong there.
- Platform BSP owns the logical-to-concrete UART mapping; current mapping remains `Communication UART -> USART1`.
- UART Service remains sole owner of the active RX Session and the only production caller allowed to cancel that Session.
- Notification is a wake hint; Service/RingBuffer state is truth.
- Drain valid RingBuffer bytes before recovery that starts a new RX Session.
- Recovery priority after drain is `ERROR > DATA_LOSS > STOPPED`.
- `PLATFORM_ERR_TIMEOUT` from `service_uart_wait_event()` is normal idle; APP remains RUNNING.
- No Protocol Parser, Frame Queue, Async TX, UART echo, multi-UART, Device Registry, Factory/IoC, Event Bus, Watchdog policy, or product shutdown framework.
- Keep CubeMX `defaultTask`, `USART1_mutex_Init()`, and legacy `fputc()` technical debt in this phase unless a real blocker proves otherwise.
- Do not modify frozen lower-layer public contracts for APP convenience.

If implementation requires a frozen contract change:

```text
STOP / BLOCKED
```

Return to design review.

---

## Approved APP Public Surface

`app_system.h`:

```c
platform_error_t app_system_init(void);
```

`app_communication.h` must expose the frozen APP data model plus:

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

Getters are read-only observability APIs. Do not add setters, global object getters, restart commands, direct Service exposure APIs, or runtime shutdown APIs.

APP statistics are exactly:

```text
processedChunkCount
processedByteCount
dataLossRecoveryCount
uartErrorRecoveryCount
fatalErrorCount
```

Do not duplicate UART Service RX statistics.

---

## Task 0: Preflight and Repository State Gate

**Files:** read only initially.

- [ ] Run:

```bash
git status --short
git log --oneline -n 15
```

- [ ] Read every Mandatory Reference.

- [ ] Confirm prerequisites:

```text
Platform BSP UART Binding Phase 1   COMPLETED
Communication UART Binding          VERIFIED
UART Service Phase 1                COMPLETED
Base RX Vertical Slice              VERIFIED
Production APP Layer                NOT IMPLEMENTED
APP Phase 1 Design                   FROZEN
APP Phase 1 Config Addendum          FROZEN
```

- [ ] Confirm production files do not already exist, or inspect/reconcile them if they do:

```text
00_Config/project_config.h
01_APP/app_system.h
01_APP/app_system.c
01_APP/app_communication.h
01_APP/app_communication.c
Tests/project_config/test_project_config.c
Tests/app_communication/test_app_communication.c
Tests/app_system/test_app_system.c
```

- [ ] Confirm `main.c` order remains:

```text
MX_GPIO_Init
MX_DMA_Init
MX_USART1_UART_Init
osKernelInitialize
MX_FREERTOS_Init
osKernelStart
```

- [ ] Confirm `freertos.c` has USER CODE sections available for a thin APP entry and still contains the known defaultTask / USART1 mutex legacy glue.

- [ ] Confirm UART Service requires a valid `consumerThread` during `service_uart_init()`.

- [ ] Confirm `01_APP/` currently has no direct Impl/HAL/CMSIS/FreeRTOS dependency.

- [ ] Run `git diff --check`.

**Gate:** material mismatch with either frozen APP design -> `STOP / BLOCKED`.

---

## Task 1: Centralize Product Static Configuration

**Files:**

- Modify: `00_Config/README.md`
- Create: `00_Config/project_config.h`
- Create: `Tests/project_config/test_project_config.c`

### 1.1 README contract

Document that `00_Config` contains product-level compile-time settings only.

Explicitly state that these do **not** belong in Config:

```text
object instances / pointers
storage addresses
runtime context/state/statistics
HAL / DMA / USART handles
Impl mapping
callbacks
RTOS native handles
```

### 1.2 Public configuration header

Create `project_config.h` with project-standard self-written header/comment rules.

Allowed public dependencies:

```c
#include "platform_uart_types.h"
#include "platform_thread.h"
```

Required macros:

```c
#define PROJECT_COMM_UART_BAUD_RATE              (115200U)
#define PROJECT_COMM_UART_DATA_BITS              PLATFORM_UART_DATA_BITS_8
#define PROJECT_COMM_UART_STOP_BITS              PLATFORM_UART_STOP_BITS_1
#define PROJECT_COMM_UART_PARITY                 PLATFORM_UART_PARITY_NONE
#define PROJECT_COMM_UART_FLOW_CONTROL           PLATFORM_UART_FLOW_CONTROL_NONE
#define PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS     (1000U)

#define PROJECT_COMM_DMA_RX_BUFFER_SIZE           (128U)
#define PROJECT_COMM_RING_BUFFER_STORAGE_SIZE     (512U)
#define PROJECT_COMM_READ_BUFFER_SIZE             (128U)

#define PROJECT_COMM_TASK_STACK_SIZE_BYTES        (1024U)
#define PROJECT_COMM_TASK_PRIORITY                PLATFORM_THREAD_PRIORITY_NORMAL

#define PROJECT_COMM_WAIT_TIMEOUT_MS              (1000U)
#define PROJECT_COMM_ERROR_IDLE_DELAY_MS          (1000U)
```

Do not define UART instance, DMA stream, HAL handle, Service pointer, Task object, Buffer address, or BSP mapping in this header.

### 1.3 RED/GREEN tests

Add compile/static-assert tests verifying the exact frozen values and enum selections.

The test source includes only:

```c
#include "project_config.h"
```

plus standard headers required by the test harness. It must not include Impl/HAL/CMSIS/FreeRTOS concrete headers.

Use C11 static assertions where practical, e.g.:

```c
_Static_assert(PROJECT_COMM_UART_BAUD_RATE == 115200U,
               "unexpected communication baud rate");
_Static_assert(PROJECT_COMM_DMA_RX_BUFFER_SIZE == 128U,
               "unexpected dma rx buffer size");
_Static_assert(PROJECT_COMM_RING_BUFFER_STORAGE_SIZE == 512U,
               "unexpected ring storage size");
```

- [ ] Verify RED before the header exists if following strict TDD in a clean worktree.
- [ ] Implement header and README.
- [ ] Compile/run test GREEN with repository-appropriate Platform include paths.
- [ ] Run `git diff --check`.
- [ ] Coding Standard Review.

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/00_Config \
        RTT_elog_DMA_UART_ring_project/Tests/project_config
git commit -m "feat(config): add project communication settings"
```

---

## Task 2: APP Communication Object, Init, and Observability

**Files:**

- Create: `01_APP/app_communication.h`
- Create: `01_APP/app_communication.c`
- Create: `Tests/app_communication/test_app_communication.c`

**Consumes:** UART Service public API, Platform UART public object/lifecycle, Platform Time/Log, `project_config.h`.

### 2.1 Data model

Implement:

```c
typedef enum
{
    APP_COMMUNICATION_STATE_UNINITIALIZED = 0,
    APP_COMMUNICATION_STATE_INITIALIZED,
    APP_COMMUNICATION_STATE_RUNNING,
    APP_COMMUNICATION_STATE_ERROR,
    APP_COMMUNICATION_STATE_MAX
} app_communication_state_t;

typedef struct
{
    platform_uart_t *uart;
    service_uart_t *service;
} app_communication_config_t;

typedef struct
{
    app_communication_state_t state;
    platform_error_t lastError;
} app_communication_context_t;

typedef struct
{
    uint32_t processedChunkCount;
    uint32_t processedByteCount;
    uint32_t dataLossRecoveryCount;
    uint32_t uartErrorRecoveryCount;
    uint32_t fatalErrorCount;
} app_communication_statistics_t;

typedef struct
{
    app_communication_state_t state;
    platform_error_t lastError;
} app_communication_status_t;

typedef struct
{
    app_communication_config_t config;
    app_communication_context_t context;
    app_communication_statistics_t statistics;
} app_communication_t;
```

### 2.2 Init RED tests

Cover:

```text
NULL communication        -> INVALID_PARAM
NULL config               -> INVALID_PARAM
NULL config.uart          -> INVALID_PARAM
NULL config.service       -> INVALID_PARAM
valid init                -> INITIALIZED
config copied             -> exact pointers
lastError                 -> OK
statistics                -> zero
second init               -> ALREADY_INITIALIZED
```

### 2.3 Getter tests

Cover NULL checks, UNINITIALIZED behavior, and exact snapshots for status/statistics.

- [ ] Implement only init/getters in this task.
- [ ] Do not start UART or Service yet.
- [ ] GREEN tests.
- [ ] `git diff --check`.
- [ ] Coding Standard Review.

**Commit:** `feat(app): add communication object`

---

## Task 3: Post-Scheduler Runtime Start

**Files:**

- Modify: `01_APP/app_communication.c`
- Modify: `Tests/app_communication/test_app_communication.c`

Host fake UART lifecycle must record:

```text
init
start
stop
```

Fake UART Service records `service_uart_start()` order/result.

Add RED tests:

```text
UNINITIALIZED
    -> NOT_INITIALIZED / safe project-consistent error

INITIALIZED success
    -> UART lifecycle init
    -> UART lifecycle start
    -> service_uart_start
    -> exact order
    -> APP RUNNING

UART init failure
    -> no later calls
    -> APP ERROR
    -> lastError original
    -> fatalErrorCount++

UART start failure
    -> no Service start
    -> APP ERROR

Service start failure after UART started
    -> best-effort UART stop once
    -> original Service error preserved
    -> APP ERROR

RUNNING / ERROR start attempt
    -> INVALID_STATE
```

Validate required lifecycle pointers before dereference.

APP must never call `platform_uart_read_async()` or `platform_uart_cancel()` directly.

- [ ] GREEN tests.
- [ ] `git diff --check`.
- [ ] Coding Standard Review.

**Commit:** `feat(app): add communication runtime start`

---

## Task 4: RX Drain, Timeout, Combined Events, and Recovery

**Files:**

- Modify: `01_APP/app_communication.c`
- Modify: `Tests/app_communication/test_app_communication.c`

Use an APP-local read buffer sized only by:

```c
PROJECT_COMM_READ_BUFFER_SIZE
```

Do not hard-code `128` again in APP source.

Fake Service must script/record:

```text
service_uart_wait_event
service_uart_read
service_uart_get_status
service_uart_stop
service_uart_start
```

### 4.1 RX drain

Tests:

```text
RX_AVAILABLE
    -> read repeatedly until PLATFORM_ERR_EMPTY
    -> multiple chunks supported
    -> processedChunkCount += successful reads
    -> processedByteCount += bytes

read error != EMPTY
    -> APP ERROR
    -> fatalErrorCount++
```

A Service wake/read chunk is not a protocol frame.

### 4.2 Timeout

Frozen APP behavior:

```text
service_uart_wait_event -> PLATFORM_ERR_TIMEOUT
    -> app_communication_process returns PLATFORM_ERR_OK
    -> APP stays RUNNING
    -> no recovery
    -> no fatal counter
```

### 4.3 Combined events

Verify ordered calls:

```text
RX_AVAILABLE | ERROR
    -> drain first
    -> get Service status
    -> direct service_uart_start
    -> no service_uart_stop

RX_AVAILABLE | DATA_LOSS
    -> drain first
    -> service_uart_stop
    -> service_uart_start

RX_AVAILABLE | DATA_LOSS | ERROR
    -> drain first
    -> ERROR recovery only
    -> no DATA_LOSS stop path
```

Recovery precedence is exactly:

```text
ERROR > DATA_LOSS > STOPPED
```

### 4.4 DATA_LOSS recovery

Tests:

```text
stop OK
    -> restart
    -> dataLossRecoveryCount++

stop error + actual Service STOPPED
    -> restart still allowed

stop error + actual Service not STOPPED
    -> APP ERROR

restart failure
    -> APP ERROR
    -> fatalErrorCount++
```

APP must not call `platform_uart_cancel()`.

### 4.5 UART ERROR recovery

Tests:

```text
ERROR
    -> get Service status / lastError
    -> direct service_uart_start
    -> no service_uart_stop
    -> uartErrorRecoveryCount++ on success

restart failure
    -> APP ERROR
    -> fatalErrorCount++
```

### 4.6 Unexpected STOPPED

Standalone STOPPED in normal loop:

```text
APP -> ERROR
lastError = PLATFORM_ERR_CANCELED
fatalErrorCount++
no automatic restart storm
```

`app_communication_process()` is valid only while APP is RUNNING.

- [ ] GREEN APP Communication tests.
- [ ] `git diff --check`.
- [ ] Coding Standard Review.

**Commit:** `feat(app): consume uart service events`

---

## Task 5: Communication Task Entry and Fatal Error Idle

**Files:**

- Modify: `01_APP/app_communication.c`
- Modify: `Tests/app_communication/test_app_communication.c` where practical.

Task entry contract:

```text
argument = app_communication_t *
        ↓
app_communication_start()
        ↓ success
loop app_communication_process(PROJECT_COMM_WAIT_TIMEOUT_MS)
        ↓ APP ERROR
leave normal processing behavior
log low-frequency fatal state
platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS)
```

Rules:

- Use `PROJECT_COMM_WAIT_TIMEOUT_MS`; do not hard-code 1000.
- Use `PROJECT_COMM_ERROR_IDLE_DELAY_MS`; do not hard-code 1000.
- No busy loop on fatal error.
- No per-RX-chunk log.
- Log lifecycle/recovery/fatal events only through Platform Log.
- No direct CMSIS/FreeRTOS API.
- Do not add loop-escape/test-only production APIs solely to unit-test the infinite task entry; test `start()` and `process()` directly.

- [ ] Coding Standard Review.

**Commit:** `feat(app): add communication task entry`

---

## Task 6: APP System Composition Root

**Files:**

- Create: `01_APP/app_system.h`
- Create: `01_APP/app_system.c`
- Create: `Tests/app_system/test_app_system.c`

### 6.1 Static product resources

`app_system.c` owns:

```text
platform_uart_t       Communication UART
service_uart_t        UART Service
platform_thread_t     Communication Thread
app_communication_t   APP Communication
uint8_t               DMA RX storage[PROJECT_COMM_DMA_RX_BUFFER_SIZE]
uint8_t               Ring storage[PROJECT_COMM_RING_BUFFER_STORAGE_SIZE]
```

All sizes come from `project_config.h`.

### 6.2 UART config composition

Construct `platform_uart_config_t` using only project config macros:

```c
static const platform_uart_config_t g_communicationUartConfig = {
    .baudRate = PROJECT_COMM_UART_BAUD_RATE,
    .dataBits = PROJECT_COMM_UART_DATA_BITS,
    .stopBits = PROJECT_COMM_UART_STOP_BITS,
    .parity = PROJECT_COMM_UART_PARITY,
    .flowControl = PROJECT_COMM_UART_FLOW_CONTROL,
    .defaultTimeoutMs = PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS
};
```

Do not put this runtime module config struct into `00_Config`; only its values belong there.

### 6.3 Communication Thread config

Use:

```text
name           "communication"
entry          app_communication_task_entry
argument       &APP Communication object
stackSizeBytes PROJECT_COMM_TASK_STACK_SIZE_BYTES
priority       PROJECT_COMM_TASK_PRIORITY
```

### 6.4 Frozen pre-scheduler order

```text
1 platform_bsp_uart_construct_communication
2 app_communication_init
3 platform_thread_create
4 service_uart_init
5 return OK
```

`service_uart_config_t` receives actual APP-owned object/storage addresses in `app_system.c`:

```text
uart                  -> Communication UART object
dmaRxBuffer           -> APP DMA storage
dmaRxBufferSize       -> PROJECT_COMM_DMA_RX_BUFFER_SIZE
ringBufferStorage     -> APP Ring storage
ringBufferStorageSize -> PROJECT_COMM_RING_BUFFER_STORAGE_SIZE
consumerThread        -> Communication Thread object
```

These pointers/addresses must not be moved to `project_config.h`.

### 6.5 Host tests

Fakes record BSP construct, APP Communication init, Platform Thread create/terminate, and UART Service init.

Tests:

```text
all success
    -> exact 1-2-3-4 order
    -> exact pointers
    -> exact config macro values/sizes

BSP failure
    -> nothing later called

APP Communication init failure
    -> no Thread / Service init

Thread create failure
    -> no Service init

Service init failure after Thread create
    -> best-effort platform_thread_terminate
    -> original Service error returned

second app_system_init
    -> fails safely
    -> no second UART construction/thread creation
```

Do not invent a UART CREATED-object destructor API. Fatal pre-scheduler failure returns the original error; CubeMX glue stops startup.

Verify no `impl_*`, HAL, CMSIS, or FreeRTOS concrete include in APP source.

- [ ] GREEN Host Test.
- [ ] `git diff --check`.
- [ ] Coding Standard Review.

**Commit:** `feat(app): add system composition root`

---

## Task 7: CubeMX Thin Integration, Keil Integration, and Regression

**Files:**

- Modify USER CODE sections only: `Core/Src/freertos.c`
- Modify: `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx`

### 7.1 freertos.c

USER CODE include:

```c
#include "app_system.h"
```

In `MX_FREERTOS_Init()` USER CODE Init:

```text
result = app_system_init()
if result != PLATFORM_ERR_OK:
    Error_Handler()
```

Use a minimal local `platform_error_t result` declaration in a legal USER CODE section.

Do not:

```text
move Communication Task into defaultTask
remove defaultTask
remove USART1_mutex_Init()
put UART Service logic in freertos.c
put HAL UART logic in APP glue
```

### 7.2 Keil project

Add include paths:

```text
00_Config
01_APP
```

Add sources:

```text
01_APP/app_system.c
01_APP/app_communication.c
```

`project_config.h` is header-only and does not need a source entry.

### 7.3 Regression gate

Run new tests plus relevant lower-layer regressions at minimum:

```text
project_config
app_communication
app_system
platform_bsp_uart
service_uart
platform_uart
platform_os relevant tests
ring_buffer
```

Use existing aggregate scripts when available.

### 7.4 Keil Full Rebuild

Required evidence:

```text
0 Error(s)
```

Review new warnings; do not weaken compiler settings.

- [ ] Verify APP has no direct Impl/HAL/CMSIS/FreeRTOS dependency.
- [ ] Verify APP source uses `PROJECT_COMM_*` for all frozen product parameters.
- [ ] Verify no lower-layer frozen API/source changed for APP convenience.
- [ ] Run `git diff --check`.
- [ ] Coding Standard Review.

**Commit:** `build: integrate app phase1`

---

## Task 8: Target Board Production APP Verification

**Goal:** prove the production `01_APP/` Communication Task now consumes the previously verified ISR -> UART Service chain.

### 8.1 Baseline runtime

Confirm after flash:

```text
APP Communication state = RUNNING
UART Service state       = RUNNING
no immediate fatal error
no restart storm
```

### 8.2 RX payload

Reuse:

```text
00..FF repeated
1280 bytes total
```

Expected:

```text
APP processedByteCount  = 1280
Service rxBytesReceived = 1280
Service rxBytesDropped  = 0
DATA_LOSS               = false
ERROR                   = not observed
```

### 8.3 Temporary content-integrity hook

At the APP byte-consumption seam only, temporarily compare the repeating `00..FF` pattern.

Requirements:

```text
Task Context only
localized and clearly marked
no lower-layer modification
no ISR logging
counts compared bytes and mismatches
summary through Platform Log / RTT
```

Expected:

```text
compared bytes = 1280
mismatch       = 0
```

Do not leave test-pattern behavior in production APP.

### 8.4 Cleanup

- [ ] Remove temporary compare hook.
- [ ] Re-run Keil Full Rebuild.
- [ ] Required: `0 Error(s)`.
- [ ] Record only actual evidence.

If hardware is unavailable, mark Board Verification `PENDING MANUAL TEST`; do not mark APP Phase 1 COMPLETED.

---

## Task 9: Final Boundary Review and Handoff

**Files:**

- Modify: `00_Doc/04_Agent/handoff.md`

### 9.1 Dependency/config review

Confirm:

```text
00_Config -> public Platform types only where needed
00_Config -> Impl/HAL/CMSIS/FreeRTOS concrete     ABSENT
01_APP -> Service / Platform                      PRESENT
01_APP -> Impl/HAL/CMSIS/FreeRTOS concrete        ABSENT
```

Confirm:

```text
project_config.h owns product values
app_system owns runtime object/storage composition
Platform BSP owns Communication UART -> USART1 mapping
```

Search APP source and ensure frozen values are not duplicated as product literals outside `project_config.h`.

### 9.2 Scope review

Confirm no:

```text
Protocol Parser
Frame Queue
Async TX
multi-UART
Device Registry
Factory/IoC
runtime shutdown framework
```

### 9.3 Verification record

Record actual results only:

```text
Project Config Header Test         PASS / actual
APP Communication Host Test       PASS / actual
APP System Host Test              PASS / actual
Lower-layer Regression            PASS / actual
Coding Standard Review            PASS / actual
Keil Full Rebuild                 PASS / actual
Production APP Board RX Test      PASS / PENDING
Content Integrity Hook            PASS / PENDING
Cleanup Rebuild                   PASS / PENDING
```

### 9.4 Completion state

Only when all mandatory Host/Keil/Board gates pass:

```text
APP Phase 1                    COMPLETED
Production APP RX Vertical     VERIFIED
Production APP Layer           IMPLEMENTED (Phase 1)
Next Phase                     protocol/application behavior design
Next State                     READY_FOR_NEXT_DESIGN
```

If Board Test is pending:

```text
APP Phase 1                    IMPLEMENTED / HOST_KEIL_VERIFIED
Production APP RX Vertical     BOARD_VERIFICATION_PENDING
```

- [ ] Run final:

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
[ ] Main APP design and Config addendum followed without contract drift
[ ] project_config.h centralizes all frozen product parameters
[ ] 00_Config README documents static-config boundary
[ ] project_config.h has no Impl/HAL/CMSIS/FreeRTOS concrete dependency
[ ] app_system.h/.c implemented
[ ] app_communication.h/.c implemented
[ ] APP owns UART / Service / Thread / DMA / Ring storage
[ ] runtime object/storage pointers remain in app_system, not project_config.h
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
[ ] Project Config Host Test PASS
[ ] APP Host tests PASS
[ ] relevant lower-layer regressions PASS
[ ] Coding Standard Review PASS
[ ] Keil Full Rebuild 0 Error(s)
[ ] Production APP board RX test PASS
[ ] 1280-byte content comparison mismatch = 0
[ ] temporary board-test hook removed
[ ] cleanup Keil Rebuild 0 Error(s)
[ ] handoff updated with actual evidence
```

After completion, stop. Do not begin Protocol Parser, command processing, async TX, or any next APP feature in the same scope without a new design/plan.