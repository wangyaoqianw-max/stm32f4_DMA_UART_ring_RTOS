# RTOS Platform Phase 1 Implementation Plan

> **For agentic workers:** execute this plan task-by-task. Do not redesign the frozen API during implementation; architecture changes require STOP/BLOCKED and a return to design.

**Goal:** Build a reusable Platform OS abstraction and CMSIS-RTOS2/FreeRTOS implementation for Thread, Mutex, Semaphore, Queue, Thread Notification, Software Timer, and Time/Delay before UART Service development begins.

**Architecture:** `APP / Service -> Platform OS -> Impl OS -> CMSIS-RTOS2 -> FreeRTOS`. Platform headers expose no CMSIS/FreeRTOS types. The selected backend implements Platform API symbols at link time; no runtime backend registry is introduced.

**Tech Stack:** C, STM32F411CEU6, CMSIS-RTOS2, FreeRTOS V10.3.1, Keil MDK-ARM, MinGW/GCC host tests.

**Spec:** `00_Doc/02_架构设计/RTOS_Platform_OS设计.md`

## Global Constraints

- Keep dependency direction: `APP -> Service -> Platform -> Impl -> Vendor / HAL / RTOS / Hardware`.
- Platform OS public headers must not include `cmsis_os2.h`, `FreeRTOS.h`, `task.h`, `queue.h`, `semphr.h`, or `timers.h`.
- Public timeout unit is milliseconds; `0U = NO_WAIT`, `0xFFFFFFFFU = WAIT_FOREVER`.
- First backend is CMSIS-RTOS2; native FreeRTOS APIs are allowed only inside `04_Impl/impl_os/freertos/` when CMSIS cannot express the required behavior cleanly.
- Only explicit `_from_isr` APIs may be used from ISR.
- Do not implement UART Service, RingBuffer, Communication Task, protocol parsing, or APP communication in this phase.
- Do not add a heap wrapper, StreamBuffer, MessageBuffer, EventGroup full mirror, generic critical-section wrapper, or scheduler-control API.
- Do not modify Vendor FreeRTOS/CMSIS source.
- Preserve unrelated user changes.

---

## 0. Preflight and Previous-Phase Closure Gate

**Files:**
- Read: `00_Doc/04_Agent/handoff.md`
- Read: `00_Doc/04_Agent/architecture.md`
- Read: `00_Doc/04_Agent/requirements.md`
- Read: `00_Doc/02_架构设计/RTOS_Platform_OS设计.md`
- Read: `Core/Inc/FreeRTOSConfig.h`

- [ ] Run:

```bash
git status --short
git log --oneline -n 10
```

- [ ] Confirm the repository still contains the Phase 2A implementation and that temporary UART board-test code has been restored.

- [ ] Check `handoff.md` for the final post-restore Keil result. If UART Phase 2A has not yet recorded the required final `0 Error(s)` rebuild, do not rewrite history. Record it as a remaining previous-phase verification item and continue RTOS work only if the user explicitly accepts the independent-subsystem transition.

- [ ] Do not use `reset`, `checkout .`, `clean`, or delete unrelated uncommitted work.

**Deliverable:** clean understanding of repository state; no source modification.

---

## 1. Freeze Platform OS Public Types and Headers

**Files:**
- Create: `03_Platform/platform_os/platform_os_types.h`
- Create: `03_Platform/platform_os/platform_thread.h`
- Create: `03_Platform/platform_os/platform_mutex.h`
- Create: `03_Platform/platform_os/platform_semaphore.h`
- Create: `03_Platform/platform_os/platform_queue.h`
- Create: `03_Platform/platform_os/platform_notify.h`
- Create: `03_Platform/platform_os/platform_timer.h`
- Create: `03_Platform/platform_os/platform_time.h`
- Create: `03_Platform/platform_os/platform_os.h`
- Create: `Tests/platform_os/test_platform_os_headers.c`

**Interfaces produced:**

```c
typedef struct { void *native; } platform_thread_t;
typedef struct { void *native; } platform_mutex_t;
typedef struct { void *native; } platform_semaphore_t;
typedef struct { void *native; } platform_queue_t;
typedef struct { void *native; } platform_timer_t;

#define PLATFORM_OS_OBJECT_INITIALIZER { NULL }
#define PLATFORM_OS_NO_WAIT            (0U)
#define PLATFORM_OS_WAIT_FOREVER       (0xFFFFFFFFU)
#define PLATFORM_NOTIFY_VALID_MASK     (0x7FFFFFFFU)
```

- [ ] Write `test_platform_os_headers.c` that includes every Platform OS public header without any CMSIS/FreeRTOS include path and instantiates all opaque objects/config enums.

- [ ] Run a compile-only host test. From `RTT_elog_DMA_UART_ring_project/`:

```bash
gcc -std=c11 -Wall -Wextra -Werror -c \
  -I03_Platform/platform_common \
  -I03_Platform/platform_os \
  -I04_Impl/impl_board \
  Tests/platform_os/test_platform_os_headers.c \
  -o Tests/platform_os/test_platform_os_headers.o
```

Expected before headers exist: FAIL. Expected after implementation: PASS.

- [ ] Implement only declarations/types described by the frozen spec. `platform_os.h` aggregates the other public headers and contains no backend definitions.

- [ ] Repeat compile-only test and run `git diff --check`.

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/03_Platform/platform_os RTT_elog_DMA_UART_ring_project/Tests/platform_os/test_platform_os_headers.c
git commit -m "feat(os): define platform os public interfaces"
```

---

## 2. Build Fake CMSIS Test Harness and Common Timeout Mapping

**Files:**
- Create: `Tests/platform_os/cmsis_os2.h`
- Create: `Tests/platform_os/test_platform_os.c`
- Create: `04_Impl/impl_os/freertos/impl_freertos_common.h`
- Create: `04_Impl/impl_os/freertos/impl_freertos_time.c`

**Private helpers produced:**

```c
uint32_t impl_freertos_timeout_to_ticks(uint32_t timeoutMs);
platform_error_t impl_freertos_map_status(osStatus_t status);
```

`impl_freertos_timeout_to_ticks()` rules:

```text
WAIT_FOREVER -> osWaitForever
0            -> 0
non-zero ms  -> ceil(ms * tickFreq / 1000)
minimum      -> 1 tick
calculation  -> 64-bit intermediate
```

- [ ] Fake CMSIS header must define only the CMSIS types/constants/functions consumed by the Impl tests and provide controllable globals for return values/call recording.

- [ ] Add failing tests for:
  - `1 ms` at `1000 Hz -> 1 tick`.
  - `1 ms` at `250 Hz -> 1 tick`.
  - `5 ms` at `250 Hz -> 2 ticks`.
  - `1000 ms` at `250 Hz -> 250 ticks`.
  - `WAIT_FOREVER -> osWaitForever`.
  - `platform_time_delay_ms()` calls `osDelay()` with converted ticks.
  - `platform_time_get_ms()` converts tick count using `osKernelGetTickFreq()`.
  - CMSIS `osErrorISR` maps to `PLATFORM_ERR_INVALID_STATE`.

- [ ] Implement minimum common helpers and Time/Delay adapter until tests pass.

- [ ] Verify with:

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  -ITests/platform_os \
  -I03_Platform/platform_common \
  -I03_Platform/platform_os \
  -I04_Impl/impl_board \
  -I04_Impl/impl_os/freertos \
  Tests/platform_os/test_platform_os.c \
  04_Impl/impl_os/freertos/impl_freertos_time.c \
  -o Tests/platform_os/test_platform_os
./Tests/platform_os/test_platform_os
```

**Commit:** `feat(os): add freertos time adapter and test harness`

---

## 3. Implement Thread Adapter

**Files:**
- Modify: `Tests/platform_os/test_platform_os.c`
- Modify: `Tests/platform_os/cmsis_os2.h`
- Create: `04_Impl/impl_os/freertos/impl_freertos_thread.c`

**Public API:**

```c
platform_thread_create()
platform_thread_get_current()
platform_thread_set_priority()
platform_thread_get_priority()
platform_thread_suspend()
platform_thread_resume()
platform_thread_terminate()
platform_thread_yield()
```

- [ ] Add failing tests for null parameters, duplicate create, stack size `0`, invalid priority, correct name/entry/argument/stack-size mapping, priority mapping, current-thread lookup, suspend/resume, terminate clearing `native`, and CMSIS error propagation.

- [ ] Use CMSIS `osThreadNew`, `osThreadGetId`, priority, suspend/resume/terminate/yield APIs. Do not include native FreeRTOS task headers.

- [ ] Treat `osThreadNew() == NULL` as `PLATFORM_ERR_NO_MEMORY` and leave `native == NULL`.

- [ ] Rebuild host suite with `impl_freertos_thread.c`; require `-Werror` PASS.

**Commit:** `feat(os): add platform thread freertos adapter`

---

## 4. Implement Mutex and Semaphore Adapters

**Files:**
- Modify: `Tests/platform_os/test_platform_os.c`
- Modify: `Tests/platform_os/cmsis_os2.h`
- Create: `04_Impl/impl_os/freertos/impl_freertos_mutex.c`
- Create: `04_Impl/impl_os/freertos/impl_freertos_semaphore.c`

### Mutex tests

