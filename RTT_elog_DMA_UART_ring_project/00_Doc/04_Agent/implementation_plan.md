# APP Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the temporary UART consumer test wiring with a production `01_APP/` composition root and Communication Task that consumes UART Service byte-stream data, applies the frozen DATA_LOSS / ERROR recovery policy, and preserves all existing Platform / Service boundaries.

**Architecture:** `app_system` owns the product composition and caller-owned static resources before the scheduler starts. `app_communication` owns the post-scheduler Communication Task behavior: Platform UART lifecycle start, UART Service session start, wait/drain/recovery, minimal APP statistics, and low-frequency logging. APP may depend on Service and Platform, but never directly on Impl, HAL, CMSIS-RTOS2, or FreeRTOS concrete APIs.

**Tech Stack:** C, STM32F411CEU6, Platform BSP UART, Platform UART lifecycle, Platform OS Thread/Time, Platform Log, UART Service, SPSC RingBuffer, CMSIS-RTOS2 + FreeRTOS behind Platform adapters, GCC Host Test, Keil MDK-ARM.

**Spec:** `00_Doc/02_架构设计/APP_Phase1设计.md`

---

## Mandatory References

Before modifying project C/H files, read:

```text
00_Doc/02_架构设计/APP_Phase1设计.md
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

APP must not include or use:

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

Frozen initial product values for this implementation:

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

Other frozen constraints:

- No dynamic allocation.
- `platform_uart_t`, `service_uart_t`, `platform_thread_t`, `app_communication_t`, DMA RX storage, and RingBuffer storage are APP-owned static resources.
- Platform BSP owns only logical-to-concrete UART mapping; current mapping remains `Communication UART -> USART1`.
- UART Service remains sole owner of the active RX Session and the only production caller allowed to cancel that RX Session.
- Notification remains a wake hint; Service/RingBuffer state is truth.
- Drain valid RingBuffer bytes before any recovery that starts a new RX Session.
- Recovery priority is `ERROR > DATA_LOSS > STOPPED` after RX drain.
- `PLATFORM_ERR_TIMEOUT` from `service_uart_wait_event()` is handled as normal idle; `app_communication_process()` returns `PLATFORM_ERR_OK` and keeps APP RUNNING.
- No Protocol Parser, Frame Queue, Async TX, UART echo, multi-UART, Event Bus, Device Registry, Factory, IoC container, Watchdog policy, or product shutdown framework.
- Keep CubeMX `defaultTask`, `USART1_mutex_Init()`, and legacy `fputc()` technical debt in this phase.
- CubeMX source changes are restricted to USER CODE sections except Keil project metadata.
- Preserve unrelated local changes; never use destructive reset/clean commands.
- If implementation requires changing any frozen Platform UART, Platform BSP UART, UART Service, RingBuffer, Platform Notify, or Platform Thread public contract: `STOP / BLOCKED` and return to design review.

---

## File Map

Production files:

```text
01_APP/app_communication.h
    APP Communication public state/config/statistics/object and Task-facing API.

01_APP/app_communication.c
    UART lifecycle start, Service wait/drain/recovery, Task entry, APP statistics/logging.

01_APP/app_system.h
    Product composition entry: app_system_init().

01_APP/app_system.c
    Static UART/Service/Thread/APP objects, backing storage, product configuration, pre-scheduler composition.

Core/Src/freertos.c
    Thin CubeMX USER CODE call to app_system_init().

MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
    APP source/include integration only.
```

Host tests:

```text
Tests/app_communication/test_app_communication.c
Tests/app_system/test_app_system.c
```

---

## Approved APP Public Surface

`app_system.h`:

```c
platform_error_t app_system_init(void);
```

`app_communication.h` must define:

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

#define APP_COMMUNICATION_INITIALIZER {0}
```

Public functions:

```c
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

The status/statistics getters are approved read-only observability APIs for Host/board verification. Do not add clear-statistics, shutdown, restart-command, parser, TX, global-object getter, or direct Service exposure APIs.

---

# Task 0: Preflight, State Synchronization, and Scope Gate

**Files:**
- Read all Mandatory References.
- Modify: `00_Doc/04_Agent/handoff.md`

**Interfaces:**
- Consumes: frozen APP Phase 1 design and completed Platform BSP/UART Service baselines.
- Produces: repository execution state identifying APP Phase 1 as the active implementation phase.

- [ ] Run:

```bash
git status --short
git log --oneline -n 15
```

- [ ] Confirm prerequisites from source + handoff:

```text
Platform BSP UART Binding Phase 1   COMPLETED / VERIFIED
UART Service Phase 1                COMPLETED
Base RX Vertical Slice              VERIFIED
01_APP production implementation    ABSENT
APP Phase 1 design                   FROZEN
```

- [ ] Confirm production APP source does not already exist except the placeholder README:

```text
01_APP/app_system.h          ABSENT
01_APP/app_system.c          ABSENT
01_APP/app_communication.h   ABSENT
01_APP/app_communication.c   ABSENT
```

If any exists, inspect it before proceeding; do not overwrite unknown work.

- [ ] Confirm current `Core/Src/main.c` ordering remains:

```text
MX_GPIO_Init()
MX_DMA_Init()
MX_USART1_UART_Init()
osKernelInitialize()
MX_FREERTOS_Init()
osKernelStart()
```

- [ ] Synchronize only the current-state portion of `handoff.md` to:

```text
Current active phase: APP Phase 1
Current authoritative design: 00_Doc/02_架构设计/APP_Phase1设计.md
Current implementation plan: 00_Doc/04_Agent/implementation_plan.md
Current state: READY_FOR_IMPLEMENTATION
Production APP Layer: NOT IMPLEMENTED
```

Also fix the stale sentence that identifies the current `implementation_plan.md` as an old UART Service/BSP plan. Keep completed-history sections intact.

- [ ] Confirm `Core/Src/freertos.c` still has the current empty defaultTask and USER CODE sections; do not remove the task or legacy mutex initialization.

- [ ] Run:

```bash
git diff --check
```

**Gate:** Any material mismatch between repository reality and the frozen design is `STOP / BLOCKED` before source implementation.

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/00_Doc/04_Agent/handoff.md
git commit -m "docs: start app phase1 implementation"
```

---

# Task 1: APP Communication Public Contract, Init, Status, and Statistics

**Files:**
- Create: `01_APP/app_communication.h`
- Create: `01_APP/app_communication.c`
- Create: `Tests/app_communication/test_app_communication.c`

**Interfaces:**
- Consumes: Platform UART lifecycle/public types and UART Service public types.
- Produces: approved APP Communication model, init, read-only observability, and linkable API surface.

## 1.1 Host fake

- [ ] Build a fake environment in `test_app_communication.c` with at least:

```c
typedef struct
{
    platform_error_t uartInitResult;
    platform_error_t uartStartResult;
    platform_error_t uartStopResult;
    uint32_t uartInitCount;
    uint32_t uartStartCount;
    uint32_t uartStopCount;

    platform_error_t serviceStartResult;
    platform_error_t serviceStopResult;
    platform_error_t serviceWaitResult;
    platform_error_t serviceStatusResult;
    service_uart_status_t serviceStatus;
    uint32_t serviceStartCount;
    uint32_t serviceStopCount;
    uint32_t serviceWaitCount;

    uint32_t waitEvents;
    platform_error_t readResults[8];
    platform_size_t readLengths[8];
    uint32_t readIndex;
    uint32_t readCount;

    uint32_t delayCount;
    uint32_t lastDelayMs;
    uint32_t yieldCount;
} fake_app_communication_t;
```

Provide fake definitions for every external symbol referenced by `app_communication.c` during Host linking:

```text
service_uart_start
service_uart_stop
service_uart_read
service_uart_wait_event
service_uart_get_status
platform_time_delay_ms
platform_thread_yield
Platform_Log_GetOutputFn
```

`Platform_Log_GetOutputFn()` returns a no-op varargs sink.

## 1.2 RED init/getter tests

- [ ] Add:

```text
NULL communication                         -> INVALID_PARAM
NULL config                                -> INVALID_PARAM
NULL config.uart                           -> INVALID_PARAM
NULL config.service                        -> INVALID_PARAM
UART lifecycle pointer NULL                -> NOT_SUPPORTED
UART lifecycle init/start/stop missing     -> NOT_SUPPORTED
valid init                                 -> INITIALIZED
valid init                                 -> config pointers copied exactly
valid init                                 -> lastError OK
valid init                                 -> all APP statistics zero
second init                                -> ALREADY_INITIALIZED
get_status after init                      -> INITIALIZED / OK
get_statistics after init                  -> all zero
NULL getter arguments                      -> INVALID_PARAM
```

