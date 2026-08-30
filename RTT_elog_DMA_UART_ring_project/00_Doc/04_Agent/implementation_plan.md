# RingBuffer Phase 1 Implementation Plan

> **For agentic workers:** execute this plan task-by-task. Use TDD: RED → GREEN → Refactor → Verify → Commit. Do not redesign the frozen API during implementation; architecture changes require STOP / BLOCKED and a return to design.

**Goal:** Implement a pure-C SPSC byte-stream RingBuffer for later UART Service use, with caller-owned storage, reserved-slot capacity, partial-write overflow reporting, deterministic Host Test, and Keil integration.

**Architecture:** RingBuffer lives in `02_Service/service_common/`. It depends only on Platform common definitions/types/errors and C memory-copy support. It knows nothing about UART, DMA, RTOS, Notification, logging, protocol parsing, statistics, or task creation. Concurrency is frozen as Single Producer / Single Consumer: Producer is the sole writer of `writeIndex`; Consumer is the sole writer of `readIndex`.

**Tech Stack:** C, STM32F411CEU6, ARMCC5 / Keil MDK-ARM, GCC Host Test.

**Spec:** `00_Doc/02_架构设计/RingBuffer_SPSC设计.md`

## Mandatory References

Before modifying any project C/H file, read:

```text
00_Doc/04_Agent/execution_rules.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/02_架构设计/RingBuffer_SPSC设计.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/handoff.md
03_Platform/platform_common/platform_def.h
03_Platform/platform_common/platform_types.h
03_Platform/platform_common/platform_error.h
```

Preflight report must explicitly contain:

```text
Coding Standard:
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
Status: READ

Agent Execution Rules:
00_Doc/04_Agent/execution_rules.md
Status: READ
```

`platform_def.h` already defines project common macros including `PLATFORM_TRUE`, `PLATFORM_FALSE`, `NULL`, and `ARRAY_SIZE`. RingBuffer may use those definitions when useful. Do not redefine them locally.

## Global Constraints

- Only implement RingBuffer in this phase.
- Production files are limited to `02_Service/service_common/ring_buffer.h` and `ring_buffer.c`.
- Tests live under `Tests/ring_buffer/`.
- Do not create UART Service, Communication Task, Notification integration, service statistics, protocol parsing, or service_log behavior.
- Do not modify Platform UART, UART DMA Impl, Platform OS, Vendor, HAL, CMSIS, FreeRTOS Kernel, or CubeMX-generated logic.
- No dynamic allocation.
- No Mutex, Semaphore, Critical Section, RTOS API, IRQ masking, or logging inside RingBuffer.
- Caller owns backing storage.
- `storageSize = N`; usable capacity is exactly `N - 1`.
- No power-of-two requirement.
- SPSC only.
- Producer is the only writer of `writeIndex`; Consumer is the only writer of `readIndex`.
- Publish an index only after the corresponding data copy completes.
- Write uses Partial Write: preserve old data, write the largest prefix that fits, return `PLATFORM_ERR_OVERFLOW` if the request is not fully stored.
- Read is non-blocking and reads up to the requested size.
- `reset()` is quiescent-only and performs no storage clearing.
- Statistics are not part of RingBuffer V1.
- Follow repository C coding standard and `execution_rules.md` as mandatory gates.

---

## Task 0: Preflight and Scope Confirmation

**Files:**
- Read all mandatory references above.
- Inspect `02_Service/`, `Tests/`, and current Keil groups/include paths.

- [ ] Run:

```bash
git status --short
git log --oneline -n 12
```

- [ ] Confirm:

```text
RTOS Platform Phase 1 = COMPLETED
No existing production RingBuffer implementation
No existing service_uart implementation
```

- [ ] Report frozen scope before coding:

```text
Phase: RingBuffer Phase 1
Scope: Pure SPSC RingBuffer only
UART Service: NOT IN SCOPE
Platform Notify integration: NOT IN SCOPE
Statistics: NOT IN SCOPE
Board test: NOT REQUIRED
```

- [ ] Preserve unrelated changes. Do not use `git reset`, `git reset --hard`, `git checkout .`, or `git clean`.

**Deliverable:** verified context; no source modification.

---

## Task 1: Public Contract, Init, Reset, and Size Queries

