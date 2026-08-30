# RingBuffer Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a pure-C SPSC byte-stream RingBuffer for later UART Service use, with caller-owned storage, reserved-slot capacity, partial-write overflow reporting, deterministic Host Test, and Keil integration.

**Architecture:** RingBuffer lives in `02_Service/service_common/`. It depends only on Platform common types/errors and C memory-copy support. It knows nothing about UART, DMA, RTOS, Notification, logging, protocol parsing, statistics, or task creation. Concurrency is frozen as Single Producer / Single Consumer: Producer is the sole writer of `writeIndex`; Consumer is the sole writer of `readIndex`.

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

## Global Constraints

- Only implement RingBuffer in this phase.
- Create production files only under `02_Service/service_common/`.
- Do not create UART Service, Communication Task, Notification integration, service statistics, protocol parsing, or service_log behavior.
- Do not modify Platform UART, UART DMA Impl, Platform OS, Vendor, HAL, CMSIS, FreeRTOS Kernel, or CubeMX-generated logic.
- No dynamic allocation.
- No Mutex, Semaphore, Critical Section, RTOS API, IRQ masking, or logging inside RingBuffer.
- Caller owns backing storage.
- `storageSize = N`; usable capacity is exactly `N - 1`.
- No power-of-two requirement.
- SPSC only.
- Write uses Partial Write: preserve old data, write the largest prefix that fits, return `PLATFORM_ERR_OVERFLOW` if the request is not fully stored.
- Read is non-blocking and reads up to the requested size.
- `reset()` is quiescent-only and performs no storage clearing.
- Statistics are not part of RingBuffer V1.
- Follow repository C coding standard and `execution_rules.md` as mandatory gates.

---

## Task 0: Preflight and Scope Confirmation

**Files:**
- Read: mandatory references above.
- Inspect: `02_Service/`
- Inspect: `Tests/`

- [ ] **Step 1: Inspect repository state**

Run:

```bash
git status --short
git log --oneline -n 12
```

Confirm:

```text
RTOS Platform Phase 1 = COMPLETED
No existing RingBuffer production implementation
No existing service_uart implementation
```

- [ ] **Step 2: Report the frozen scope before coding**

```text
Phase: RingBuffer Phase 1
Scope: Pure SPSC RingBuffer only
UART Service: NOT IN SCOPE
Platform Notify integration: NOT IN SCOPE
Statistics: NOT IN SCOPE
Board test: NOT REQUIRED
```

- [ ] **Step 3: Protect unrelated work**

Do not use:

```text
git reset
git reset --hard
git checkout .
git clean
```

If unrelated uncommitted changes exist, preserve them.

**Deliverable:** verified context; no source modification.

---

## Task 1: Public Contract, Initialization, Reset, and Size Queries

**Files:**
- Create: `02_Service/service_common/ring_buffer.h`
- Create: `02_Service/service_common/ring_buffer.c`
- Create: `Tests/ring_buffer/test_ring_buffer.c`

**Produces:**

```c
typedef struct
{
    uint8_t *storage;
    platform_size_t storageSize;
    volatile platform_size_t readIndex;
    volatile platform_size_t writeIndex;
} ring_buffer_t;

platform_error_t ring_buffer_init(
    ring_buffer_t *ringBuffer,
    uint8_t *storage,
    platform_size_t storageSize);

platform_error_t ring_buffer_reset(
    ring_buffer_t *ringBuffer);

platform_error_t ring_buffer_get_readable_size(
    const ring_buffer_t *ringBuffer,
    platform_size_t *readableSize);

platform_error_t ring_buffer_get_free_size(
    const ring_buffer_t *ringBuffer,
    platform_size_t *freeSize);
```

The header must also declare the frozen Write/Read APIs so the entire V1 public contract is visible from Task 1.

- [ ] **Step 1: Write failing tests**

Create these tests:

```c
static void test_init_rejects_null_ring_buffer(void);
static void test_init_rejects_null_storage(void);
static void test_init_rejects_storage_smaller_than_two(void);
static void test_init_sets_empty_state(void);
static void test_initial_sizes_match_reserved_slot_model(void);
static void test_null_object_returns_null_pointer(void);
static void test_uninitialized_object_returns_not_initialized(void);
static void test_query_rejects_null_output(void);
static void test_reset_returns_to_empty_without_clearing_storage(void);
```

Core expectation:

```c
uint8_t storage[8] = {0};
ring_buffer_t ringBuffer = {0};
platform_size_t readableSize = 0;
platform_size_t freeSize = 0;

assert(ring_buffer_init(&ringBuffer, storage, 8U) == PLATFORM_ERR_OK);
assert(ring_buffer_get_readable_size(&ringBuffer, &readableSize) == PLATFORM_ERR_OK);
assert(ring_buffer_get_free_size(&ringBuffer, &freeSize) == PLATFORM_ERR_OK);
assert(readableSize == 0U);
assert(freeSize == 7U);
```

Reset test must verify a pre-existing nonzero storage byte remains unchanged after reset.

- [ ] **Step 2: Run RED verification**

From `RTT_elog_DMA_UART_ring_project/`:

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  -I02_Service/service_common \
  -I03_Platform/platform_common \
  -I04_Impl/impl_board \
  Tests/ring_buffer/test_ring_buffer.c \
  02_Service/service_common/ring_buffer.c \
  -o Tests/ring_buffer/test_ring_buffer
```

Expected before implementation: FAIL because the new module does not yet exist.

- [ ] **Step 3: Implement public header**

Requirements:

```text
project file header
Header Guard
required Platform common includes only
ring_buffer_t exactly matches frozen spec
all six public API declarations
Chinese public API Doxygen in .h
SPSC ownership documented
caller-owned storage documented
capacity N-1 documented
reset quiescent-only documented
```

Do not add `ring_buffer_cfg.h`, magic fields, statistics, object base class, or runtime ops table.

- [ ] **Step 4: Implement private validation without inventing missing boolean macros**

Use an error-returning private helper, for example:

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

Do not introduce `PLATFORM_TRUE` / `PLATFORM_FALSE`; those symbols are not part of the current project baseline.

- [ ] **Step 5: Implement init/reset/query semantics**

Exact behavior:

```text
init ringBuffer NULL          -> NULL_POINTER
init storage NULL             -> NULL_POINTER
init storageSize < 2          -> INVALID_PARAM
valid init                    -> OK, readIndex=0, writeIndex=0

other API ringBuffer NULL     -> NULL_POINTER
object storage NULL/<2        -> NOT_INITIALIZED
required output NULL          -> NULL_POINTER
reset                         -> indexes only; no memset
```

Readable formula:

```c
if (writeIndex >= readIndex) {
    currentReadable = writeIndex - readIndex;
} else {
    currentReadable = ringBuffer->storageSize - readIndex + writeIndex;
}
```

Free formula:

```c
capacity = ringBuffer->storageSize - 1U;
currentFree = capacity - currentReadable;
```

- [ ] **Step 6: Run GREEN verification**

Compile with the Task 1 GCC command, then:

```bash
./Tests/ring_buffer/test_ring_buffer
```

Expected: all Task 1 tests PASS.

- [ ] **Step 7: Run coding-style checks and commit**

```bash
git diff --check
```

Review file header, naming, Doxygen location, 4-space indentation, braces, no TAB, no Yoda Condition, and no unnecessary dependencies.

Commit:

```bash
git add \
  RTT_elog_DMA_UART_ring_project/02_Service/service_common/ring_buffer.h \
  RTT_elog_DMA_UART_ring_project/02_Service/service_common/ring_buffer.c \
  RTT_elog_DMA_UART_ring_project/Tests/ring_buffer/test_ring_buffer.c

git commit -m "feat(ring): add spsc ring buffer core"
```

---

## Task 2: Non-Wrapping Write and Read

**Files:**
- Modify: `02_Service/service_common/ring_buffer.c`
- Modify: `Tests/ring_buffer/test_ring_buffer.c`

**Produces:**

```c
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
```

- [ ] **Step 1: Add failing tests**

Add:

```c
static void test_write_and_read_simple_sequence(void);
static void test_read_is_partial_when_output_is_smaller(void);
static void test_read_returns_empty_when_no_data_exists(void);
static void test_zero_length_write_allows_null_data(void);
static void test_zero_length_read_allows_null_buffer(void);
static void test_nonzero_write_rejects_null_data(void);
static void test_nonzero_read_rejects_null_buffer(void);
static void test_write_requires_written_length(void);
static void test_read_requires_read_length(void);
```

Example:

```c
uint8_t storage[8] = {0};
uint8_t input[3] = {0x11U, 0x22U, 0x33U};
uint8_t output[3] = {0};
ring_buffer_t ringBuffer = {0};
platform_size_t writtenLength = 0;
platform_size_t readLength = 0;

