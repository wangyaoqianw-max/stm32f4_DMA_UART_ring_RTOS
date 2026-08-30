# UART Service Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a single-UART RX Service that copies Platform UART DMA RX events into the existing SPSC RingBuffer, wakes one dedicated Communication Task through Platform Notify, exposes read/status/statistics APIs, and preserves explicit lifecycle/error/data-loss semantics.

**Architecture:** `02_Service/service_uart/` owns the RX Service state machine, RingBuffer integration, statistics, and Task wakeup. APP owns Platform UART hardware lifecycle, Task lifecycle, and all backing storage. Platform UART gains exactly one approved callback-binding API so Service can own the asynchronous event sink without exposing callback wiring to APP.

**Tech Stack:** C, STM32F411CEU6, STM32 HAL, Platform UART, SPSC RingBuffer, Platform OS Thread Notification, CMSIS-RTOS2 / FreeRTOS, ARMCC5 / Keil MDK-ARM, GCC Host Test.

**Spec:** `00_Doc/02_架构设计/UART_Service_Phase1设计.md`

## Mandatory References

Before modifying project C/H files, read:

```text
00_Doc/02_架构设计/UART_Service_Phase1设计.md
00_Doc/02_架构设计/UART_Phase2A_DMA_RX设计.md
00_Doc/02_架构设计/RingBuffer_SPSC设计.md
00_Doc/02_架构设计/RTOS_Platform_OS设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/handoff.md
03_Platform/platform_mcu/uart/platform_uart.h
03_Platform/platform_mcu/uart/platform_uart_types.h
03_Platform/platform_os/platform_notify.h
03_Platform/platform_os/platform_os_types.h
02_Service/service_common/ring_buffer.h
```

Preflight report must explicitly contain:

```text
Coding Standard:
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
Status: READ

Agent Execution Rules:
00_Doc/04_Agent/execution_rules.md
Status: READ

UART Service Phase 1 Design:
00_Doc/02_架构设计/UART_Service_Phase1设计.md
Status: READ / FROZEN
```

## Global Constraints

- Dependency remains `APP -> Service -> Platform -> Impl -> Vendor / HAL / RTOS / Hardware`.
- Phase 1 assumes one Platform UART, one UART Service, one dedicated Consumer Task.
- Consumer Task Thread Flags are exclusively owned by this UART Service in Phase 1.
- APP owns Communication Task creation/destruction, Platform UART hardware lifecycle, DMA RX storage, RingBuffer storage, and `service_uart_t` storage.
- Service owns the active RX Session and is the only caller allowed to cancel that RX Session.
- No dynamic allocation.
- No HAL/CMSIS/FreeRTOS concrete type may enter UART Service public API.
- RX callback is ISR-sensitive: no blocking, malloc/free, protocol parsing, large logging, automatic restart, or APP callback.
- RingBuffer remains the existing SPSC implementation; do not redesign its API or `N - 1` capacity rule.
- Notification is only a wake hint; RingBuffer readable size + Service runtime state are the truth.
- `SERVICE_UART_EVENT_DATA_LOSS` is sticky for the current RX Session.
- `service_uart_start()` begins a new Session and resets RingBuffer/runtime session status, but does not clear cumulative statistics.
- Stop/error does not clear already buffered data.
- Statistics are best-effort snapshots; do not add locks or IRQ masking only to create a transactional statistics snapshot.
- The only approved Platform UART public API extension is `platform_uart_set_callback()`.
- Async TX Service, protocol parsing, Frame Queue, multi-UART aggregation, Service-created Task, auto-recovery, and `service_log` are out of scope.
- Preserve unrelated local changes. Never use destructive reset/clean commands to simplify implementation.

---

## Task 0: Preflight, Current-State Sync, and Scope Gate

**Files:**
- Read all Mandatory References.
- Modify: `00_Doc/04_Agent/handoff.md`

**Interfaces:**
- Consumes: frozen UART Service design.
- Produces: repository current-state metadata that no longer identifies RingBuffer Phase 1 as the active implementation phase.

