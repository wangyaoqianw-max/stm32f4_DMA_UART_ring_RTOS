# RingBuffer Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a pure-C SPSC byte-stream RingBuffer for later UART Service use, with caller-owned storage, partial-write overflow reporting, deterministic host tests, and Keil integration.

**Architecture:** `RingBuffer` lives in `02_Service/service_common/` and depends only on Platform common types/errors plus C memory-copy support. It knows nothing about UART, DMA, FreeRTOS, Notification, logging, protocol parsing, statistics, or task creation. Concurrency is frozen as Single Producer / Single Consumer with `writeIndex` owned by Producer and `readIndex` owned by Consumer.

**Tech Stack:** C, STM32F411CEU6, ARMCC5 / Keil MDK-ARM, GCC host tests.

**Spec:** `00_Doc/02_架构设计/RingBuffer_SPSC设计.md`

## Mandatory References

Before modifying any C/H file, read in this order:

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
- Create only `02_Service/service_common/ring_buffer.h`, `02_Service/service_common/ring_buffer.c`, and RingBuffer host tests, plus required Keil project integration and handoff update.
- Do not create `service_uart`, `service_uart_cfg`, UART statistics, Communication Task, protocol parser, or Notification integration.
- Do not modify Platform UART, UART DMA Impl, Platform OS, Vendor, HAL, CMSIS, FreeRTOS Kernel, or CubeMX-generated logic.
- No dynamic allocation.
- No Mutex, Semaphore, Critical Section, RTOS API, IRQ control, or logging inside RingBuffer.
- Storage is caller-owned and must remain valid while the RingBuffer is used.
- SPSC only: Producer is the sole writer of `writeIndex`; Consumer is the sole writer of `readIndex`.
- `storageSize = N`, usable capacity is exactly `N - 1`.
- No power-of-two capacity requirement.
- Write uses partial-write semantics: preserve old data, write the largest prefix that fits, return `PLATFORM_ERR_OVERFLOW` when `writtenLength < dataLength`.
- Read is non-blocking and reads up to the requested size.
- `reset()` is quiescent-only; it is not concurrent-safe.
- Statistics do not belong to RingBuffer V1.
- Follow the repository C coding standard for naming, Chinese comments, file headers, Doxygen, braces, NULL checking, ownership, and resource rules.

---

## Task 0: Preflight and Scope Confirmation

**Files:**
- Read: `00_Doc/04_Agent/execution_rules.md`
- Read: `00_Doc/02_架构设计/嵌入式项目C代码设计规范.md`
- Read: `00_Doc/02_架构设计/RingBuffer_SPSC设计.md`
- Read: `00_Doc/04_Agent/architecture.md`
- Read: `00_Doc/04_Agent/requirements.md`
- Read: `00_Doc/04_Agent/handoff.md`
- Inspect: `02_Service/`
- Inspect: `Tests/`

- [ ] **Step 1: Inspect repository state**

Run:

```bash
git status --short
git log --oneline -n 12
```

Expected:

- no destructive cleanup is required;
- RTOS Platform Phase 1 remains recorded as `COMPLETED`;
- no existing `service_uart` or RingBuffer implementation is present.

- [ ] **Step 2: Confirm scope in the execution report**

State explicitly:

```text
Phase: RingBuffer Phase 1
Scope: Pure SPSC RingBuffer only
UART Service: NOT IN SCOPE
Platform Notify integration: NOT IN SCOPE
Statistics: NOT IN SCOPE
Board test: NOT REQUIRED
```

- [ ] **Step 3: Protect unrelated work**

Do not run:

```text
git reset
git reset --hard
git checkout .
git clean
```

If unrelated uncommitted changes exist, leave them untouched and work around them.

**Deliverable:** verified execution context; no source modification.

---

## Task 1: Define RingBuffer Public Contract and Initialization

**Files:**
- Create: `02_Service/service_common/ring_buffer.h`
- Create: `02_Service/service_common/ring_buffer.c`
- Create: `Tests/ring_buffer/test_ring_buffer.c`

**Consumes:**

```c
platform_error_t
platform_size_t
uint8_t
```

from Platform common types/error headers.

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

- [ ] **Step 1: Write the first failing host tests**

Create `Tests/ring_buffer/test_ring_buffer.c` with test helpers and at least these cases:

```c
static void test_init_rejects_null_ring_buffer(void);
static void test_init_rejects_null_storage(void);
static void test_init_rejects_storage_smaller_than_two(void);
static void test_init_sets_empty_state(void);
static void test_initial_sizes_match_reserved_slot_model(void);
static void test_uninitialized_queries_fail(void);
static void test_reset_returns_to_empty_without_clearing_storage(void);
```