assert(ring_buffer_init(&ringBuffer, storage, 8U) == PLATFORM_ERR_OK);
assert(ring_buffer_write(&ringBuffer, input, 3U, &writtenLength) == PLATFORM_ERR_OK);
assert(writtenLength == 3U);
assert(ring_buffer_read(&ringBuffer, output, 3U, &readLength) == PLATFORM_ERR_OK);
assert(readLength == 3U);
assert(memcmp(input, output, 3U) == 0);
```

- [ ] **Step 2: Run RED**

Compile and execute the RingBuffer Host Test. New tests must fail before implementation.

- [ ] **Step 3: Implement Write ordering**

Required sequence:

```text
validate object
validate writtenLength
set *writtenLength = 0
handle dataLength == 0
validate data
snapshot readIndex
use Producer-owned writeIndex
calculate free
choose writeLength=min(dataLength, free)
copy data
publish writeIndex last
set writtenLength
return OK or OVERFLOW
```

- [ ] **Step 4: Implement Read ordering**

Required sequence:

```text
validate object
validate readLength
set *readLength = 0
handle bufferSize == 0
validate buffer
snapshot writeIndex
use Consumer-owned readIndex
calculate readable
if readable == 0 -> EMPTY
choose actualRead=min(bufferSize, readable)
copy data
publish readIndex last
set readLength
return OK
```

- [ ] **Step 5: Run GREEN**

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

Expected: Task 1 + Task 2 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add \
  RTT_elog_DMA_UART_ring_project/02_Service/service_common/ring_buffer.c \
  RTT_elog_DMA_UART_ring_project/Tests/ring_buffer/test_ring_buffer.c

git commit -m "feat(ring): add ring buffer read write"
```

---

## Task 3: Wrap, Full, and Partial-Write Overflow

**Files:**
- Modify: `02_Service/service_common/ring_buffer.c`
- Modify: `Tests/ring_buffer/test_ring_buffer.c`

- [ ] **Step 1: Add reserved-slot/full tests**

For `storage[8]`:

```text
usable capacity = 7
write 7 -> OK, written=7, free=0
write 1 -> OVERFLOW, written=0
read back -> original 7 bytes unchanged
```

Tests:

```c
static void test_capacity_is_storage_size_minus_one(void);
static void test_full_buffer_rejects_new_data_without_overwrite(void);
```

- [ ] **Step 2: Add partial-overflow test**

Scenario:

```text
storageSize=8, capacity=7
write 5
request write 4
```

Expected:

```text
writtenLength=2
return OVERFLOW
stream contains old 5 + first 2 new bytes
remaining 2 new bytes absent
```

Test:

```c
static void test_partial_write_preserves_old_data_and_reports_overflow(void);
```

- [ ] **Step 3: Add Wrap Write / Wrap Read tests using public operations only**

Sequence:

```text
write A B C D E
read A B C D
write F G H I J
read all
```

Expected final readable stream:

```text
E F G H I J
```

Tests must not force `readIndex` or `writeIndex` by directly writing struct members.

- [ ] **Step 4: Run RED**

Compile/run and confirm boundary tests expose any incomplete wrap/full behavior.

- [ ] **Step 5: Implement final two-segment copy**

Write:

```c
firstLength = writeLength;
if (firstLength > (ringBuffer->storageSize - writeIndex)) {
    firstLength = ringBuffer->storageSize - writeIndex;
}
secondLength = writeLength - firstLength;
```

Copy first segment to `storage[writeIndex]`, second segment to `storage[0]`, then publish final `writeIndex`.

Read uses the same two-segment structure with `readIndex` and only publishes `readIndex` after output copy completes.

Primary path must not become a byte-by-byte modulo loop.

- [ ] **Step 6: Verify old data is never silently overwritten**

All full/partial tests must read back exact preserved byte order.

- [ ] **Step 7: Commit**

```bash
git add \
  RTT_elog_DMA_UART_ring_project/02_Service/service_common/ring_buffer.c \
  RTT_elog_DMA_UART_ring_project/Tests/ring_buffer/test_ring_buffer.c

git commit -m "test(ring): cover wrap and overflow semantics"
```

---

## Task 4: Deterministic Reference-Model Stress Test

**Files:**
- Modify: `Tests/ring_buffer/test_ring_buffer.c`