- [ ] Run:

```bash
git status --short
git log --oneline -n 15
```

- [ ] Confirm completed prerequisites from source + handoff:

```text
UART Phase 2A          COMPLETED
RTOS Platform Phase 1  COMPLETED
RingBuffer Phase 1     COMPLETED
```

- [ ] Confirm production UART Service does not already exist:

```text
02_Service/service_uart/service_uart.h   ABSENT
02_Service/service_uart/service_uart.c   ABSENT
```

An empty or absent `service_uart/` directory is acceptable.

- [ ] Synchronize `handoff.md` current-state sections before production code:

```text
Current active phase: UART Service Phase 1
Current state: READY_FOR_IMPLEMENTATION
Current authoritative design: 00_Doc/02_架构设计/UART_Service_Phase1设计.md
Current implementation plan: 00_Doc/04_Agent/implementation_plan.md
```

Also amend the historical “Platform UART public API frozen” wording to record the already-approved single exception:

```text
UART Service Phase 1 may add platform_uart_set_callback();
all other Platform UART public API semantics remain frozen.
```

Replace the active RingBuffer-only scope guard with the UART Service Phase 1 scope from the frozen design; keep RingBuffer completion history intact.

- [ ] If `architecture.md` contains a direct semantic prohibition that cannot coexist with the frozen UART Service design, STOP / BLOCKED and report the exact conflict. Do not silently rewrite architecture during implementation.

- [ ] Run:

```bash
git diff --check
```

**Deliverable:** repository execution state synchronized; no production source changed.

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/00_Doc/04_Agent/handoff.md
git commit -m "docs: start uart service phase1"
```

---

## Task 1: Add Controlled Platform UART Callback Binding

**Files:**
- Modify: `03_Platform/platform_mcu/uart/platform_uart.h`
- Modify: `03_Platform/platform_mcu/uart/platform_uart.c`
- Modify: `Tests/platform_uart/test_platform_uart.c`

**Interfaces:**
- Consumes: existing `platform_uart_t.callback` / `callbackContext` fields.
- Produces:

```c
platform_error_t platform_uart_set_callback(
    platform_uart_t *uart,
    platform_uart_callback_t callback,
    void *callbackContext);
```

Frozen rules:

```text
constructed UART required
STARTED -> PLATFORM_ERR_INVALID_STATE
callback == NULL -> unbind and clear callbackContext
CREATED / INITIALIZED / STOPPED / ERROR -> binding allowed
no RX_DATA / ERROR / CANCELED semantic changes
```

- [ ] Add failing tests covering:

```text
NULL uart                              -> INVALID_PARAM
zero/unconstructed UART                -> NOT_INITIALIZED
constructed CREATED + bind             -> OK, fields updated
constructed CREATED + unbind           -> OK, callback/context NULL
STARTED + attempt replace               -> INVALID_STATE, old binding unchanged
STOPPED + unbind                        -> OK
```

Test the public API; do not require Service code yet.

- [ ] Run Platform UART test and verify RED. Use:

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  -I03_Platform/platform_mcu/uart \
  -I03_Platform/platform_common \
  -I04_Impl/impl_board \
  Tests/platform_uart/test_platform_uart.c \
  03_Platform/platform_mcu/uart/platform_uart.c \
  03_Platform/platform_common/platform_device.c \
  03_Platform/platform_common/platform_object.c \
  -o Tests/platform_uart/test_platform_uart
```

Expected before implementation: compile or test FAIL because the new API is absent.

- [ ] Implement a private “constructed UART object” validator instead of abusing `platform_uart_validate_ready()`, because callback binding is deliberately allowed before STARTED.

Required logic:

```c
if (uart == NULL) {
    return PLATFORM_ERR_INVALID_PARAM;
}

if ((PLATFORM_TRUE != platform_object_is_valid(
         &uart->device.object, PLATFORM_OBJECT_DEVICE)) ||
    (PLATFORM_DEVICE_CLASS_UART != uart->device.dev_class)) {
    return PLATFORM_ERR_NOT_INITIALIZED;
}

if (PLATFORM_OBJECT_STARTED == uart->device.object.state) {
    return PLATFORM_ERR_INVALID_STATE;
}

uart->callback = callback;
uart->callbackContext = (callback == NULL) ? NULL : callbackContext;
return PLATFORM_ERR_OK;
```

Use repository naming/comment rules rather than copying this snippet mechanically if the coding standard requires formatting changes.

- [ ] Run GREEN, then:

```bash
./Tests/platform_uart/test_platform_uart
git diff --check
```

- [ ] Coding Standard Review. Confirm this task did not alter read/write/read_async/cancel/event semantics.

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.h \
        RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.c \
        RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart.c
git commit -m "feat(uart): add callback binding api"
```

---

## Task 2: UART Service Public Contract, Data Model, Init, and Deinit

**Files:**
- Create: `02_Service/service_uart/service_uart.h`
- Create: `02_Service/service_uart/service_uart.c`
- Create: `Tests/service_uart/test_service_uart.c`

**Interfaces:**
- Consumes: `platform_uart_set_callback()`, `ring_buffer_init()`.
- Produces the frozen types and APIs from the design.

Public types must include:

```c
typedef enum
{
    SERVICE_UART_STATE_UNINITIALIZED = 0,
    SERVICE_UART_STATE_INITIALIZED,
    SERVICE_UART_STATE_RUNNING,
    SERVICE_UART_STATE_STOPPING,
    SERVICE_UART_STATE_STOPPED,
    SERVICE_UART_STATE_ERROR,
    SERVICE_UART_STATE_MAX
} service_uart_state_t;

typedef struct
{
    platform_uart_t *uart;
    uint8_t *dmaRxBuffer;
    platform_size_t dmaRxBufferSize;
    uint8_t *ringBufferStorage;
    platform_size_t ringBufferStorageSize;
    platform_thread_t *consumerThread;
} service_uart_config_t;

typedef struct
{
    volatile service_uart_state_t state;
    ring_buffer_t rxRingBuffer;
    volatile platform_error_t lastError;
    volatile platform_bool_t dataLossOccurred;
} service_uart_context_t;
```

Statistics fields must be exactly:

```text
rxEventCount
rxBytesReceived
rxBytesBuffered
rxBytesRead
rxBytesDropped
ringBufferOverflowCount
ringBufferHighWaterMark
uartErrorCount
cancelCount
```

Also provide `service_uart_status_t`, `service_uart_t`, `SERVICE_UART_INITIALIZER`, and public Service Event macros from the frozen design.

- [ ] In `test_service_uart.c`, provide test doubles for the Platform functions used by Service. The fake must record callback binding and allow the test to invoke the bound callback explicitly:

```c
typedef struct
{
    platform_uart_callback_t callback;
    void *callbackContext;
    uint8_t *rxBuffer;
    platform_size_t rxBufferSize;
    platform_error_t setCallbackResult;
    platform_error_t readAsyncResult;
    platform_error_t cancelResult;
    uint32_t notifySetCount;
    uint32_t notifySetFromIsrCount;
} fake_service_platform_t;
```

- [ ] Add failing init/deinit tests:

```text
service/config required
uart required
dmaRxBuffer required
dmaRxBufferSize > 0
ringBufferStorage required
ringBufferStorageSize >= 2
consumerThread required
valid init -> INITIALIZED
valid init -> config copied
valid init -> RingBuffer initialized
valid init -> callback bound with callbackContext == service
second init -> ALREADY_INITIALIZED
callback bind failure -> init rolls back to UNINITIALIZED
```

Deinit tests:

```text
INITIALIZED -> unbind + clear Service -> UNINITIALIZED
STOPPED -> allowed when Platform setter permits unbind
ERROR -> allowed when Platform setter permits unbind
RUNNING / STOPPING -> INVALID_STATE
unbind failure -> Service object remains intact
external DMA/RingBuffer storage bytes are not cleared/freed
```

- [ ] Run RED with:

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  -I02_Service/service_uart \
  -I02_Service/service_common \
  -I03_Platform/platform_mcu/uart \
  -I03_Platform/platform_os \
  -I03_Platform/platform_common \
  -I04_Impl/impl_board \
  Tests/service_uart/test_service_uart.c \
  02_Service/service_uart/service_uart.c \
  02_Service/service_common/ring_buffer.c \
  -o Tests/service_uart/test_service_uart
```

