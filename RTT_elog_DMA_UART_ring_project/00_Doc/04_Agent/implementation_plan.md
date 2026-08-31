# Platform Log Naming Refactor Implementation Plan

> **For agentic workers:** Execute this plan task-by-task. Do not expand scope. Every self-developed `.c/.h` modification must follow `00_Doc/04_Agent/execution_rules.md` and the current V2.0 coding standard.

**Goal:** Resolve the confirmed Platform Log naming technical debt by migrating the self-developed public Log API and affected private Impl symbols to the current project naming convention, while preserving architecture, runtime behavior, EasyLogger/RTT integration, initialization semantics, and all existing Log features.

**Refactor Type:** Repository-internal breaking rename with complete call-site migration. No backward-compatibility aliases are required unless an actual external consumer is discovered during preflight.

**Architecture:** Keep the existing chain unchanged:

```text
APP / Service
    ↓
Platform Log API
    ↓
Impl Log Adapter
    ↓
EasyLogger / RTT
```

**Behavioral Principle:**

```text
API spelling changes
Behavior does not
Architecture does not
```

---

# 0. Mandatory References

Before modifying any project C/H file, read at minimum:

```text
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/handoff.md
03_Platform/platform_middleware/platform_log.h
04_Impl/impl_middleware/impl_log/easylogger_port.c
04_Impl/impl_middleware/impl_log/easylogger_port.h
```

Also inspect every production/test/documentation call site discovered by repository-wide search.

Preflight report must explicitly contain:

```text
Coding Standard:
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
Status: READ

Agent Execution Rules:
00_Doc/04_Agent/execution_rules.md
Status: READ

Platform Log Naming Refactor Scope:
Status: CONFIRMED
```

Current naming baseline:

```text
Functions           snake_case
Types               lower_snake_case_t
Macros              UPPER_SNAKE_CASE
Enum members        UPPER_SNAKE_CASE
File-local mutable  g_ + lowerCamelCase
```

If repository reality materially differs from this plan:

```text
STOP / BLOCKED
```

Do not silently redesign the module.

---

# 1. Frozen Scope

## 1.1 Public API Rename

Required public symbol migration:

```text
Platform_Log_Level_t
    -> platform_log_level_t

Platform_Log_Init()
    -> platform_log_init()

Platform_Log_SetLevel()
    -> platform_log_set_level()

Platform_Log_EnableOutput()
    -> platform_log_enable_output()

Platform_Log_GetOutputFn()
    -> platform_log_get_output_fn()
```

Keep unchanged:

```text
platform_log_output_fn_t

PLATFORM_LOG_LEVEL_ASSERT
PLATFORM_LOG_LEVEL_ERROR
PLATFORM_LOG_LEVEL_WARN
PLATFORM_LOG_LEVEL_INFO
PLATFORM_LOG_LEVEL_DEBUG
PLATFORM_LOG_LEVEL_VERBOSE
PLATFORM_LOG_LEVEL_MAX

platform_log_e(...)
platform_log_w(...)
platform_log_i(...)
platform_log_d(...)
platform_log_v(...)
```

The Log macros must only update their internal getter reference:

```text
Platform_Log_GetOutputFn()
    -> platform_log_get_output_fn()
```

## 1.2 Affected Impl Private Symbols

Within the self-developed Log adapter, migrate affected private functions to the current naming convention:

```text
Impl_Elog_NoOutput
    -> impl_elog_no_output

Impl_Elog_ConfigFormat
    -> impl_elog_config_format

Impl_Elog_ConvertLevel
    -> impl_elog_convert_level

Impl_Elog_AssertHook
    -> impl_elog_assert_hook

Impl_Elog_AsyncTask
    -> impl_elog_async_task
```

Only rename private symbols that belong to the same Log adapter and are directly within this refactor scope.

Do not use this task to rename unrelated modules.

## 1.3 Third-Party Symbols

The following are third-party / middleware APIs and must not be renamed merely for style:

```text
elog_init()
elog_start()
elog_output()
elog_set_filter_lvl()
elog_set_output_enabled()
elog_assert_set_hook()
elog_async_get_line_log()
elog_port_output()
CMSIS-RTOS2 APIs
SEGGER RTT APIs
```

---

# 2. Explicit Non-Goals

This refactor must NOT:

```text
- add service_log forwarding layer
- redesign Platform Log architecture
- redesign EasyLogger adapter
- redesign RTT output
- change synchronous/asynchronous Log mode
- change Log level semantics
- change filter behavior
- change no-op pre-init output behavior
- change initialization or failure cleanup semantics
- change Platform OS
- change UART / RingBuffer / APP logic
- introduce Config / Context / Data structures without real need
- add dynamic allocation
- add compatibility aliases by default
- edit Vendor / EasyLogger source merely for naming consistency
- perform repository-wide unrelated formatting cleanup
```

If an unrelated defect is discovered:

```text
record it separately
continue only if it does not block this refactor
```

If it blocks correctness:

```text
STOP / BLOCKED
```

---

# 3. Behavioral Contracts That Must Remain Unchanged

## 3.1 Pre-init Logging

Before successful initialization:

```text
platform_log_e/w/i/d/v(...)
    ↓
platform_log_get_output_fn()
    ↓
no-op backend
```

Logging before init must remain safe and non-crashing.

## 3.2 Initialization

`platform_log_init()` must preserve current behavior:

```text
already initialized
    -> return PLATFORM_ERR_OK

successful init
    -> configure EasyLogger
    -> register assert hook
    -> start EasyLogger
    -> switch output function to elog_output
    -> return PLATFORM_ERR_OK
```

Failure paths and cleanup behavior must remain equivalent to the current implementation.

## 3.3 Level Filtering

`platform_log_set_level()` must preserve:

```text
not initialized
    -> PLATFORM_ERR_NOT_INITIALIZED

invalid level >= PLATFORM_LOG_LEVEL_MAX
    -> PLATFORM_ERR_INVALID_PARAM

valid level
    -> map Platform level to EasyLogger level
    -> PLATFORM_ERR_OK
```

## 3.4 Output Enable

`platform_log_enable_output()` must preserve current enable/disable semantics and error handling.

## 3.5 Backend Getter

`platform_log_get_output_fn()` must continue returning the currently bound output backend.

---

# 4. Task 0 — Preflight and Impact Analysis

**Files:** read only initially.

- [ ] Run repository status/history checks:

```bash
git status --short
git log --oneline -n 15
```

- [ ] Read all Mandatory References.
- [ ] Confirm current `platform_log.h` public symbols match the expected old naming baseline.
- [ ] Confirm `easylogger_port.c` is the current implementation location for the Platform Log API.
- [ ] Search the entire repository for all old public symbols:

```text
Platform_Log_Level_t
Platform_Log_Init
Platform_Log_SetLevel
Platform_Log_EnableOutput
Platform_Log_GetOutputFn
```

- [ ] Search for affected old private symbols:

```text
Impl_Elog_NoOutput
Impl_Elog_ConfigFormat
Impl_Elog_ConvertLevel
Impl_Elog_AssertHook
Impl_Elog_AsyncTask
```

- [ ] Classify every match:

```text
production C/H
Host Test
CubeMX generated file / USER CODE
documentation
historical-only text
third-party/vendor
```

- [ ] Determine whether any external/public consumer outside this repository is actually required.

Default assumption:

```text
Repository-internal API only
No compatibility aliases
```

If evidence shows a required external consumer:

```text
STOP / BLOCKED
```

Return for compatibility-policy decision.

- [ ] Run `git diff --check`.

**Gate:** No production code changes before impact analysis is complete.

---

# 5. Task 1 — Strengthen / Update Platform Log Host Tests First

**Goal:** Ensure the rename is behavior-preserving and not validated only by compilation.

**Files:** inspect existing Log tests first; modify/create only the minimum necessary test files.

Required behavioral coverage where feasible in the existing Host Test architecture:

```text
pre-init getter returns safe backend
pre-init macro/backend invocation does not crash
init success
repeat init keeps existing behavior
set_level before init -> NOT_INITIALIZED
set_level invalid level -> INVALID_PARAM
set_level valid level -> success / expected backend mapping path
enable_output before init -> NOT_INITIALIZED
enable_output true/false -> expected backend call
get_output_fn after init -> active backend
```

- [ ] Reuse existing test harness instead of creating a parallel Log test framework if one exists.
- [ ] Update tests to target the new public names.
- [ ] If strict TDD is practical, demonstrate the expected compile failure against old public names before production rename.
- [ ] Do not weaken existing assertions merely to make the rename pass.