Core expectations:

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

For reset, preset a storage byte to a nonzero value before reset and verify the storage content is not bulk-cleared.

- [ ] **Step 2: Run the tests and verify RED**

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

Expected before implementation: FAIL because RingBuffer files/API do not yet exist.

- [ ] **Step 3: Implement the minimum public object and validation helpers**

`ring_buffer.h` must:

- use the project file-header template;
- use a Header Guard;
- include only required Platform common headers;
- define `ring_buffer_t` exactly as frozen by the spec;
- declare all six frozen public APIs, even if Write/Read are implemented in later tasks;
- document SPSC ownership, caller-owned storage, usable capacity `N - 1`, and reset quiescence in Chinese Doxygen.

`ring_buffer.c` must add a private initialized-state check equivalent to:

```c
static platform_bool_t ring_buffer_is_initialized(const ring_buffer_t *ringBuffer)
{
    if (ringBuffer == NULL) {
        return PLATFORM_FALSE;
    }

    if (ringBuffer->storage == NULL) {
        return PLATFORM_FALSE;
    }

    if (ringBuffer->storageSize < 2U) {
        return PLATFORM_FALSE;
    }

    return PLATFORM_TRUE;
}
```

Do not add a magic value, object base class, mutex, statistics, or heap ownership.

- [ ] **Step 4: Implement init/reset/query functions**

Required semantics:

```text
ring_buffer_init(NULL, ...)        -> NULL_POINTER
ring_buffer_init(..., NULL, ...)   -> NULL_POINTER
storageSize < 2                    -> INVALID_PARAM
valid init                         -> OK, indexes = 0

query/reset with invalid object    -> NOT_INITIALIZED
query with NULL output             -> NULL_POINTER
reset                              -> indexes = 0 only
```

Readable calculation:

```c
if (writeIndex >= readIndex) {
    readableSize = writeIndex - readIndex;
} else {
    readableSize = ringBuffer->storageSize - readIndex + writeIndex;
}
```

Free calculation:

```c
capacity = ringBuffer->storageSize - 1U;
freeSize = capacity - readableSize;
```

- [ ] **Step 5: Run GREEN verification**

Run the same GCC command and then:

```bash
./Tests/ring_buffer/test_ring_buffer
```

Expected: all Task 1 tests PASS.

- [ ] **Step 6: Coding Standard Review**

Check:

```text
file header
Header Guard
snake_case functions
lower_snake_case_t type
lowerCamelCase params/locals
Chinese public API Doxygen in .h
no duplicated public API Doxygen in .c
4 spaces, no TAB
no Yoda Condition
NULL and size validation
```

Run:

```bash
git diff --check
```

- [ ] **Step 7: Commit**

```bash
git add \
  RTT_elog_DMA_UART_ring_project/02_Service/service_common/ring_buffer.h \
  RTT_elog_DMA_UART_ring_project/02_Service/service_common/ring_buffer.c \
  RTT_elog_DMA_UART_ring_project/Tests/ring_buffer/test_ring_buffer.c

git commit -m "feat(ring): add spsc ring buffer core"
```

---

## Task 2: Implement Non-Wrapping Write and Read

**Files:**
- Modify: `02_Service/service_common/ring_buffer.c`
- Modify: `Tests/ring_buffer/test_ring_buffer.c`

**Consumes:** Task 1 object/query APIs.

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

- [ ] **Step 1: Add failing basic Write/Read tests**

Add at least:

```c
static void test_write_and_read_simple_sequence(void);
static void test_read_is_partial_when_output_buffer_is_smaller(void);
static void test_read_returns_empty_when_no_data_exists(void);
static void test_zero_length_write_succeeds_with_null_data(void);
static void test_zero_length_read_succeeds_with_null_buffer(void);
static void test_nonzero_write_rejects_null_data(void);
static void test_nonzero_read_rejects_null_buffer(void);
static void test_write_requires_written_length_output(void);
static void test_read_requires_read_length_output(void);
```

Example basic data test:

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

- [ ] **Step 2: Run RED verification**

Compile and run with the Task 1 GCC command.

Expected: FAIL because Write/Read behavior is not implemented yet.

- [ ] **Step 3: Implement simple synchronous Write path**

Required ordering:

```text
1. validate object / output pointer / zero-length semantics
2. snapshot readIndex
3. use owned writeIndex
4. calculate free space
5. choose writeLength = min(dataLength, freeSize)
6. copy bytes
7. publish writeIndex last
8. report writtenLength
9. return OK or OVERFLOW
```