- [ ] Implement `service_uart_init()` transactionally:

```text
validate all config fields
verify service is UNINITIALIZED
initialize internal RingBuffer
copy Config
initialize runtime status/statistics
bind Platform UART callback
only then publish INITIALIZED
on failure -> restore zero/UNINITIALIZED Service object
```

- [ ] Implement `service_uart_deinit()` so it only clears Service after callback unbind succeeds. Do not call Platform UART hardware lifecycle, destroy the Task, free storage, or clear external buffers.

- [ ] Run GREEN + `git diff --check` + Coding Standard Review.

**Commit:** `feat(service-uart): add service object lifecycle`

---

## Task 3: Start, Stop, Restart, and RX Session Ownership

**Files:**
- Modify: `02_Service/service_uart/service_uart.c`
- Modify: `Tests/service_uart/test_service_uart.c`

**Interfaces:**
- Produces:

```c
platform_error_t service_uart_start(service_uart_t *service);
platform_error_t service_uart_stop(service_uart_t *service);
```

- [ ] Add failing start tests:

```text
UNINITIALIZED -> NOT_INITIALIZED
INITIALIZED -> read_async called with configured DMA buffer/size
STOPPED -> new Session allowed
ERROR -> new Session allowed
RUNNING / STOPPING -> INVALID_STATE
start resets RingBuffer
start clears dataLossOccurred for new Session
start clears Session runtime error
start does NOT clear cumulative statistics
read_async failure -> previous safe state restored and error returned
```

`service_uart_start()` must publish RUNNING before invoking `platform_uart_read_async()` so an immediately arriving RX callback is not incorrectly discarded. Preserve the previous safe state and restore it if `read_async()` fails.

- [ ] Configure fake `platform_uart_cancel()` to synchronously invoke the registered CANCELED callback before returning, matching the current STM32 Impl contract.

- [ ] Add failing stop tests:

```text
RUNNING -> STOPPING before cancel
cancel synchronously emits CANCELED
CANCELED during STOPPING -> STOPPED
cancel returns OK only after callback
service stop then calls platform_notify_set() in Task Context
stop returns with state STOPPED
cancel failure -> STOPPING rolls back to RUNNING
stop from INITIALIZED / STOPPED / ERROR -> INVALID_STATE
CANCELED callback itself does not call notify
```

- [ ] Add restart test:

```text
start -> RX data buffered -> stop
STOPPED read may drain old data
start again -> old unread RingBuffer data is reset/discarded
statistics remain cumulative
new RX Session becomes RUNNING
```

- [ ] Add unexpected CANCELED test:

```text
CANCELED while not STOPPING -> state STOPPED
cancelCount++
callback does not call Task/ISR notify
```

- [ ] Run RED, implement minimal state-machine logic, then GREEN.

- [ ] Verify APP/other layers never call `platform_uart_cancel(RX)` for an active Service-owned Session in production changes.

- [ ] `git diff --check` + Coding Standard Review.

**Commit:** `feat(service-uart): add rx session lifecycle`

---

## Task 4: RX_DATA -> RingBuffer -> Statistics -> ISR Wake

**Files:**
- Modify: `02_Service/service_uart/service_uart.c`
- Modify: `Tests/service_uart/test_service_uart.c`