- [ ] Normal create maps to CMSIS mutex attributes.
- [ ] Recursive create enables recursive attribute.
- [ ] Duplicate create rejected.
- [ ] `lock(NO_WAIT)` + `osErrorResource -> PLATFORM_ERR_BUSY`.
- [ ] finite timeout + `osErrorTimeout -> PLATFORM_ERR_TIMEOUT`.
- [ ] unlock and delete work; successful delete clears `native`.
- [ ] `osErrorISR -> PLATFORM_ERR_INVALID_STATE`.

### Semaphore tests

- [ ] Reject `maxCount == 0` and `initialCount > maxCount`.
- [ ] create stores valid handle.
- [ ] `take(NO_WAIT)` unavailable -> `PLATFORM_ERR_EMPTY`.
- [ ] finite timeout -> `PLATFORM_ERR_TIMEOUT`.
- [ ] give when full -> `PLATFORM_ERR_FULL`.
- [ ] `give_from_isr()` uses an ISR-permitted CMSIS path and never blocks.
- [ ] delete clears `native`.

- [ ] Implement with CMSIS mutex/semaphore APIs only unless a concrete CMSIS limitation is demonstrated.

- [ ] Run full host suite with all adapter files compiled.

**Commit:** `feat(os): add mutex and semaphore adapters`

---

## 5. Implement Queue Adapter

**Files:**
- Modify: `Tests/platform_os/test_platform_os.c`
- Modify: `Tests/platform_os/cmsis_os2.h`
- Create: `04_Impl/impl_os/freertos/impl_freertos_queue.c`

**Public API:**

```c
platform_queue_create()
platform_queue_send()
platform_queue_send_from_isr()
platform_queue_receive()
platform_queue_get_count()
platform_queue_get_space()
platform_queue_delete()
```

- [ ] Add failing tests for zero item count/size, correct depth/item-size mapping, duplicate create, null item pointers, no-wait full -> `FULL`, finite send timeout -> `TIMEOUT`, no-wait receive empty -> `EMPTY`, finite receive timeout -> `TIMEOUT`, ISR send using timeout zero, count/space conversion, and delete clearing handle.

- [ ] Implement using CMSIS Message Queue APIs. `_from_isr()` must pass zero timeout and must not call a blocking path.

- [ ] Do not add peek, overwrite, pointer ownership, or receive-from-ISR in V1.

- [ ] Run full host suite.

**Commit:** `feat(os): add platform queue adapter`

---

## 6. Implement Thread Notification Adapter

**Files:**
- Modify: `Tests/platform_os/test_platform_os.c`
- Modify: `Tests/platform_os/cmsis_os2.h`
- Create: `04_Impl/impl_os/freertos/impl_freertos_notify.c`

**Public API:**

```c
platform_notify_set()
platform_notify_set_from_isr()
platform_notify_wait()
platform_notify_clear()
```

- [ ] Reject `flags == 0`, any flag outside `0x7FFFFFFF`, null target thread, and null output pointer where required.

- [ ] Verify task-context set calls CMSIS thread flags and preserves flag bits.

- [ ] Verify ISR set uses the CMSIS path that is ISR-safe in the current wrapper.

- [ ] Test wait-any / wait-all, clear-on-exit / no-clear, no-wait empty -> `PLATFORM_ERR_EMPTY`, finite timeout -> `PLATFORM_ERR_TIMEOUT`, and successful returned flags.

- [ ] Test clear returns previous flags and rejects ISR context through CMSIS error mapping.

- [ ] Do not create a separate notification object; wait/clear operate on the current thread, matching CMSIS Thread Flags semantics.

- [ ] Run full host suite.

**Commit:** `feat(os): add thread notification adapter`

---

## 7. Implement Software Timer Adapter

**Files:**
- Modify: `Tests/platform_os/test_platform_os.c`
- Modify: `Tests/platform_os/cmsis_os2.h`
- Create: `04_Impl/impl_os/freertos/impl_freertos_timer.c`

**Public API:**

```c
platform_timer_create()
platform_timer_start()
platform_timer_stop()
platform_timer_is_running()
platform_timer_delete()
```

- [ ] Add failing tests for null callback/name policy, once/periodic mapping, callback/argument forwarding, zero period rejection, ms-to-tick conversion, running-state query, delete clearing handle, and task-only ISR rejection where CMSIS reports `osErrorISR`.

- [ ] Timer callback is RTOS Timer Task context, not ISR context; document this in the public header.

- [ ] Run full host suite.

**Commit:** `feat(os): add software timer adapter`

---

## 8. Platform OS Regression and API Review

**Files:**
- Review all: `03_Platform/platform_os/*`
- Review all: `04_Impl/impl_os/freertos/*`
- Review: `Tests/platform_os/*`

- [ ] Compile public-header isolation test again with no CMSIS/FreeRTOS include path.