- [ ] **Step 1: Add deterministic pseudo-random generator**

```c
static uint32_t test_next_random(uint32_t *state)
{
    *state = (*state * 1664525U) + 1013904223U;
    return *state;
}
```

Use a fixed seed.

- [ ] **Step 2: Add independent linear reference model**

Use a plain byte array plus `referenceLength`.

Reference semantics must independently implement:

```text
capacity=N-1
partial write
read up to request
EMPTY on nonzero read when empty
```

Do not call RingBuffer private calculations from the reference model.

- [ ] **Step 3: Execute at least 100000 operations**

Use small storage such as:

```text
storageSize=17
usable capacity=16
```

Pseudo-randomly alternate writes and reads. After every operation compare:

```text
return code
writtenLength/readLength
read byte content
readableSize
freeSize
```

- [ ] **Step 4: Run repeated deterministic verification**

```bash
./Tests/ring_buffer/test_ring_buffer
./Tests/ring_buffer/test_ring_buffer
```

Both runs must produce the same PASS result.

- [ ] **Step 5: Run sanitizers when locally available**

Preferred:

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  -fsanitize=address,undefined \
  -I02_Service/service_common \
  -I03_Platform/platform_common \
  -I04_Impl/impl_board \
  Tests/ring_buffer/test_ring_buffer.c \
  02_Service/service_common/ring_buffer.c \
  -o Tests/ring_buffer/test_ring_buffer_san

./Tests/ring_buffer/test_ring_buffer_san
```

If the installed Windows GCC lacks sanitizer runtime support, record `SANITIZER_NOT_AVAILABLE`; do not change production code to satisfy tool availability.

- [ ] **Step 6: Commit**

```bash
git add RTT_elog_DMA_UART_ring_project/Tests/ring_buffer/test_ring_buffer.c
git commit -m "test(ring): add deterministic stress coverage"
```

---

## Task 5: Contract Review and Regression Gate

**Files:**
- Review: `02_Service/service_common/ring_buffer.h`
- Review: `02_Service/service_common/ring_buffer.c`
- Review: `Tests/ring_buffer/test_ring_buffer.c`

- [ ] **Step 1: Verify SPSC state ownership**

By inspection:

```text
write path modifies writeIndex only
read path modifies readIndex only
queries modify neither
reset modifies both only under documented quiescent contract
```

There must be no shared `count`, shared `isFull`, lock, critical section, or hidden IRQ masking.

- [ ] **Step 2: Verify publication order**

```text
write: copy bytes -> publish writeIndex
read: copy bytes -> publish readIndex
```

Do not add CMSIS/FreeRTOS barriers without returning to design review.

- [ ] **Step 3: Verify exact error semantics**

```text
required pointer NULL        -> NULL_POINTER
init storageSize < 2         -> INVALID_PARAM
invalid initialized state    -> NOT_INITIALIZED
empty nonzero read           -> EMPTY
partial/zero-space write     -> OVERFLOW
complete operation           -> OK
zero-length read/write       -> OK
```

- [ ] **Step 4: Verify scope exclusions by search/review**

RingBuffer production code must contain none of:

```text
HAL_
osThread
xTask
FreeRTOS
platform_notify
platform_log
malloc
free
UART_HandleTypeDef
```

- [ ] **Step 5: Coding Standard Review**

Only record:

```text
Coding Standard Review: PASS
```

after checking file headers, naming, Chinese Doxygen, no duplicated public docs in `.c`, braces, no TAB, no Yoda Condition, const correctness, NULL/length checks, memcpy boundaries, ownership comments, and SPSC comments.

Run:

```bash
git diff --check
```

- [ ] **Step 6: Run final RingBuffer Host Test**

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

Required: PASS, zero compiler warnings.

- [ ] **Step 7: Run existing regression suites**

Re-run the repository Host suites for:

```text
Tests/platform_uart
Tests/impl_platform_uart
Tests/platform_os
Tests/platform_log
```

Use their existing compile commands unchanged. RingBuffer must not require modifying those modules or tests.

If any regression fails, stop and identify the cause before Keil integration.

- [ ] **Step 8: Commit review fixes only if needed**

```bash
git add \
  RTT_elog_DMA_UART_ring_project/02_Service/service_common \
  RTT_elog_DMA_UART_ring_project/Tests/ring_buffer