**Interfaces:**
- Consumes: bound `platform_uart_callback_t`, `ring_buffer_write()`, `ring_buffer_get_readable_size()`, `platform_notify_set_from_isr()`.
- Produces: SPSC Producer behavior and RX statistics.

- [ ] Add failing normal RX test:

```text
state RUNNING
event RX_DATA length 5
RingBuffer receives exact 5 bytes in order
rxEventCount       += 1
rxBytesReceived    += 5
rxBytesBuffered    += 5
rxBytesDropped     += 0
notify_set_from_isr count += 1
```

- [ ] Add failing Partial Write test. Use a deliberately small RingBuffer so only a prefix fits:

```text
incoming             = 10
written              = 4
rxBytesReceived      += 10
rxBytesBuffered      += 4
rxBytesDropped       += 6
ringBufferOverflowCount += 1
dataLossOccurred     = PLATFORM_TRUE
notify_set_from_isr  called
old unread bytes are preserved
```

- [ ] Add failing full-drop test:

```text
writtenLength == 0
all new bytes counted in rxBytesDropped
ringBufferOverflowCount++
dataLossOccurred == TRUE
notify_set_from_isr still called so Consumer can observe DATA_LOSS
```

- [ ] Add High Water Mark tests:

```text
highWaterMark updates after successful/partial writes
highWaterMark never decreases after reads
highWaterMark <= ringBufferStorageSize - 1
start/stop/start does not clear highWaterMark
clear_statistics later will clear it
```

- [ ] Verify the accounting invariant with non-wrapping test values:

```text
rxBytesReceived == rxBytesBuffered + rxBytesDropped
```

- [ ] Add callback-state tests:

```text
RX_DATA in RUNNING  -> processed
RX_DATA in STOPPING / STOPPED / ERROR -> not written
```

- [ ] Implement callback RX handling in this order:

```text
validate callbackContext/event/uart identity
require RUNNING
account received event
ring_buffer_write immediately while event.data is valid
account buffered/dropped/overflow
derive readable size and update HighWaterMark
wake Consumer with platform_notify_set_from_isr()
return from callback without parsing/logging/restart
```

The Notify return value has no safe synchronous recovery path inside this ISR callback. Handle it explicitly according to the coding standard: do not block, retry-loop, or transition hardware lifecycle from ISR. If the implementation intentionally discards the return after the call, document that engineering reason locally and record any required coding-standard exception in handoff.

- [ ] Run GREEN + `git diff --check` + Coding Standard Review.

**Commit:** `feat(service-uart): buffer rx events and statistics`

---

## Task 5: Non-Blocking Read, Status, Statistics, and Clear Rules

**Files:**
- Modify: `02_Service/service_uart/service_uart.h`
- Modify: `02_Service/service_uart/service_uart.c`
- Modify: `Tests/service_uart/test_service_uart.c`

**Interfaces:**
- Produces:

```c
platform_error_t service_uart_read(
    service_uart_t *service,
    uint8_t *buffer,
    platform_size_t bufferSize,
    platform_size_t *readLength);

platform_error_t service_uart_get_readable_size(
    const service_uart_t *service,
    platform_size_t *readableSize);

platform_error_t service_uart_get_status(
    const service_uart_t *service,
    service_uart_status_t *status);

platform_error_t service_uart_get_statistics(
    const service_uart_t *service,
    service_uart_statistics_t *statistics);

platform_error_t service_uart_clear_statistics(
    service_uart_t *service);
```

- [ ] Add failing read tests:

```text
RUNNING / STOPPED / ERROR -> existing buffered data readable
INITIALIZED -> INVALID_STATE
empty -> EMPTY + readLength 0
partial destination buffer -> read available prefix only
successful read -> rxBytesRead += actual readLength
zero-size read follows RingBuffer zero-length semantics
only Consumer Task may call read by contract; no runtime native-handle comparison added
```

- [ ] Add status/query tests:

```text
get_readable_size returns RingBuffer truth
get_status copies state/lastError/dataLossOccurred
get_statistics returns every frozen statistics field
queries on UNINITIALIZED -> NOT_INITIALIZED
```