For this task, implementation may already use the final two-segment copy helper so Task 3 only adds wrap-specific tests.

- [ ] **Step 4: Implement simple synchronous Read path**

Required ordering:

```text
1. validate object / output pointer / zero-length semantics
2. snapshot writeIndex
3. use owned readIndex
4. calculate readable bytes
5. if requested > 0 and readable == 0 -> EMPTY
6. choose readLength = min(bufferSize, readable)
7. copy bytes
8. publish readIndex last
9. return OK
```

- [ ] **Step 5: Run GREEN verification**

Run:

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

Expected: all Task 1 + Task 2 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add \
  RTT_elog_DMA_UART_ring_project/02_Service/service_common/ring_buffer.c \
  RTT_elog_DMA_UART_ring_project/Tests/ring_buffer/test_ring_buffer.c

git commit -m "feat(ring): add ring buffer read write"
```

---

## Task 3: Freeze Wrap and Overflow Behavior

**Files:**
- Modify: `02_Service/service_common/ring_buffer.c`
- Modify: `Tests/ring_buffer/test_ring_buffer.c`

**Consumes:** frozen partial-write semantics from the spec.

**Produces:** verified Wrap, Full, and Partial Write behavior.

- [ ] **Step 1: Add failing capacity/full tests**

For `uint8_t storage[8]`, verify usable capacity is 7:

```text
write 7 bytes -> OK, written = 7, free = 0
write 1 more  -> OVERFLOW, written = 0
old 7 bytes   -> unchanged and readable in original order
```

Add:

```c
static void test_reserved_slot_capacity_is_storage_size_minus_one(void);
static void test_full_buffer_rejects_new_data_without_overwriting_old_data(void);
```

- [ ] **Step 2: Add failing partial-overflow test**

Scenario:

```text
storageSize = 8, capacity = 7
write 5 bytes
free = 2
request another 4 bytes
```

Expected:

```text
writtenLength = 2
return OVERFLOW
final stream = original 5 bytes + first 2 bytes of new input
last 2 new bytes are absent
```

Add:

```c
static void test_partial_write_preserves_old_data_and_reports_overflow(void);
```

- [ ] **Step 3: Add failing Wrap Write test**

Force indexes near the physical end through normal public operations, not by directly modifying private state in the test.

Example sequence with storage size 8:

```text
write 5:  A B C D E
read 4:   consume A B C D
write 5:  F G H I J
```

Expected readable stream:

```text
E F G H I J
```

and physical copy must wrap correctly.

- [ ] **Step 4: Add failing Wrap Read test**

Read the wrapped stream into a linear output buffer and verify exact order:

```text
E F G H I J
```

- [ ] **Step 5: Run RED verification**

Compile and run the RingBuffer suite.

Expected: at least the newly introduced boundary tests fail until final wrap/overflow logic is correct.

- [ ] **Step 6: Implement final two-segment copy logic**

Write path equivalent:

```c
firstLength = writeLength;
if (firstLength > (ringBuffer->storageSize - writeIndex)) {
    firstLength = ringBuffer->storageSize - writeIndex;
}

secondLength = writeLength - firstLength;
```

Copy:

```text
input[0:firstLength]
    -> storage[writeIndex:]

input[firstLength:]
    -> storage[0:secondLength]
```

Then calculate and publish the final `writeIndex`.

Read path uses the same two-segment principle with `readIndex`.

No byte-by-byte modulo loop is required as the primary implementation path.

- [ ] **Step 7: Verify no silent overwrite**

Run all tests and confirm the full-buffer and partial-overflow tests explicitly read back preserved old data.

- [ ] **Step 8: Commit**

```bash
git add \
  RTT_elog_DMA_UART_ring_project/02_Service/service_common/ring_buffer.c \
  RTT_elog_DMA_UART_ring_project/Tests/ring_buffer/test_ring_buffer.c