- [ ] Run RED before production implementation exists:

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  -I01_APP \
  -I02_Service/service_uart \
  -I02_Service/service_common \
  -I03_Platform/platform_bsp \
  -I03_Platform/platform_mcu/uart \
  -I03_Platform/platform_os \
  -I03_Platform/platform_middleware \
  -I03_Platform/platform_common \
  -I04_Impl/impl_board \
  Tests/app_communication/test_app_communication.c \
  -o Tests/app_communication/test_app_communication
```

Expected: compile/link FAIL because APP Communication production contract/implementation is absent.

## 1.3 GREEN implementation

- [ ] Implement `app_communication_init()`:

```text
validate communication/config/uart/service
require state == UNINITIALIZED
require uart->device.lifecycle != NULL
require lifecycle init/start/stop != NULL
copy config
state = INITIALIZED
lastError = PLATFORM_ERR_OK
zero APP statistics
```

Do not inspect `uart->implContext`, Service internal Context, HAL state, or RTOS native handles.

- [ ] Implement status/statistics getters as plain snapshots with no locks and no mutation.

- [ ] Keep start/process/task-entry behavior unimplemented beyond what is required for linkability until their RED tests are introduced.

- [ ] Run GREEN, `git diff --check`, Coding Standard Review.

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/01_APP/app_communication.h \
        RTT_elog_DMA_UART_ring_project/01_APP/app_communication.c \
        RTT_elog_DMA_UART_ring_project/Tests/app_communication/test_app_communication.c
git commit -m "feat(app): add communication object"
```

---

# Task 2: Post-Scheduler Communication Start Sequence

**Files:**
- Modify: `01_APP/app_communication.c`
- Modify: `Tests/app_communication/test_app_communication.c`

**Interfaces:**
- Consumes: initialized APP Communication object.
- Produces: frozen `UART lifecycle init -> UART lifecycle start -> service_uart_start` runtime sequence.

## 2.1 RED tests

- [ ] Make fake lifecycle functions append call IDs:

```text
1 = UART lifecycle init
2 = UART lifecycle start
3 = service_uart_start
4 = UART lifecycle stop
```

- [ ] Add:

```text
UNINITIALIZED start
    -> NOT_INITIALIZED

INITIALIZED + all success
    -> call order 1,2,3
    -> APP state RUNNING
    -> lastError OK

UART init failure
    -> return original init error
    -> no UART start / Service start
    -> APP ERROR
    -> fatalErrorCount == 1

UART start failure
    -> return original start error
    -> no Service start
    -> UART stop not called
    -> APP ERROR
    -> fatalErrorCount == 1

Service start failure after UART STARTED
    -> call order 1,2,3,4
    -> UART stop attempted once
    -> return original Service start error even if UART stop fails
    -> APP ERROR
    -> fatalErrorCount == 1

start from RUNNING / ERROR
    -> INVALID_STATE
    -> no lifecycle calls
```

## 2.2 GREEN implementation

- [ ] Add one private fatal-transition helper:

```text
state = APP_COMMUNICATION_STATE_ERROR
lastError = originalError
fatalErrorCount++
return originalError
```

It may emit one Platform Log error when entering ERROR; do not log repeatedly in error idle.

- [ ] Implement exact sequence:

```text
lifecycle->init(uart)
    failure -> APP ERROR

lifecycle->start(uart)
    failure -> APP ERROR

service_uart_start(service)
    failure -> best-effort lifecycle->stop(uart)
               preserve original Service start error
               APP ERROR

success -> APP RUNNING
```

- [ ] Do not call `service_uart_init()` or Platform async RX directly here.

- [ ] Run full APP Communication test GREEN, `git diff --check`, Coding Standard Review.

**Commit:** `feat(app): start communication runtime`

---

# Task 3: RX Drain, Combined Events, Recovery, and One-Cycle Process

**Files:**
- Modify: `01_APP/app_communication.c`
- Modify: `Tests/app_communication/test_app_communication.c`