- [ ] `service_uart_get_statistics()` must copy fields individually as a best-effort snapshot. Do not use Mutex/IRQ masking just for snapshot consistency.

- [ ] Add clear-statistics tests:

```text
INITIALIZED -> allowed
STOPPED -> allowed when Consumer quiescent
ERROR -> allowed when Consumer quiescent
RUNNING / STOPPING -> INVALID_STATE
all counters + highWaterMark -> 0
dataLossOccurred unchanged
lastError unchanged
RingBuffer contents/indexes unchanged
```

- [ ] Run GREEN + `git diff --check` + Coding Standard Review.

**Commit:** `feat(service-uart): add read status and statistics api`

---

## Task 6: wait_event Lost-Wakeup Defense and ERROR Handling

**Files:**
- Modify: `02_Service/service_uart/service_uart.c`
- Modify: `Tests/service_uart/test_service_uart.c`

**Interfaces:**
- Produces:

```c
#define SERVICE_UART_EVENT_RX_AVAILABLE    (1U << 0)
#define SERVICE_UART_EVENT_DATA_LOSS       (1U << 1)
#define SERVICE_UART_EVENT_ERROR           (1U << 2)
#define SERVICE_UART_EVENT_STOPPED         (1U << 3)

platform_error_t service_uart_wait_event(
    service_uart_t *service,
    uint32_t timeoutMs,
    uint32_t *events);
```

Platform Notify uses one private flag in `service_uart.c`, for example:

```c
#define SERVICE_UART_NOTIFY_WAKE_FLAG    (1U << 0)
```

Do not expose this Platform bit through Service Config or APP API.

- [ ] Extend the fake Notify backend so tests can control:

```text
clear result
wait result
received wake flag
hook invoked between clear/check/wait to simulate an arriving RX event
```

- [ ] Add failing immediate-state tests:

```text
RingBuffer readable > 0 -> RX_AVAILABLE without blocking
sticky dataLossOccurred -> DATA_LOSS
state ERROR -> ERROR
state STOPPED -> STOPPED
ERROR + unread bytes -> ERROR | RX_AVAILABLE
DATA_LOSS + unread bytes -> DATA_LOSS | RX_AVAILABLE
```

- [ ] Implement the frozen wait ordering exactly:

```text
validate service/state/events
*events = 0
platform_notify_clear(private WAKE)
rebuild Service events from RingBuffer + Context
if events != 0 -> return OK
platform_notify_wait(private WAKE, waitAny, clearOnExit, timeout)
if wait fails/timeout -> return that result
rebuild Service events
if events == 0 -> PLATFORM_ERR_EMPTY (invariant/spurious wake)
else -> OK
```

- [ ] Add race tests:

```text
RX already existed before clear -> still returned from RingBuffer truth
RX arrives after clear but before first state check -> returned immediately
RX arrives after first state check but before wait -> WAKE causes immediate return
stale WAKE with no Service truth -> no false RX_AVAILABLE; return EMPTY after wake
```

- [ ] Add wait state tests:

```text
RUNNING / STOPPED / ERROR -> allowed
INITIALIZED / STOPPING -> INVALID_STATE
NULL events -> NULL_POINTER or repository-consistent parameter error
```

- [ ] Add ERROR callback test:

```text
RUNNING + PLATFORM_UART_EVENT_ERROR
lastError = event.error
uartErrorCount++
state = ERROR
platform_notify_set_from_isr called
RingBuffer is not reset
no platform_uart_cancel()
no platform_uart_read_async() restart
```

- [ ] Verify ERROR with buffered data can be drained through `service_uart_read()` before APP calls `service_uart_start()`.

- [ ] Run GREEN + `git diff --check` + Coding Standard Review.

**Commit:** `feat(service-uart): add wait events and error handling`

---

## Task 7: Full Host Regression and Contract Review

**Files:**
- Review all files changed in Tasks 1-6.
- No unrelated production changes.