**Files:**
- Create: `02_Service/service_common/ring_buffer.h`
- Create: `02_Service/service_common/ring_buffer.c`
- Create: `Tests/ring_buffer/test_ring_buffer.c`

**Public object:**

```c
typedef struct
{
    uint8_t *storage;
    platform_size_t storageSize;
    volatile platform_size_t readIndex;
    volatile platform_size_t writeIndex;
} ring_buffer_t;
```

**Frozen public API:**

```c
platform_error_t ring_buffer_init(
    ring_buffer_t *ringBuffer,
    uint8_t *storage,
    platform_size_t storageSize);

platform_error_t ring_buffer_reset(
    ring_buffer_t *ringBuffer);

platform_error_t ring_buffer_write(
    ring_buffer_t *ringBuffer,
    const uint8_t *data,
    platform_size_t dataLength,
    platform_size_t *writtenLength);

platform_error_t ring_buffer_read(
    ring_buffer_t *ringBuffer,
    uint8_t *buffer,
    platform_size_t bufferSize,
    platform_size_t *readLength);

platform_error_t ring_buffer_get_readable_size(
    const ring_buffer_t *ringBuffer,
    platform_size_t *readableSize);

platform_error_t ring_buffer_get_free_size(
    const ring_buffer_t *ringBuffer,
    platform_size_t *freeSize);
```

- [ ] Write failing tests for:

```text
init(NULL, ...)                       -> NULL_POINTER
init(..., NULL, ...)                  -> NULL_POINTER
storageSize < 2                       -> INVALID_PARAM
valid init                            -> indexes 0/0
initial readable                      -> 0
initial free for storage[8]           -> 7
other API with ringBuffer == NULL     -> NULL_POINTER
zeroed/uninitialized object           -> NOT_INITIALIZED
query output == NULL                  -> NULL_POINTER
reset                                 -> indexes 0/0 only; storage bytes unchanged
```

- [ ] Run RED. From `RTT_elog_DMA_UART_ring_project/`:

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  -I02_Service/service_common \
  -I03_Platform/platform_common \
  -I04_Impl/impl_board \
  Tests/ring_buffer/test_ring_buffer.c \
  02_Service/service_common/ring_buffer.c \
  -o Tests/ring_buffer/test_ring_buffer