**Interfaces:**
- Consumes: UART Service `wait_event/read/get_status/start/stop` APIs.
- Produces: deterministic one-cycle `app_communication_process()`.

Private constant:

```c
#define APP_COMMUNICATION_READ_BUFFER_SIZE (128U)
```

## 3.1 RED: wait and drain

- [ ] Script fake `service_uart_read()` results/lengths and fill successful output with deterministic bytes.

- [ ] Add:

```text
process while not RUNNING
    -> INVALID_STATE

wait TIMEOUT
    -> process returns OK
    -> APP remains RUNNING
    -> no restart/stop

RX_AVAILABLE, one successful read then EMPTY
    -> two read calls
    -> processedChunkCount += 1
    -> processedByteCount += readLength

RX_AVAILABLE, three successful reads then EMPTY
    -> drain all four calls
    -> chunk count += 3
    -> byte count == sum(readLength)

successful read with readLength == 0
    -> APP ERROR / INVALID_STATE
    -> prevents infinite drain loop

read returns non-EMPTY error
    -> APP ERROR using original read error

wait returns non-TIMEOUT error
    -> APP ERROR using original wait error

wait returns OK with events == 0
    -> normal no-op cycle
```

Phase 1 byte consumption is only successful-byte accounting. Do not infer frame boundaries.

## 3.2 RED: ERROR recovery

- [ ] Add:

```text
RX_AVAILABLE | ERROR
    -> complete drain before get_status/start

ERROR status.state == ERROR + restart success
    -> no Service stop/cancel
    -> Service start exactly once
    -> uartErrorRecoveryCount++
    -> APP remains RUNNING
    -> APP lastError records Service status lastError

Service get_status failure
    -> APP ERROR using status error

Service status not ERROR
    -> APP ERROR / INVALID_STATE

ERROR restart failure
    -> APP ERROR using restart error

RX_AVAILABLE | DATA_LOSS | ERROR
    -> drain first
    -> ERROR recovery only
    -> Service stop count == 0
    -> dataLossRecoveryCount unchanged
```

## 3.3 RED: DATA_LOSS recovery

- [ ] Add:

```text
RX_AVAILABLE | DATA_LOSS
    -> drain first
    -> Service stop
    -> Service get_status
    -> Service start

stop OK + status STOPPED + restart OK
    -> dataLossRecoveryCount++
    -> APP RUNNING

stop error + status STOPPED
    -> still restart
    -> successful restart keeps APP RUNNING
    -> dataLossRecoveryCount++

stop error + status not STOPPED
    -> APP ERROR using original stop error
    -> no restart

stop OK + status not STOPPED
    -> APP ERROR / INVALID_STATE
    -> no restart

status query failure after stop error
    -> APP ERROR using original stop error

status query failure after stop OK
    -> APP ERROR using status query error

restart failure
    -> APP ERROR using restart error
```

DATA_LOSS is sticky; “log and continue waiting” is forbidden.

## 3.4 RED: STOPPED

- [ ] Add:

```text
standalone STOPPED
    -> APP ERROR
    -> lastError = PLATFORM_ERR_CANCELED
    -> no automatic restart
```

## 3.5 GREEN process order

- [ ] Implement exactly:

```text
service_uart_wait_event(timeout)
    TIMEOUT -> return OK
    other failure -> APP ERROR

if RX_AVAILABLE:
    service_uart_read() repeatedly until PLATFORM_ERR_EMPTY
    count each successful non-zero chunk

if ERROR:
    require Service status == ERROR
    record/log Service lastError
    service_uart_start()
    success -> uartErrorRecoveryCount++ -> return OK
    failure -> APP ERROR

else if DATA_LOSS:
    stopResult = service_uart_stop()
    statusResult = service_uart_get_status()
    resolve true STOPPED state even when stopResult != OK
    require STOPPED
    service_uart_start()
    success -> dataLossRecoveryCount++ -> return OK
    failure -> APP ERROR

else if STOPPED:
    APP ERROR / PLATFORM_ERR_CANCELED

otherwise -> OK
```

RX handling is independent and happens first. Recovery precedence after drain is `ERROR`, then `DATA_LOSS`, then standalone `STOPPED`.

- [ ] APP must never call `platform_uart_cancel()`.