- [ ] Run UART Service Host Test:

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  -I02_Service/service_uart \
  -I02_Service/service_common \
  -I03_Platform/platform_mcu/uart \
  -I03_Platform/platform_os \
  -I03_Platform/platform_common \
  -I04_Impl/impl_board \
  Tests/service_uart/test_service_uart.c \
  02_Service/service_uart/service_uart.c \
  02_Service/service_common/ring_buffer.c \
  -o Tests/service_uart/test_service_uart

./Tests/service_uart/test_service_uart
```

- [ ] Re-run existing test suites using their established repository commands:

```text
Tests/ring_buffer
Tests/platform_uart
Tests/impl_platform_uart
Tests/platform_os
Tests/platform_log
```

Every suite that passed before UART Service work must remain PASS.

- [ ] Review dependency boundaries. `service_uart.h/.c` must contain none of:

```text
UART_HandleTypeDef
DMA_HandleTypeDef
USART1
HAL_UART_
HAL_DMA_
cmsis_os2.h
FreeRTOS.h
task.h
queue.h
semphr.h
EasyLogger
SEGGER RTT
```

- [ ] Review concurrency ownership:

```text
RX callback = sole RingBuffer Producer
Communication Task = sole RingBuffer Consumer
Service = sole owner of active platform_uart_cancel(RX)
callback never blocks/restarts/parses
```

- [ ] Review data-model semantics:

```text
Config = copied static wiring
Context = runtime state
Statistics = cumulative diagnostics
start resets Session state/RingBuffer but not Statistics
stop/error preserve buffered bytes
clear_statistics does not mutate runtime status
```

- [ ] Review callback binding lifecycle:

```text
bind before Platform UART STARTED
no callback replacement while STARTED
stop Service before Platform hardware stop
unbind only after Platform allows callback setter
```

- [ ] Run:

```bash
git diff --check
```

- [ ] Coding Standard Review must answer all five mandatory questions in `execution_rules.md`.

**Commit:** only if review fixes are needed: `refactor(service-uart): finalize phase1 contracts`

---

## Task 8: Keil Integration

**Files:**
- Modify only as needed: `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx`

- [ ] Add include path:

```text
../02_Service/service_uart
```

Keep existing `service_common`, Platform UART, Platform OS, and common include paths.

- [ ] Add a focused group such as:

```text
service/service_uart
```

- [ ] Add exactly the production Service source:

```text
../02_Service/service_uart/service_uart.c
```

Do not add Host Test files to the target.

- [ ] Run locally:

```text
Clean Targets
→ Rebuild all target files
```

Acceptance:

```text
0 Error(s)
```

If the execution environment cannot run Keil, record:

```text
CODE_COMPLETE_PENDING_KEIL_VERIFICATION
```

Do not invent a successful build result.

**Commit:** `build: integrate uart service into keil`

---

## Task 9: Real Board ISR -> Service -> Task Smoke Test

**Files:**
- Temporary test wiring only in existing APP / CubeMX USER CODE locations.
- Restore temporary board-test changes before phase completion.

**Interfaces under test:**

```text
USART1 RX
 -> DMA Circular
 -> Platform RX_DATA ISR callback
 -> UART Service ring_buffer_write
 -> Platform Notify from ISR
 -> Communication Task service_uart_wait_event
 -> service_uart_read