```

Expected before implementation: FAIL.

- [ ] Implement the public header using repository file header, Header Guard, Chinese Doxygen in `.h`, and only required Platform common includes. Document SPSC ownership, caller-owned storage, `N-1` usable capacity, and quiescent-only reset.

- [ ] Implement a private validation helper that preserves error semantics. Recommended form:

```c
static platform_error_t ring_buffer_validate(const ring_buffer_t *ringBuffer)
{
    if (ringBuffer == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if ((ringBuffer->storage == NULL) || (ringBuffer->storageSize < 2U)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return PLATFORM_ERR_OK;
}
```

This helper uses `platform_error_t` to preserve the distinction between null object and invalid/uninitialized object. This is an interface choice, not a workaround for boolean macros.

- [ ] Implement readable/free formulas:

```c
if (writeIndex >= readIndex) {
    readable = writeIndex - readIndex;
} else {
    readable = ringBuffer->storageSize - readIndex + writeIndex;
}

capacity = ringBuffer->storageSize - 1U;
freeSize = capacity - readable;
```

- [ ] Run GREEN and `git diff --check`.

- [ ] Coding Standard Review before commit.

**Commit:**

```bash
git add RTT_elog_DMA_UART_ring_project/02_Service/service_common \
        RTT_elog_DMA_UART_ring_project/Tests/ring_buffer/test_ring_buffer.c
git commit -m "feat(ring): add spsc ring buffer core"
```

---

## Task 2: Non-Wrapping Write and Read

**Files:**
- Modify: `02_Service/service_common/ring_buffer.c`
- Modify: `Tests/ring_buffer/test_ring_buffer.c`

- [ ] Add failing tests for:

```text
simple write/read preserves order
partial read when output buffer is smaller
empty read -> EMPTY + readLength 0
zero-length write permits data == NULL and returns OK + writtenLength 0
zero-length read permits buffer == NULL and returns OK + readLength 0
nonzero write + data == NULL -> NULL_POINTER
nonzero read + buffer == NULL -> NULL_POINTER
writtenLength == NULL -> NULL_POINTER
readLength == NULL -> NULL_POINTER
```

- [ ] Run RED.

- [ ] Implement write ordering exactly as:

```text
validate object
validate writtenLength
*writtenLength = 0
if dataLength == 0 -> OK
validate data
snapshot readIndex once
use Producer-owned writeIndex
calculate free from that snapshot
writeLength = min(dataLength, free)
copy bytes
publish writeIndex last
*writtenLength = writeLength
if writeLength < dataLength -> OVERFLOW
else -> OK
```

- [ ] Implement read ordering exactly as:

```text
validate object
validate readLength
*readLength = 0
if bufferSize == 0 -> OK
validate buffer
snapshot writeIndex once
use Consumer-owned readIndex
calculate readable from that snapshot
if readable == 0 -> EMPTY
readLengthActual = min(bufferSize, readable)
copy bytes
publish readIndex last
*readLength = readLengthActual
return OK
```

- [ ] Run GREEN with the Task 1 GCC command and `git diff --check`.

- [ ] Coding Standard Review before commit.

**Commit:** `feat(ring): add ring buffer read write`

---

## Task 3: Wrap, Full, and Partial-Write Overflow

**Files:**
- Modify: `02_Service/service_common/ring_buffer.c`
- Modify: `Tests/ring_buffer/test_ring_buffer.c`

- [ ] Add failing reserved-slot tests using `storage[8]`:

```text
usable capacity = 7
write 7 -> OK, written=7, free=0
write another 1 -> OVERFLOW, written=0
read back -> original 7 bytes unchanged
```

- [ ] Add failing partial-overflow test:

```text
capacity = 7
write old 5 bytes
request new 4 bytes
expected writtenLength = 2
expected return = OVERFLOW
expected stream = old 5 + first 2 new bytes
```

- [ ] Add wrap test using only public APIs:

```text
write A B C D E
read A B C D
write F G H I J
read all
expected E F G H I J
```

Tests must not manufacture state by assigning `readIndex` or `writeIndex` directly.

- [ ] Run RED.

- [ ] Implement two-segment copy. Write example:

```c
firstLength = writeLength;
if (firstLength > (ringBuffer->storageSize - writeIndex)) {
    firstLength = ringBuffer->storageSize - writeIndex;
}

secondLength = writeLength - firstLength;
```

Copy `[writeIndex, storageSize)` first and `[0, secondLength)` second. Publish the final `writeIndex` only after both copies. Read follows the same two-segment model and publishes `readIndex` last.

- [ ] Do not implement the main path as a byte-by-byte modulo loop.

- [ ] Verify full/partial overflow never overwrites unread bytes.

- [ ] Run GREEN + `git diff --check` + Coding Standard Review.

**Commit:** `feat(ring): add wrap and overflow semantics`

---

## Task 4: Deterministic Reference-Model Stress Test

**Files:**
- Modify: `Tests/ring_buffer/test_ring_buffer.c`

- [ ] Add a deterministic pseudo-random generator with a fixed seed. Do not depend on wall-clock time.

- [ ] Maintain a simple linear reference queue in the test only.

- [ ] Execute at least `100000` deterministic operations mixing:

```text
write request lengths: 0 .. 31
read request lengths: 0 .. 31
wrap transitions
empty states
full states
partial-overflow states
```

- [ ] For every write, verify:

```text
expectedWritten = min(requestLength, referenceFree)
actual writtenLength == expectedWritten
return OK iff expectedWritten == requestLength
return OVERFLOW iff expectedWritten < requestLength
reference queue receives exactly the accepted prefix
```

- [ ] For every read, verify:

```text
expectedRead = min(requestLength, referenceReadable)
when requestLength > 0 and referenceReadable == 0 -> EMPTY
otherwise -> OK
readLength == expectedRead
byte sequence matches reference exactly
```

- [ ] After each operation verify `readable + free == capacity`.

- [ ] Drain at the end and verify exact remaining sequence.

- [ ] Run with:

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  -I02_Service/service_common \
  -I03_Platform/platform_common \
  -I04_Impl/impl_board \
  Tests/ring_buffer/test_ring_buffer.c \
  02_Service/service_common/ring_buffer.c \
  -o Tests/ring_buffer/test_ring_buffer

./Tests/ring_buffer/test_ring_buffer
```

Expected: all deterministic tests PASS.

- [ ] Run `git diff --check` and Coding Standard Review.

**Commit:** `test(ring): add deterministic reference stress test`

---

## Task 5: RingBuffer API and Concurrency Review

**Files:**
- Review: `02_Service/service_common/ring_buffer.h`
- Review: `02_Service/service_common/ring_buffer.c`
- Review: `Tests/ring_buffer/test_ring_buffer.c`

- [ ] Verify dependencies contain none of:

```text
UART
DMA
cmsis_os2.h
FreeRTOS.h
task.h
queue.h
semphr.h
platform_notify
platform_log
EasyLogger
SEGGER RTT
```

- [ ] Verify RingBuffer contains no statistics fields, no dynamic allocation, no locks, and no task/ISR registration logic.

- [ ] Verify concurrency contract in code and comments:

```text
Producer writes only writeIndex
Consumer writes only readIndex
Producer snapshots readIndex
Consumer snapshots writeIndex
Index publication occurs after memcpy
reset is quiescent-only
```

- [ ] Verify public API comments state ownership, zero-length semantics, output-length semantics, overflow semantics, and reset restriction.

- [ ] Verify no `PLATFORM_TRUE/FALSE`, `NULL`, or other common macros are redefined locally; use `platform_def.h` only when those common definitions are needed.

- [ ] Run Host Test and `git diff --check` again.

**Commit:** only if review fixes are needed: `refactor(ring): finalize spsc contract`

---

## Task 6: Existing Regression

RingBuffer does not modify existing subsystems, but regression must verify no dependency/build breakage.

- [ ] Run existing Host Test suites according to their repository commands:

```text
Tests/platform_uart
Tests/impl_platform_uart
Tests/platform_os
Tests/platform_log
```

- [ ] Require all previously passing suites to remain PASS.

- [ ] Do not change unrelated subsystem code to make a regression pass. If a pre-existing failure is discovered, record it separately instead of hiding it inside RingBuffer work.

---

## Task 7: Keil Integration

**Files:**
- Modify only as needed: `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx`

- [ ] Add include path:

```text
../02_Service/service_common
```

- [ ] Add a suitable Keil group, preferably:

```text
service/service_common
```

- [ ] Add exactly:

```text
../02_Service/service_common/ring_buffer.c
```

No Test source belongs in the target.

- [ ] Do not add UART Service or change existing UART/RTOS groups.

- [ ] Run locally:

```text
Clean Targets
→ Rebuild all target files
```

Acceptance:

```text
0 Error(s)
```

If Codex cannot execute Keil, do not invent a result. Record:

```text
CODE_COMPLETE_PENDING_KEIL_VERIFICATION
```

and stop for user verification.

No board smoke test is required for this pure-software RingBuffer phase.

---

## Task 8: Final Handoff and Completion Gate

**Files:**
- Modify: `00_Doc/04_Agent/handoff.md`

- [ ] Record:

```text
RingBuffer Phase 1 status
created/modified files
frozen API
SPSC ownership contract
storage ownership
N-1 capacity rule
partial-write overflow behavior
Host Test result
deterministic stress result
existing regression result
Coding Standard Review result
Keil integration result
Keil Full Rebuild result
deviations/blockers
```

- [ ] Completion status rules:

If code/tests/regression pass but no real Keil build:

```text
CODE_COMPLETE_PENDING_KEIL_VERIFICATION
```

Only after all of the following have real evidence:

```text
ring_buffer.h/.c implemented
RingBuffer Host Test PASS
deterministic reference stress PASS
existing UART/Platform OS/Log regressions PASS
Coding Standard Review = PASS
Keil project integration complete
current Full Rebuild = 0 Error(s)
handoff updated with actual results
```

may the handoff state:

```text
RingBuffer Phase 1 = COMPLETED
```

---

## Explicit Scope Guard

Do not implement in this phase:

```text
UART Service
Platform UART RX_DATA callback integration
Platform Notify integration
Communication Task
Service statistics / highWaterMark
Protocol Parser
service_log
DMA Buffer ownership integration
UART error recovery policy
peek/discard/read_until/find/read_line
zero-copy span API
multi-producer or multi-consumer support
```

If implementation proves the frozen API, `N-1` capacity model, Partial Write policy, or SPSC concurrency contract must change:

```text
STOP / BLOCKED
```

Record the exact conflict and return to design review before modifying the frozen contract.