- [ ] Run all APP Communication Host tests GREEN, `git diff --check`, Coding Standard Review.

**Commit:** `feat(app): process uart service events`

---

# Task 4: Communication Task Entry and Fatal Error Idle

**Files:**
- Modify: `01_APP/app_communication.c`
- Modify: `Tests/app_communication/test_app_communication.c` only for finite helper/state behavior; do not add production loop-escape APIs.

**Interfaces:**
- Consumes: `app_communication_start/process`, Platform Time, Platform Thread yield.
- Produces: production Communication Task entry.

Private constants:

```c
#define APP_COMMUNICATION_WAIT_TIMEOUT_MS     (1000U)
#define APP_COMMUNICATION_ERROR_IDLE_DELAY_MS (1000U)
```

- [ ] Implement:

```text
argument == NULL -> return

app_communication_start()
    success -> normal process loop
    failure -> error idle

while state == RUNNING:
    app_communication_process(communication, 1000U)

error idle forever:
    platform_time_delay_ms(1000U)
    if delay fails -> platform_thread_yield() once before next retry
```

- [ ] Do not print every RX chunk.
- [ ] Log only start success, recovery, recovery failure/unexpected stop, and fatal transition through Platform Log.
- [ ] Do not add CMSIS/FreeRTOS calls.
- [ ] Do not add retry/start storms after APP enters ERROR.
- [ ] Infinite-loop behavior is primarily board-tested; Host tests continue to validate `start()` and `process()` directly.
- [ ] `git diff --check` + Coding Standard Review.

**Commit:** `feat(app): add communication task entry`

---

# Task 5: APP System Composition Root

**Files:**
- Create: `01_APP/app_system.h`
- Create: `01_APP/app_system.c`
- Create: `Tests/app_system/test_app_system.c`

**Interfaces:**
- Produces public `platform_error_t app_system_init(void);` only.

## 5.1 Frozen static resources/config

`app_system.c` owns:

```c
static platform_uart_t g_communicationUart = PLATFORM_UART_INITIALIZER;
static service_uart_t g_uartService = SERVICE_UART_INITIALIZER;
static platform_thread_t g_communicationThread = {0};
static app_communication_t g_appCommunication = APP_COMMUNICATION_INITIALIZER;
static uint8_t g_uartDmaRxBuffer[128U];
static uint8_t g_uartRingBufferStorage[512U];
```

UART config:

```c
static const platform_uart_config_t g_communicationUartConfig = {
    115200U,
    PLATFORM_UART_DATA_BITS_8,
    PLATFORM_UART_STOP_BITS_1,
    PLATFORM_UART_PARITY_NONE,
    PLATFORM_UART_FLOW_CONTROL_NONE,
    1000U
};
```

Thread config must resolve exactly to:

```text
name            = "communication"
entry           = app_communication_task_entry
argument        = &g_appCommunication
stackSizeBytes  = 1024U
priority        = PLATFORM_THREAD_PRIORITY_NORMAL
```

## 5.2 RED Host fake/tests

- [ ] Fake and record:

```text
platform_bsp_uart_construct_communication
app_communication_init
platform_thread_create
service_uart_init
platform_thread_terminate
```

Ordered IDs:

```text
1 BSP construct
2 APP Communication init
3 Platform Thread create
4 UART Service init
5 Platform Thread terminate (rollback only)
```

- [ ] Add:

```text
all success
    -> exact order 1,2,3,4
    -> UART config exact 115200 8N1 no flow / 1000 ms
    -> APP config references same captured UART + Service
    -> Thread entry/argument/1024/NORMAL exact
    -> Service config references same UART + created Thread
    -> DMA buffer size 128
    -> RingBuffer storage size 512

BSP construct failure
    -> return original error
    -> no later call

APP Communication init failure
    -> return original error
    -> no Thread/Service creation

Thread create failure
    -> return original error
    -> no Service init

Service init failure
    -> terminate created Communication Thread once
    -> return original Service init error even if terminate fails
```

`app_system_init()` is a boot-time one-shot contract. Do not invent UART “unconstruct” or general shutdown APIs in this phase.

## 5.3 GREEN implementation

- [ ] Implement exact pre-scheduler sequence:

```text
platform_bsp_uart_construct_communication()
app_communication_init()
platform_thread_create()
service_uart_init()
return OK
```