git commit -m "refactor(ring): finalize spsc ring buffer contract"
```

Skip if review needs no changes.

---

## Task 6: Keil Integration

**Files:**
- Modify: `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx`
- Do not modify: `Core/Src/freertos.c`

- [ ] **Step 1: Add exact include path**

```text
../02_Service/service_common
```

Do not add a broader Service parent path unless the existing project format requires it.

- [ ] **Step 2: Add RingBuffer source to Service group**

Preferred group:

```text
service/service_common
```

Compile:

```text
../02_Service/service_common/ring_buffer.c
```

- [ ] **Step 3: Do not add board-test code**

This is a pure software module. Do not modify:

```text
Core/Src/freertos.c
Core/Src/main.c
UART callback
DMA configuration
```

for RingBuffer runtime testing.

- [ ] **Step 4: Keil Clean + Full Rebuild**

```text
Clean Targets
→ Rebuild all target files
```

Required:

```text
0 Error(s)
```

Historical unrelated warnings may remain; do not refactor unrelated modules only to remove them.

If the Agent cannot run Keil, it must stop with:

```text
CODE_COMPLETE_PENDING_KEIL_VERIFICATION
```

and must not claim phase completion.

- [ ] **Step 5: Review `.uvprojx` diff**

Accept only the RingBuffer include path/group/source addition plus unavoidable XML ordering generated by Keil. Reject unrelated project churn.

- [ ] **Step 6: Commit integration**

```bash
git add RTT_elog_DMA_UART_ring_project/MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
git commit -m "build(ring): integrate ring buffer with keil"
```

---

## Task 7: Handoff and Closure

**Files:**
- Modify: `00_Doc/04_Agent/handoff.md`

- [ ] **Step 1: Append RingBuffer Phase 1 result**

Record:

```text
Status
Files changed
Frozen public API
SPSC index ownership
Storage/usable capacity
Partial-write overflow semantics
Host Test result
100000-op stress result
Sanitizer result or SANITIZER_NOT_AVAILABLE
Existing regression results
Coding Standard Review
Keil integration/build result
Deviations
Blockers
```

- [ ] **Step 2: Use evidence-based status**

Host complete but no real Keil build:

```text
CODE_COMPLETE_PENDING_KEIL_VERIFICATION
```

Only after real `0 Error(s)`:

```text
RingBuffer Phase 1 = COMPLETED
```

No hardware board-test evidence is required for this phase.

- [ ] **Step 3: Record next phase only as a candidate**

```text
Next candidate:
UART Service integration
= Platform UART RX_DATA + RingBuffer + Platform Notify
```

Do not create UART Service files.

- [ ] **Step 4: Commit handoff**

```bash
git add RTT_elog_DMA_UART_ring_project/00_Doc/04_Agent/handoff.md
git commit -m "docs(ring): record ring buffer phase 1 result"
```

---

# Completion Checklist

Before claiming `RingBuffer Phase 1 = COMPLETED`:

- [ ] `ring_buffer.h/.c` match frozen API.
- [ ] No UART/RTOS/log/statistics dependency.
- [ ] Caller-owned storage; no allocation.
- [ ] Capacity is `storageSize - 1`.
- [ ] Producer alone writes `writeIndex`.
- [ ] Consumer alone writes `readIndex`.
- [ ] No shared count/full flag.
- [ ] Partial Write returns OVERFLOW without overwriting old data.
- [ ] Empty nonzero Read returns EMPTY.
- [ ] Zero-length semantics match spec.
- [ ] Wrap Write PASS.
- [ ] Wrap Read PASS.
- [ ] 100000-operation deterministic reference-model stress PASS.
- [ ] RingBuffer GCC `-Wall -Wextra -Werror` PASS.
- [ ] Existing UART / Platform OS / Log regressions PASS.
- [ ] Coding Standard Review = PASS.
- [ ] Keil compiles `ring_buffer.c`.
- [ ] Keil Full Rebuild = `0 Error(s)`.
- [ ] No temporary board-test code exists.
- [ ] `handoff.md` records real results.

# STOP / BLOCKED Conditions

Stop and return to design review if implementation appears to require:

```text
changing frozen public API
shared count/isFull modified by both sides
mutex/semaphore/critical section/IRQ masking
CMSIS/FreeRTOS dependency
changing partial-write overflow semantics
statistics inside ring_buffer_t
zero-copy pointer lifetime API
hidden synchronization for reset
MPSC/MPMC support
```

Do not silently expand RingBuffer Phase 1.