git commit -m "test(ring): cover wrap and overflow semantics"
```

---

## Task 4: Add Deterministic Reference-Model Stress Test

**Files:**
- Modify: `Tests/ring_buffer/test_ring_buffer.c`

**Consumes:** final RingBuffer public API.

**Produces:** long-running deterministic verification against a simple linear reference queue.

- [ ] **Step 1: Add a tiny deterministic pseudo-random generator**

Use a local test-only LCG, for example:

```c
static uint32_t test_next_random(uint32_t *state)
{
    *state = (*state * 1664525U) + 1013904223U;
    return *state;
}
```

Do not use nondeterministic seeds.

- [ ] **Step 2: Add a simple reference model**

Use a linear array and explicit `referenceLength` representing the expected byte stream.

The model must implement the same semantics:

```text
capacity = N - 1
partial write when free is insufficient
read up to requested length
EMPTY when read requested and referenceLength == 0
```

Do not reuse RingBuffer calculations inside the reference model.

- [ ] **Step 3: Add at least 100000 deterministic operations**

Alternate pseudo-randomly between write and read operations.

For every operation compare:

```text
return code
writtenLength / readLength
read bytes
readable size
free size
```

Use a small RingBuffer, e.g. 17-byte storage / 16-byte usable capacity, to force frequent wrap/full/empty transitions.

- [ ] **Step 4: Run stress test**

Run:

```bash
./Tests/ring_buffer/test_ring_buffer
```

Expected: deterministic PASS on repeated executions.

- [ ] **Step 5: Run memory-safety friendly host build if GCC supports it in the local environment**

Preferred additional command:

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

If the Windows GCC toolchain does not provide sanitizers, record `SANITIZER_NOT_AVAILABLE`; do not treat tool absence as a code failure.

- [ ] **Step 6: Commit**

```bash
git add RTT_elog_DMA_UART_ring_project/Tests/ring_buffer/test_ring_buffer.c
git commit -m "test(ring): add deterministic stress coverage"
```

---

## Task 5: API, Concurrency, and Coding-Standard Review

**Files:**
- Review: `02_Service/service_common/ring_buffer.h`
- Review: `02_Service/service_common/ring_buffer.c`
- Review: `Tests/ring_buffer/test_ring_buffer.c`

- [ ] **Step 1: Review SPSC ownership**

Verify by inspection:

```text
write path never modifies readIndex
read path never modifies writeIndex
query APIs modify neither
reset modifies both only under documented quiescent-only contract
```

Reject any introduced shared `count`, shared `isFull`, mutex, or hidden critical section.

- [ ] **Step 2: Review publication order**

Verify:

```text
write: data copy completes before writeIndex publication
read:  output copy completes before readIndex publication
```

Do not add CMSIS/FreeRTOS barriers in this pure module without returning to design review.

- [ ] **Step 3: Review error semantics**

Verify exact behavior:

```text
NULL required pointer       -> NULL_POINTER
storageSize < 2 on init     -> INVALID_PARAM
invalid existing object     -> NOT_INITIALIZED
empty non-zero read         -> EMPTY
partial / zero-space write  -> OVERFLOW
complete write/read         -> OK
zero-length write/read      -> OK
```

- [ ] **Step 4: Review scope exclusions**

Confirm RingBuffer contains none of:

```text
UART
DMA
FreeRTOS
CMSIS-RTOS2
Notification
Platform Log
statistics
malloc/free
protocol parsing
```

- [ ] **Step 5: Coding Standard Review**

Record result internally as:

```text
Coding Standard Review: PASS
```

only after checking:

- file header;
- Header Guard;
- naming;
- Chinese Doxygen in `.h`;
- no duplicated public API documentation in `.c`;
- 4 spaces / no TAB;
- brace rules;
- no Yoda Condition;
- no unchecked bounds around memcpy;
- const correctness;
- clear ownership and SPSC comments.

Run:

```bash
git diff --check
```

- [ ] **Step 6: Re-run complete RingBuffer host suite**

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

Expected: PASS with zero compiler warnings.

- [ ] **Step 7: Run existing subsystem regression suites**

At minimum re-run the current repository Host suites for:

```text
Tests/platform_uart
Tests/impl_platform_uart
Tests/platform_os
Tests/platform_log
```

Use their existing compile commands unchanged; RingBuffer must not require changes to those tests or their production modules.

If a regression fails, stop and determine whether RingBuffer introduced the failure before continuing to Keil integration.

- [ ] **Step 8: Commit review-only fixes if needed**

```bash
git add \
  RTT_elog_DMA_UART_ring_project/02_Service/service_common \
  RTT_elog_DMA_UART_ring_project/Tests/ring_buffer