- [ ] Compile full Fake CMSIS suite with:

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  -ITests/platform_os \
  -I03_Platform/platform_common \
  -I03_Platform/platform_os \
  -I04_Impl/impl_board \
  -I04_Impl/impl_os/freertos \
  Tests/platform_os/test_platform_os.c \
  04_Impl/impl_os/freertos/impl_freertos_time.c \
  04_Impl/impl_os/freertos/impl_freertos_thread.c \
  04_Impl/impl_os/freertos/impl_freertos_mutex.c \
  04_Impl/impl_os/freertos/impl_freertos_semaphore.c \
  04_Impl/impl_os/freertos/impl_freertos_queue.c \
  04_Impl/impl_os/freertos/impl_freertos_notify.c \
  04_Impl/impl_os/freertos/impl_freertos_timer.c \
  -o Tests/platform_os/test_platform_os
./Tests/platform_os/test_platform_os
```

- [ ] Run existing regressions:

```text
Tests/platform_uart
Tests/impl_platform_uart
Tests/platform_log
```

- [ ] Check that no Platform OS public header contains CMSIS/FreeRTOS identifiers.

- [ ] Run `git diff --check`.

**Commit:** only if review fixes are needed: `refactor(os): finalize platform os contracts`

---

## 9. Keil Integration

**Files:**
- Modify: `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx` only as required to add the new Impl `.c` files and Platform include path.
- Do not modify CubeMX generated RTOS logic to make the adapter compile.

- [ ] Add all `04_Impl/impl_os/freertos/impl_freertos_*.c` sources to an appropriate Keil group, preferably `04_Impl/impl_os` or equivalent existing project grouping.

- [ ] Ensure include paths contain `03_Platform/platform_os` and `04_Impl/impl_os/freertos` only where needed.

- [ ] Execute local:

```text
Clean Targets
→ Rebuild all target files
```

Expected: `0 Error(s)`.

- [ ] Existing unrelated warnings may remain; do not refactor UART/Log/CubeMX code to remove them in this phase.

- [ ] If random `.o` I/O errors (`C4051E`, `L6449E`, `Invalid argument`) recur, classify as environment/output issue first; do not change OS source logic to work around filesystem failures.

---

## 10. Temporary Board Smoke Test

**Primary file:**
- Modify temporarily: `Core/Src/freertos.c` USER CODE sections only.
- Optional temporary ISR hook only if needed for ISR notification verification; any generated-file modification must stay inside USER CODE sections and be fully restored afterward.

**Board-test sequence:**

```text
1. platform_time_delay_ms / platform_time_get_ms
2. platform_thread_get_current
3. create worker thread
4. mutex shared-counter protection
5. semaphore task synchronization
6. queue task-to-task round trip
7. notification task-to-task
8. software timer callback
9. notification ISR-to-task using existing USART1 RX callback path
```

- [ ] Log results through Platform Log / RTT from Task Context.

- [ ] For ISR notification test, reuse the already verified Platform UART Phase 2A path without introducing UART Service or RingBuffer:

```text
PC sends short USART1 data
    ↓
Platform UART RX_DATA callback (ISR)
    ↓
platform_notify_set_from_isr(targetThread, TEST_RX_FLAG)
    ↓
Task platform_notify_wait(...)
    ↓
RTT prints PASS
```

- [ ] ISR callback must not log, block, allocate, take a mutex, or parse data.

- [ ] Required final RTT summary:

```text
RTOS PLATFORM BOARD TEST
TIME                PASS
THREAD              PASS
MUTEX               PASS
SEMAPHORE           PASS
QUEUE               PASS
NOTIFY TASK         PASS
TIMER               PASS
NOTIFY ISR          PASS
RTOS PLATFORM BOARD TEST: PASS
```

- [ ] Do not claim PASS without real board evidence.

---

## 11. Restore Temporary Test and Final Verification

**Files:**
- Restore temporary board-test changes from `Core/Src/freertos.c` and any temporary ISR hook.
- Modify: `00_Doc/04_Agent/handoff.md`

- [ ] Restore normal runtime code; do not leave test tasks, buffers, or test flags in production source.

- [ ] Run final local Keil:

```text
Clean Targets
→ Rebuild all target files
```

Expected: `0 Error(s)`.

- [ ] Update `handoff.md` with actual:
  - files changed;
  - Host test results;
  - Keil result;
  - Thread/Mutex/Semaphore/Queue/Notify/Timer/Time board results;
  - ISR notification evidence;
  - temporary test restore result;
  - final rebuild result;
  - deviations and remaining warnings.

### Completion status

Only after all verification:

```text
RTOS Platform Phase 1 = COMPLETED
```

Code + Host test complete but no board verification:

```text
CODE_COMPLETE_PENDING_HARDWARE_VERIFICATION
```

Frozen API cannot be correctly implemented without architecture change:

```text
BLOCKED
```

After `COMPLETED`, return to Sol design for:

```text
UART Service
+ SPSC RingBuffer
+ Platform UART RX_DATA
+ Platform Notify ISR→Task
```