Service config:

```text
uart                  = &g_communicationUart
dmaRxBuffer           = g_uartDmaRxBuffer
dmaRxBufferSize       = 128
ringBufferStorage     = g_uartRingBufferStorage
ringBufferStorageSize = 512
consumerThread        = &g_communicationThread
```

On `service_uart_init()` failure only, best-effort terminate the already-created Communication Thread. Preserve the original Service init error.

- [ ] `app_system.c` may include only APP / Service / Platform headers. No Impl/HAL/CMSIS/FreeRTOS headers.
- [ ] Run Host test GREEN, `git diff --check`, Coding Standard Review.

**Commit:** `feat(app): add system composition root`

---

# Task 6: CubeMX Thin Entry, Regressions, and Keil Integration

**Files:**
- Modify: `Core/Src/freertos.c` USER CODE sections only.
- Modify: `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx`

## 6.1 CubeMX glue

- [ ] Add to USER CODE Includes:

```c
#include "app_system.h"
```

- [ ] Add to `MX_FREERTOS_Init()` USER CODE Init:

```c
if (PLATFORM_ERR_OK != app_system_init()) {
    Error_Handler();
}
```

The required runtime order remains:

```text
osKernelInitialize()
MX_FREERTOS_Init() -> app_system_init()
osKernelStart()
Communication Task -> app_communication_start()
```

- [ ] Do not move `app_system_init()` into `main.c`.
- [ ] Do not remove `defaultTask` or `USART1_mutex_Init()`.
- [ ] APP itself must never call `Error_Handler()`.

## 6.2 Keil project

- [ ] Add include path:

```text
01_APP
```

- [ ] Add sources:

```text
01_APP/app_system.c
01_APP/app_communication.c
```

Use one APP group consistent with existing project organization; do not reorganize unrelated groups.

## 6.3 Regression

- [ ] Build/run new APP tests:

```text
Tests/app_communication/test_app_communication
Tests/app_system/test_app_system
```

- [ ] Rebuild/run relevant existing regressions at minimum:

```text
Tests/platform_bsp_uart/test_platform_bsp_uart
Tests/platform_uart/test_platform_uart
Tests/service_uart/test_service_uart
Platform OS host regression used by the repository
RingBuffer host regression
```

Use existing repository compile commands/scripts where present.

- [ ] Confirm no production modifications occurred in:

```text
02_Service/service_uart/
03_Platform/platform_bsp/
03_Platform/platform_mcu/uart/
03_Platform/platform_os/
04_Impl/
```

If required, STOP / BLOCKED.

## 6.4 Keil gate

- [ ] Perform Keil Full Rebuild.

Required evidence:

```text
0 Error(s)
```

Review new warnings; do not weaken warning settings.

- [ ] `git diff --check` + Coding Standard Review.

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/Core/Src/freertos.c \
        RTT_elog_DMA_UART_ring_project/MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
git commit -m "build: integrate app phase1"
```

---

# Task 7: Target Board Production APP RX Verification

**Goal:** Prove the previous temporary Consumer Task has been replaced by the production `01_APP/` path.

## 7.1 Temporary content hook

- [ ] Add one clearly marked temporary board-test hook at the APP byte-consumption seam in `app_communication.c`.

Temporary state:

```text
expectedByte: uint8_t, starts at 0
comparedByteCount: uint32_t, starts at 0
mismatchCount: uint32_t, starts at 0
```

For each consumed byte:

```text
if data[i] != expectedByte:
    mismatchCount++