```

- [ ] Use static/preallocated test objects only. Representative storage:

```c
static uint8_t g_uartServiceDmaRxBuffer[256];
static uint8_t g_uartServiceRingStorage[1024];
static service_uart_t g_uartService = SERVICE_UART_INITIALIZER;
```

Keep exact final sizes consistent with available RAM and existing project conventions; do not introduce heap allocation.

- [ ] APP-side setup must follow the frozen order:

```text
construct Platform UART with no active Service callback
obtain dedicated Communication Task handle
service_uart_init()
Platform UART hardware init/start
service_uart_start()
```

- [ ] Communication Task behavior:

```text
wait_event
if RX_AVAILABLE -> read repeatedly until EMPTY
if DATA_LOSS -> record/report diagnostic state
if ERROR -> record status; do not auto-restart inside ISR
if STOPPED -> handle task/control state
```

Protocol parsing is not added for the smoke test.

- [ ] Verify at least:

```text
Short burst                          PASS
Multiple bursts                     PASS
Continuous stream > DMA buffer      PASS
Byte order / mismatch               PASS
RingBuffer wrap through Task drain  PASS
rxBytesReceived accounting          PASS
rxBytesRead accounting              PASS
normal load rxBytesDropped == 0     PASS
stop -> STOPPED                     PASS
restart -> RUNNING                  PASS
```

Use a continuous payload larger than the 256-byte DMA buffer, e.g. 640 bytes or more, so the real DMA wrap path is exercised again through the Service layer.

- [ ] If practical, create a controlled Consumer-delay or small-buffer test to prove `DATA_LOSS` / dropped statistics without corrupting memory. If this requires invasive target changes, Host Test coverage is sufficient; do not weaken production RingBuffer settings solely to force overflow.

- [ ] After runtime PASS, restore all temporary board-test code and rebuild again:

```text
Clean Targets
→ Rebuild all target files
→ 0 Error(s)
```

No temporary debug loop/task behavior may remain in final production flow unless separately approved.

---

## Task 10: Final Documentation and Handoff

**Files:**
- Modify: `00_Doc/04_Agent/handoff.md`
- Modify only if implementation revealed an approved durable architectural clarification: `00_Doc/04_Agent/architecture.md`

- [ ] Record actual changed production/test/build files.

- [ ] Record actual commit SHAs for major tasks.

- [ ] Record real verification results only:

```text
UART Service Host Test
Platform UART regression
Impl UART regression
RingBuffer regression
Platform OS regression
Platform Log regression
Coding Standard Review
Keil Full Rebuild
Board ISR -> Service -> Task Smoke Test
Post-test restored Keil Rebuild
```

- [ ] Record final frozen lifecycle:

```text
construct Platform UART
-> service_uart_init(bind)
-> Platform hardware start
-> service_uart_start
...
-> service_uart_stop
-> Platform hardware stop
-> service_uart_deinit(unbind)
-> Platform hardware deinit
```

- [ ] Record controlled Platform API amendment:

```text
platform_uart_set_callback() added for upper-layer event ownership binding;
no other Platform UART contract change.
```

- [ ] Record Coding Standard status exactly:

```text
Coding Standard Review: PASS
```

or, if a real exception exists, use `EXCEPTION` and document file/rule/reason/follow-up as required by `execution_rules.md`.

- [ ] Only mark:

```text
UART Service Phase 1 = COMPLETED
```

when Host/Regression/Coding Review/Keil/Board acceptance evidence is real. Otherwise use the exact pending state that reflects what is missing.

**Commit:** `docs: complete uart service phase1 handoff`

---

## Completion Gate

UART Service Phase 1 is complete only when all are true:

```text
platform_uart_set_callback() implemented + tested
service_uart.h/.c implemented
single UART / single Consumer ownership preserved
RX_DATA -> RingBuffer -> Task path verified
Partial Write / Full Drop statistics verified
DATA_LOSS sticky status verified
HighWaterMark verified
ERROR state + manual recovery policy verified
synchronous stop/CANCELED behavior verified
wait_event lost-wakeup race tests PASS
existing regressions PASS
Coding Standard Review PASS
Keil Full Rebuild 0 Error(s)
real board ISR -> Service -> Task smoke PASS
temporary board test restored
post-restore Keil Full Rebuild 0 Error(s)
handoff updated with real evidence
```

If implementation requires another Platform UART API change, RingBuffer API change, Platform Notify API change, multi-UART design, new synchronization primitive, or HAL/RTOS concrete type exposure in Service:

```text
STOP / BLOCKED
```

Return to architecture/design review instead of expanding scope.