**Gate:** Tests must define the expected behavior before or together with the implementation rename.

---

# 6. Task 2 — Rename Platform Public API

**Primary file:**

```text
03_Platform/platform_middleware/platform_log.h
```

Required changes:

- [ ] Rename `Platform_Log_Level_t` -> `platform_log_level_t`.
- [ ] Rename four Platform Log public functions to snake_case.
- [ ] Update `platform_log_e/w/i/d/v` internal getter references.
- [ ] Keep enum member values/order unchanged.
- [ ] Keep `platform_log_output_fn_t` signature unchanged.
- [ ] Preserve existing public API semantics and Doxygen meaning.
- [ ] Update comments only where required by renamed symbols or current mandatory comment rules.
- [ ] Do not perform unrelated formatting rewrite.

Header isolation check:

```text
platform_log.h must remain independently includable
```

- [ ] Run header compile/isolation test if repository has an established mechanism.
- [ ] Run `git diff --check`.

---

# 7. Task 3 — Rename Impl Definitions and Private Symbols

**Primary file:**

```text
04_Impl/impl_middleware/impl_log/easylogger_port.c
```

Potential companion file:

```text
04_Impl/impl_middleware/impl_log/easylogger_port.h
```

Required changes:

- [ ] Rename public function definitions to match `platform_log.h`.
- [ ] Update parameter type from `Platform_Log_Level_t` to `platform_log_level_t`.
- [ ] Rename affected self-developed private functions listed in Section 1.2.
- [ ] Update all declarations and references consistently.
- [ ] Preserve EasyLogger and CMSIS-RTOS2 calls exactly in behavior.
- [ ] Preserve async cleanup paths exactly in behavior.
- [ ] Preserve no-op backend binding before initialization.
- [ ] Preserve `elog_output` binding after successful initialization.

Do NOT rename third-party function names.
Do NOT change resource lifecycle or error mapping merely because the file is being touched.

- [ ] Run Log Host Tests.
- [ ] Run `git diff --check`.
- [ ] Perform Coding Standard Review for modified C/H files.

Review must explicitly answer:

```text
1. Public functions now snake_case?
2. Public type now lower_snake_case_t?
3. Modified private self-developed functions compliant?
4. Any third-party symbol accidentally renamed?
5. Any behavior/resource/error-path change introduced?
```

---

# 8. Task 4 — Repository-Wide Call-Site Migration

Search and migrate all active call sites.

Potential areas include:

```text
APP
Service
Platform
Impl
Core USER CODE
Tests
Debug / Board test code
```

Rules:

- [ ] Replace old public symbol references with new names.
- [ ] Update type references.
- [ ] Do not add compatibility macros such as:

```c
#define Platform_Log_Init platform_log_init
```

unless Task 0 discovered a real required external compatibility contract and design approval was obtained.

- [ ] Documentation should be updated only where it describes the current API; historical narrative may retain old names if explicitly historical.
- [ ] Do not use broad search/replace that can corrupt unrelated text.

After migration, repository-wide active-code search must show zero old public symbols:

```text
Platform_Log_Level_t
Platform_Log_Init
Platform_Log_SetLevel
Platform_Log_EnableOutput
Platform_Log_GetOutputFn
```

Any intentional historical-document occurrence must be reviewed manually.

- [ ] Run `git diff --check`.

---

# 9. Task 5 — Host Regression Gate

Run the repository's existing Host Test suite relevant to this change.

Minimum expected coverage:

```text
Platform Log Host Test               PASS
Platform Log Header Isolation        PASS if available
APP Host Tests using Platform Log    PASS
UART Service regression              PASS if linked against Log
Platform OS regression               PASS if test build shares middleware integration
Dependency boundary checks           PASS
```

Do not claim a test passed unless the command actually ran successfully.

If some tests are unavailable in the environment, report:

```text
NOT RUN
reason: <exact reason>
```

Do not convert `NOT RUN` into `PASS`.

---

# 10. Task 6 — Keil Integration Gate

Perform a full Keil project rebuild using the repository's established method.

Required result:

```text
0 Error(s)
```

Warnings introduced by this refactor must be investigated.

- [ ] Confirm all new public symbols resolve.
- [ ] Confirm no stale old symbol remains in project sources.
- [ ] Confirm no duplicate compatibility symbol is accidentally exported.