expectedByte++            // uint8_t natural wrap
comparedByteCount++
```

Task Context only. No lower-layer or ISR modifications.

- [ ] Expose/print temporary summary only through a localized board-test path using Platform Log/RTT. Do not convert the test-pattern counters into permanent APP public API.

## 7.2 Board stimulus/evidence

- [ ] Rebuild, flash, and send raw binary:

```text
00..FF repeated 5 times = 1280 bytes
```

Required evidence:

```text
APP Communication state        RUNNING
UART Service state             RUNNING
APP processedByteCount         1280
temporary comparedByteCount    1280
temporary mismatchCount        0
Service rxBytesReceived        1280
Service rxBytesDropped         0
DATA_LOSS                      not observed
ERROR                          not observed
```

Do not infer PASS from prints alone; record actual counters.

## 7.3 Cleanup

- [ ] Remove the temporary expected-byte/compare logic completely.
- [ ] Keil Full Rebuild again.
- [ ] Require `0 Error(s)` after cleanup.
- [ ] Confirm production `app_communication.c` is again generic byte-stream consumption/statistics only.

If target hardware is unavailable, record `BOARD_VERIFICATION_PENDING` and do not mark APP Phase 1 COMPLETED.

---

# Task 8: Final Boundary Review and Handoff

**Files:**
- Modify: `00_Doc/04_Agent/handoff.md`

## 8.1 Dependency/scope review

- [ ] Search `01_APP/` and confirm absence of:

```text
impl_platform_uart.h
impl_freertos
usart.h
stm32f4xx_hal
cmsis_os
FreeRTOS.h
task.h
UART_HandleTypeDef
DMA_HandleTypeDef
huart1
USART1
```

- [ ] Confirm public APP API is exactly:

```text
app_system_init
app_communication_init
app_communication_start
app_communication_process
app_communication_get_status
app_communication_get_statistics
app_communication_task_entry
```

- [ ] Confirm no Protocol Parser, Frame Queue, Async TX, multi-UART, registry/factory, or shutdown framework was introduced.

## 8.2 Verification record

Record only actual evidence:

```text
APP Communication Host Test      PASS / actual
APP System Host Test             PASS / actual
Platform BSP UART Regression     PASS / actual
Platform UART Regression         PASS / actual
UART Service Regression          PASS / actual
Platform OS Regression           PASS / actual
RingBuffer Regression            PASS / actual
Keil Full Rebuild                PASS / actual
APP Production RX Board Test     PASS / PENDING
Content Integrity Hook           PASS / PENDING
Cleanup Rebuild                  PASS / PENDING
Coding Standard Review           PASS / actual
```

Never turn NOT RUN into PASS.

## 8.3 Final handoff state

Only after all mandatory Host/Keil/Board gates pass:

```text
APP Phase 1                         COMPLETED
Production APP RX Vertical Slice    VERIFIED
Production APP Layer                IMPLEMENTED (Phase 1)
Next Phase                          Protocol / APP Phase 2 Design
Next State                          READY_FOR_NEXT_DESIGN
```

If Board verification is pending:

```text
APP Phase 1                         IMPLEMENTED / HOST_KEIL_VERIFIED
Production APP RX Vertical Slice    BOARD_VERIFICATION_PENDING
```

Stable contracts to record:

```text
APP may depend on Service and Platform; APP may not depend on Impl.
app_system is the product Composition Root.
Communication Task lifecycle is APP-owned and created through Platform Thread.
Pre-scheduler composition is separate from post-scheduler UART/Service runtime start.
APP drains valid RX bytes before ERROR/DATA_LOSS recovery.
ERROR recovery = direct new Service session.
DATA_LOSS recovery = stop / resolve STOPPED / start new Service session.
Protocol parsing and async TX remain outside Phase 1.
```

- [ ] Final:

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
[ ] RX drain until PLATFORM_ERR_EMPTY verified
[ ] wait timeout returns APP process OK and remains normal idle
[ ] Combined event precedence verified
[ ] DATA_LOSS stop/status/start recovery verified
[ ] UART ERROR direct restart recovery verified
[ ] unexpected STOPPED fatal behavior verified
[ ] no fast retry / busy-loop fatal path
[ ] APP does not duplicate Service RX statistics
[ ] APP has no direct Impl/HAL/CMSIS/FreeRTOS dependency
[ ] APP Communication Host Test PASS
[ ] APP System Host Test PASS
[ ] relevant lower-layer regressions PASS
[ ] Coding Standard Review PASS
[ ] CubeMX integration remains thin and USER CODE scoped
[ ] Keil Full Rebuild 0 Error(s)
[ ] Production APP 1280-byte RX board test PASS
[ ] content comparison mismatch == 0
[ ] temporary board-test hook removed
[ ] cleanup Keil Rebuild 0 Error(s)
[ ] handoff updated with actual evidence
```

After completion, stop. Do not begin Protocol Parser, command processing, async TX, or APP Phase 2 in the same execution scope.