git commit -m "refactor(ring): finalize spsc ring buffer contract"
```

Skip this commit if no review fix is needed.

---

## Task 6: Keil Integration and Final Verification

**Files:**
- Modify: `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx`
- Do not modify: `Core/Src/freertos.c`

- [ ] **Step 1: Add Service include path**

Add exactly the required path:

```text
../02_Service/service_common
```

Do not add broad parent directories unnecessarily.

- [ ] **Step 2: Add RingBuffer source to a Service-oriented Keil group**

Preferred group:

```text
service/service_common
```

Add:

```text
../02_Service/service_common/ring_buffer.c
```

Header does not need to be compiled; adding it to the group is optional and must not be required for the build.

- [ ] **Step 3: Do not add a board smoke test**

Because RingBuffer is hardware-independent pure C, do not modify:

```text
Core/Src/freertos.c
main.c
UART callback
DMA configuration
```

for runtime verification.

- [ ] **Step 4: Execute Keil Clean + Full Rebuild**

Local MDK procedure:

```text
Clean Targets
→ Rebuild all target files
```

Required result:

```text
0 Error(s)
```

Existing unrelated warnings may remain; do not modify unrelated modules solely to remove historical warnings.

If the executing Agent cannot invoke Keil, status must remain:

```text
CODE_COMPLETE_PENDING_KEIL_VERIFICATION
```

and it must stop before claiming completion.

- [ ] **Step 5: Verify project file scope**

Check the `.uvprojx` diff contains only:

```text
Service include path
RingBuffer group/source entry
```

plus unavoidable XML ordering/format changes from Keil. Do not accept unrelated group churn.

- [ ] **Step 6: Commit Keil integration**

```bash
git add RTT_elog_DMA_UART_ring_project/MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
git commit -m "build(ring): integrate ring buffer with keil"
```

---

## Task 7: Handoff and Phase Closure

**Files:**
- Modify: `00_Doc/04_Agent/handoff.md`

- [ ] **Step 1: Record implementation result**

Append a RingBuffer Phase 1 section including:

```text
Status
Files changed
Frozen public API
SPSC ownership rule
Storage / usable capacity rule
Partial-write overflow semantics
Host Test result
Stress Test result
Existing regression result
Coding Standard Review
Keil integration status
Keil build status
Deviations
Blockers
```

- [ ] **Step 2: Use evidence-based status**

If Host tests pass but Keil has not been run:

```text
CODE_COMPLETE_PENDING_KEIL_VERIFICATION
```

Only after real Keil `0 Error(s)` evidence may the handoff say:

```text
RingBuffer Phase 1 = COMPLETED
```

No board-test evidence is required for this pure software phase.

- [ ] **Step 3: Record next phase without implementing it**

The handoff may state only:

```text
Next candidate phase:
UART Service integration
    = Platform UART RX_DATA
    + RingBuffer
    + Platform Notify
```

Do not create UART Service files in this phase.

- [ ] **Step 4: Commit handoff**

```bash
git add RTT_elog_DMA_UART_ring_project/00_Doc/04_Agent/handoff.md
git commit -m "docs(ring): record ring buffer phase 1 result"
```

---

# Final Completion Checklist

Before claiming `RingBuffer Phase 1 = COMPLETED`, verify all boxes:

- [ ] `ring_buffer.h` exists and matches frozen API.
- [ ] `ring_buffer.c` exists and contains no UART/RTOS/log/statistics dependencies.
- [ ] Caller-owned storage; no dynamic allocation.
- [ ] Usable capacity is `storageSize - 1`.
- [ ] Producer only writes `writeIndex`.
- [ ] Consumer only writes `readIndex`.
- [ ] No shared count/full flag.
- [ ] Partial write preserves old data and returns `PLATFORM_ERR_OVERFLOW`.
- [ ] Empty non-zero read returns `PLATFORM_ERR_EMPTY`.
- [ ] Zero-length read/write semantics match spec.
- [ ] Wrap Write PASS.
- [ ] Wrap Read PASS.
- [ ] Deterministic 100000-operation reference-model stress PASS.
- [ ] RingBuffer host build uses `-Wall -Wextra -Werror` and PASS.
- [ ] Existing UART / Platform OS / Log host regressions PASS.
- [ ] Coding Standard Review = PASS.
- [ ] Keil project includes `../02_Service/service_common`.
- [ ] Keil project compiles `ring_buffer.c`.
- [ ] Full Keil Rebuild = `0 Error(s)`.
- [ ] No temporary board-test code exists.
- [ ] `handoff.md` records the real result.

# STOP / BLOCKED Conditions

Stop and return to design review if implementation requires any of the following:

```text
changing the frozen public API
adding a shared count/isFull modified by both sides
adding mutex/semaphore/critical section/IRQ masking
adding CMSIS/FreeRTOS dependencies
changing overflow from partial-write semantics
adding statistics to ring_buffer_t
adding zero-copy pointer lifetime semantics
making reset concurrent-safe through hidden synchronization
supporting MPSC/MPMC
```

Do not solve those cases by silently expanding the RingBuffer Phase 1 scope.