If the Agent environment cannot run Keil:

```text
KEIL BUILD: NOT VERIFIED
```

Do not mark the phase complete until a real Keil result is provided by an appropriate environment/user verification.

---

# 11. Task 7 — RTT Runtime Regression

Because Log is a cross-cutting runtime facility, complete at least a small target-board/runtime smoke test after successful build.

Verify:

```text
platform_log_init()               -> success
platform_log_i(...)               -> visible RTT output
platform_log_e(...)               -> visible RTT output
platform_log_set_level(...)       -> filtering still works
platform_log_enable_output(false) -> output disabled
platform_log_enable_output(true)  -> output restored
```

Also verify startup logging before/around initialization does not cause a crash if such a path exists in the current product startup sequence.

Target board result must be recorded as one of:

```text
PASS
FAIL
NOT RUN
```

---

# 12. Task 8 — Final Static Review and Documentation Update

## 12.1 Old Symbol Scan

Repository active-code scan must confirm:

```text
Platform_Log_Level_t           -> 0 active occurrences
Platform_Log_Init              -> 0 active occurrences
Platform_Log_SetLevel          -> 0 active occurrences
Platform_Log_EnableOutput      -> 0 active occurrences
Platform_Log_GetOutputFn       -> 0 active occurrences
```

Private old names listed in Section 1.2 should also be zero in active code.

## 12.2 Coding Standard Review

Final status must be:

```text
Coding Standard Review: PASS
```

or the phase cannot be marked complete.

## 12.3 Handoff Update

Update:

```text
00_Doc/04_Agent/handoff.md
```

Change Platform Log naming technical debt from:

```text
CONFIRMED TECHNICAL DEBT
REQUIRES DEDICATED REFACTOR
```

to a resolved record only after all required verification has passed.

Recommended final record:

```text
Platform Log Naming Refactor       COMPLETED
Public API Naming                  V2.0 COMPLIANT
Host Regression                    PASS
Keil Full Rebuild                  PASS
RTT Runtime Regression             PASS
Coding Standard Review             PASS
```

If Keil or board verification remains pending, record the exact partial state instead of `COMPLETED`.

---

# 13. Completion Criteria

This refactor is `COMPLETED` only when all applicable criteria are satisfied:

```text
[ ] platform_log_level_t is the only active Platform Log level public type
[ ] all four public Platform Log functions use snake_case
[ ] platform_log_e/w/i/d/v use platform_log_get_output_fn()
[ ] affected self-developed Impl private symbols comply with current naming rules
[ ] no default backward-compatibility alias remains
[ ] active-code scan finds zero old public Platform Log symbols
[ ] architecture unchanged
[ ] runtime behavior unchanged
[ ] Platform Log Host Test PASS
[ ] relevant Host Regression PASS
[ ] Header Isolation PASS where supported
[ ] Dependency Boundary Scan PASS
[ ] Keil Full Rebuild PASS
[ ] RTT Runtime Regression PASS
[ ] Coding Standard Review PASS
[ ] handoff.md updated
```

If Host and source migration are complete but Keil / target runtime verification is pending, use:

```text
IMPLEMENTED / HOST_VERIFIED
KEIL_OR_BOARD_VERIFICATION_PENDING
```

Do not mark `COMPLETED` prematurely.

---

# 14. Suggested Commit Sequence

Recommended sequence:

```text
1. test(log): cover platform log behavior
2. refactor(log): normalize platform log api naming
3. test(log): run integration regressions
4. docs: complete platform log naming refactor
```

A single implementation commit is acceptable if the rename is tightly atomic and all call sites must change together to preserve buildability.

Do not mix unrelated technical debt cleanup into these commits.

---

# 15. Final Agent Report

At completion, report:

```text
Files changed:
<list>

Public API migration:
<old -> new>

Old active symbol scan:
PASS / FAIL

Host Tests:
<commands + PASS/FAIL/NOT RUN>

Keil Full Rebuild:
PASS / FAIL / NOT VERIFIED

RTT Runtime Regression:
PASS / FAIL / NOT RUN

Coding Standard Review:
PASS / NEEDS_FIX / EXCEPTION

Architecture behavior changed:
NO / YES

Remaining issues:
<none or exact list>
```

The Agent must not continue into protocol/application behavior implementation after completing this refactor.
