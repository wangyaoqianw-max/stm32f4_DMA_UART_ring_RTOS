# Service Log Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立一个薄的 Service Log 策略层，使 APP / Service 统一通过 `SERVICE_LOG_xxx()` 输出普通日志，同时复用现有 Platform Log + EasyLogger + RTT 实现。

**Architecture:** `service_log` 只负责初始化编排、默认日志策略、Level 映射和上层统一宏；不实现新的 RingBuffer、任务或输出 Backend。正常日志链冻结为 `APP / Service -> service_log -> platform_log -> EasyLogger -> RTT`，普通底层错误继续通过 `platform_error_t` 向上传递并由拥有恢复上下文的上层只记录一次。

**Tech Stack:** C11、工程现有 `platform_error_t`、Platform Log、EasyLogger、SEGGER RTT、MinGW GCC Host Tests、Keil MDK、STM32F411 + FreeRTOS。

**Spec:** `RTT_elog_DMA_UART_ring_project/00_Doc/02_架构设计/Service_Log_Phase1设计.md`

## Global Constraints

- Service Log Phase 1 只实现薄策略层，不新增日志 RingBuffer、后台任务、Async Queue、UART DMA Backend 或 Flash 持久化。
- APP / Service 正常日志统一使用 `SERVICE_LOG_E/W/I/D/V()`；迁移后不得继续直接调用 `platform_log_e/w/i/d/v()`。
- `service_log` 继续返回 `platform_error_t`，不得新增 `service_error_t`。
- Service Log 只暴露 ERROR / WARN / INFO / DEBUG / VERBOSE，不暴露 ASSERT Level。
- Assert / HardFault / BusFault / UsageFault 不纳入本阶段；CmBacktrace 留给后续 Fatal Diagnostics。
- 普通日志只保证 Task Context 使用；不得在 UART / DMA ISR 中调用 `SERVICE_LOG_xxx()`。
- 项目默认日志 Level 与 Output Enable 放入 `00_Config/project_log_config.h`。
- `service_log_init()` 必须幂等；成功后的重复调用不得覆盖运行期 Level / Enable。
- 初始化失败必须通过错误码返回，不依赖普通日志报告自身失败。
- 不修改 EasyLogger Vendor 源码，不重构现有 Platform Log 公共 API 命名。
- 自研代码的文件、API、函数内和类型说明注释默认使用中文。
- 保留用户已有未提交改动，Git 操作只指定本计划涉及的文件。

---

## File Structure

### Create

```text
RTT_elog_DMA_UART_ring_project/00_Config/project_log_config.h
RTT_elog_DMA_UART_ring_project/Tests/service_log/test_service_log.c
```

### Modify

```text
RTT_elog_DMA_UART_ring_project/02_Service/service_log/service_log.h
RTT_elog_DMA_UART_ring_project/02_Service/service_log/service_log.c
RTT_elog_DMA_UART_ring_project/01_APP/app_communication.c
RTT_elog_DMA_UART_ring_project/01_APP/app_system.c
RTT_elog_DMA_UART_ring_project/Core/Src/freertos.c
RTT_elog_DMA_UART_ring_project/Tests/app_communication/test_app_communication.c
RTT_elog_DMA_UART_ring_project/Tests/app_system/test_app_system.c
```

### Delete

```text
RTT_elog_DMA_UART_ring_project/02_Service/service_log/service_log_cfg.h
```

The empty `service_log/README.md` may remain unchanged.

---

### Task 1: 建立 Service Log 公共契约与项目默认配置

**Files:**
- Create: `RTT_elog_DMA_UART_ring_project/00_Config/project_log_config.h`
- Modify: `RTT_elog_DMA_UART_ring_project/02_Service/service_log/service_log.h`
- Create: `RTT_elog_DMA_UART_ring_project/Tests/service_log/test_service_log.c`

**Interfaces:**
- Consumes: `platform_error_t`、`platform_log_level_t`、`platform_log_e/w/i/d/v()`、`bool`。
- Produces: `service_log_level_t`、`service_log_init()`、`service_log_set_level()`、`service_log_enable_output()`、`SERVICE_LOG_E/W/I/D/V()`、`PROJECT_LOG_DEFAULT_LEVEL`、`PROJECT_LOG_DEFAULT_OUTPUT_ENABLE`。

- [ ] **Step 1: 写公共接口失败测试**

Create `Tests/service_log/test_service_log.c` with a fake Platform Log backend declared in the test translation unit:

```c
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define LOG_TAG "service-log-test"
#include "service_log.h"

#define TEST_ASSERT(condition)          \
    do {                                \
        if (!(condition)) {             \
            return __LINE__;            \
        }                               \
    } while (0)

typedef struct
{
    uint32_t initCallCount;
    uint32_t setLevelCallCount;
    uint32_t enableCallCount;
    uint32_t outputCallCount;
    platform_log_level_t level;
    bool enabled;
    const char *tag;
    const char *format;
    int argument;
} fake_platform_log_t;

static fake_platform_log_t g_log;
static platform_error_t g_initResult = PLATFORM_ERR_OK;
static platform_error_t g_setLevelResult = PLATFORM_ERR_OK;
static platform_error_t g_enableResult = PLATFORM_ERR_OK;

platform_error_t platform_log_init(void)
{
    g_log.initCallCount++;
    return g_initResult;
}

platform_error_t platform_log_set_level(platform_log_level_t level)
{
    g_log.setLevelCallCount++;
    g_log.level = level;
    return g_setLevelResult;
}

platform_error_t platform_log_enable_output(bool enable)
{
    g_log.enableCallCount++;
    g_log.enabled = enable;
    return g_enableResult;
}

static void fake_output(uint8_t level,
                        const char *tag,
                        const char *file,
                        const char *func,
                        long line,
                        const char *format,
                        ...)
{
    va_list args;

    (void)level;
    (void)file;
    (void)func;
    (void)line;

    va_start(args, format);
    g_log.tag = tag;
    g_log.format = format;
    g_log.argument = va_arg(args, int);
    g_log.outputCallCount++;
    va_end(args);
}

platform_log_output_fn_t platform_log_get_output_fn(void)
{
    return fake_output;
}
```

Add compile-time/API checks in `main()`:

```c
int main(void)
{
    service_log_level_t level = SERVICE_LOG_LEVEL_INFO;

    TEST_ASSERT(SERVICE_LOG_LEVEL_MAX > level);
    TEST_ASSERT(PLATFORM_ERR_OK == service_log_init());
    TEST_ASSERT(PLATFORM_ERR_OK == service_log_set_level(SERVICE_LOG_LEVEL_DEBUG));
    TEST_ASSERT(PLATFORM_ERR_OK == service_log_enable_output(true));

    SERVICE_LOG_I("value=%d", 42);

    TEST_ASSERT(0 == strcmp("service-log-test", g_log.tag));
    TEST_ASSERT(0 == strcmp("value=%d", g_log.format));
    TEST_ASSERT(42 == g_log.argument);

    return 0;
}
```

- [ ] **Step 2: 编译并确认因 Service Log 公共接口缺失而失败**

Run from repository root:

```powershell
gcc -std=c11 -Wall -Wextra -Werror `
  -I RTT_elog_DMA_UART_ring_project/03_Platform/platform_common `
  -I RTT_elog_DMA_UART_ring_project/03_Platform/platform_middleware `
  -I RTT_elog_DMA_UART_ring_project/02_Service/service_log `
  -I RTT_elog_DMA_UART_ring_project/00_Config `
  RTT_elog_DMA_UART_ring_project/Tests/service_log/test_service_log.c `
  RTT_elog_DMA_UART_ring_project/02_Service/service_log/service_log.c `
  -o RTT_elog_DMA_UART_ring_project/Tests/service_log/test_service_log.exe
```

Expected: FAIL because `service_log_level_t`, `SERVICE_LOG_*` macros or Service Log functions are not yet defined.

- [ ] **Step 3: 建立 `project_log_config.h`**

Create:

```c
/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file project_log_config.h
 * @brief 定义产品级日志静态策略。
 * @author YaoQian Wang
 * @date 2026-08-31
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PROJECT_LOG_CONFIG_H
#define PROJECT_LOG_CONFIG_H

//******************************** Includes *********************************//
#include <stdbool.h>
#include "service_log.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define PROJECT_LOG_DEFAULT_LEVEL          SERVICE_LOG_LEVEL_INFO
#define PROJECT_LOG_DEFAULT_OUTPUT_ENABLE  true
//******************************** Defines *********************************//

#endif
```

- [ ] **Step 4: 建立 `service_log.h` 公共接口**

Replace the empty file with:

```c
/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_log.h
 * @brief Service 层统一普通日志接口。
 * @author YaoQian Wang
 * @date 2026-08-31
 * @version V1.0
 *
 *****************************************************************************/

#ifndef SERVICE_LOG_H
#define SERVICE_LOG_H

//******************************** Includes *********************************//
#include <stdbool.h>
#include "platform_error.h"
#include "platform_log.h"
//******************************** Includes *********************************//

//******************************** Types ***********************************//
typedef enum
{
    SERVICE_LOG_LEVEL_ERROR = 0,
    SERVICE_LOG_LEVEL_WARN,
    SERVICE_LOG_LEVEL_INFO,
    SERVICE_LOG_LEVEL_DEBUG,
    SERVICE_LOG_LEVEL_VERBOSE,
    SERVICE_LOG_LEVEL_MAX
} service_log_level_t;
//******************************** Types ***********************************//

//******************************** Defines *********************************//
#define SERVICE_LOG_E(...) platform_log_e(__VA_ARGS__)
#define SERVICE_LOG_W(...) platform_log_w(__VA_ARGS__)
#define SERVICE_LOG_I(...) platform_log_i(__VA_ARGS__)
#define SERVICE_LOG_D(...) platform_log_d(__VA_ARGS__)
#define SERVICE_LOG_V(...) platform_log_v(__VA_ARGS__)
//******************************** Defines *********************************//

//******************************** Declaring *******************************//
platform_error_t service_log_init(void);
platform_error_t service_log_set_level(service_log_level_t level);
platform_error_t service_log_enable_output(bool enable);
//******************************** Declaring *******************************//

#endif
```

- [ ] **Step 5: 重新编译确认剩余失败只来自实现缺失**

Run the Step 2 command.

Expected: header compiles; linker fails only because `service_log_init()` / control functions are not yet implemented.

- [ ] **Step 6: 提交公共契约与配置**

```powershell
git add -- `
  RTT_elog_DMA_UART_ring_project/00_Config/project_log_config.h `
  RTT_elog_DMA_UART_ring_project/02_Service/service_log/service_log.h `
  RTT_elog_DMA_UART_ring_project/Tests/service_log/test_service_log.c
git commit -m "feat: define Service Log public contract"
```

---

### Task 2: 实现 Service Log 初始化、Level 映射和幂等策略

**Files:**
- Modify: `RTT_elog_DMA_UART_ring_project/02_Service/service_log/service_log.c`
- Modify: `RTT_elog_DMA_UART_ring_project/Tests/service_log/test_service_log.c`
- Delete: `RTT_elog_DMA_UART_ring_project/02_Service/service_log/service_log_cfg.h`

**Interfaces:**
- Consumes: Task 1 的 Service Log 公共契约、`PROJECT_LOG_DEFAULT_LEVEL`、`PROJECT_LOG_DEFAULT_OUTPUT_ENABLE`、Platform Log 控制 API。
- Produces: 可工作的 `service_log_init()`、`service_log_set_level()`、`service_log_enable_output()`。

- [ ] **Step 1: 扩展失败测试覆盖初始化顺序和错误传播**

Add helpers:

```c
static void reset_fake(void)
{
    g_log = (fake_platform_log_t){0};
    g_initResult = PLATFORM_ERR_OK;
    g_setLevelResult = PLATFORM_ERR_OK;
    g_enableResult = PLATFORM_ERR_OK;
}
```

Add these tests before `main()`:

```c
static int test_init_applies_default_policy(void)
{
    reset_fake();

    TEST_ASSERT(PLATFORM_ERR_OK == service_log_init());
    TEST_ASSERT(1U == g_log.initCallCount);
    TEST_ASSERT(1U == g_log.setLevelCallCount);
    TEST_ASSERT(PLATFORM_LOG_LEVEL_INFO == g_log.level);
    TEST_ASSERT(1U == g_log.enableCallCount);
    TEST_ASSERT(true == g_log.enabled);

    return 0;
}

static int test_invalid_level_rejected(void)
{
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                service_log_set_level(SERVICE_LOG_LEVEL_MAX));
    return 0;
}

static int test_level_mapping(void)
{
    TEST_ASSERT(PLATFORM_ERR_OK == service_log_set_level(SERVICE_LOG_LEVEL_ERROR));
    TEST_ASSERT(PLATFORM_LOG_LEVEL_ERROR == g_log.level);
    TEST_ASSERT(PLATFORM_ERR_OK == service_log_set_level(SERVICE_LOG_LEVEL_WARN));
    TEST_ASSERT(PLATFORM_LOG_LEVEL_WARN == g_log.level);
    TEST_ASSERT(PLATFORM_ERR_OK == service_log_set_level(SERVICE_LOG_LEVEL_INFO));
    TEST_ASSERT(PLATFORM_LOG_LEVEL_INFO == g_log.level);
    TEST_ASSERT(PLATFORM_ERR_OK == service_log_set_level(SERVICE_LOG_LEVEL_DEBUG));
    TEST_ASSERT(PLATFORM_LOG_LEVEL_DEBUG == g_log.level);
    TEST_ASSERT(PLATFORM_ERR_OK == service_log_set_level(SERVICE_LOG_LEVEL_VERBOSE));
    TEST_ASSERT(PLATFORM_LOG_LEVEL_VERBOSE == g_log.level);
    return 0;
}
```

Add separate process-level test cases or a test-only reset hook only if needed to test failed-init retry without leaking test concerns into production API. Prefer compiling the same test source multiple times with a macro selecting one scenario, so production code keeps no reset API.

Required scenarios:

```text
platform_log_init failure        -> same error returned, init may be retried
platform_log_set_level failure   -> same error returned, init may be retried
platform_log_enable_output failure -> same error returned, init may be retried
successful init repeated         -> no additional Platform init/config calls
runtime set_level after init     -> repeated service_log_init does not restore default
runtime enable change after init -> repeated service_log_init does not restore default
```

- [ ] **Step 2: 编译并确认新增行为测试失败**

Use the Task 1 compile command.

Expected: FAIL because Service Log implementation is still empty.

- [ ] **Step 3: 实现 Level 转换**

Add to `service_log.c`:

```c
static platform_log_level_t service_log_convert_level(service_log_level_t level)
{
    switch (level) {
        case SERVICE_LOG_LEVEL_ERROR:
            return PLATFORM_LOG_LEVEL_ERROR;
        case SERVICE_LOG_LEVEL_WARN:
            return PLATFORM_LOG_LEVEL_WARN;
        case SERVICE_LOG_LEVEL_INFO:
            return PLATFORM_LOG_LEVEL_INFO;
        case SERVICE_LOG_LEVEL_DEBUG:
            return PLATFORM_LOG_LEVEL_DEBUG;
        case SERVICE_LOG_LEVEL_VERBOSE:
            return PLATFORM_LOG_LEVEL_VERBOSE;
        default:
            return PLATFORM_LOG_LEVEL_ERROR;
    }
}
```

- [ ] **Step 4: 实现控制 API**

Implement:

```c
platform_error_t service_log_set_level(service_log_level_t level)
{
    if (level >= SERVICE_LOG_LEVEL_MAX) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return platform_log_set_level(service_log_convert_level(level));
}

platform_error_t service_log_enable_output(bool enable)
{
    return platform_log_enable_output(enable);
}
```

Do not add a second independent Service runtime level state; Platform Log / EasyLogger remains the source of truth for output filtering.

- [ ] **Step 5: 实现幂等初始化**

Implement with file-local state:

```c
static bool g_serviceLogInitialized = false;

platform_error_t service_log_init(void)
{
    platform_error_t result = PLATFORM_ERR_OK;

    if (g_serviceLogInitialized) {
        return PLATFORM_ERR_OK;
    }

    result = platform_log_init();
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = service_log_set_level(PROJECT_LOG_DEFAULT_LEVEL);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = service_log_enable_output(PROJECT_LOG_DEFAULT_OUTPUT_ENABLE);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    g_serviceLogInitialized = true;
    return PLATFORM_ERR_OK;
}
```

Include only:

```c
#include "service_log.h"
#include "project_log_config.h"
```

Do not call `SERVICE_LOG_I()` until after `g_serviceLogInitialized` becomes true. If adding the one-time `"log service initialized"` line, put it after the assignment and ensure Host tests account for one output call.

- [ ] **Step 6: 删除空的 Service-local 配置文件**

```powershell
git rm -- RTT_elog_DMA_UART_ring_project/02_Service/service_log/service_log_cfg.h
```

Reason: Phase 1 has only product policy, which now lives in `00_Config/project_log_config.h`; keeping an empty local cfg creates two apparent configuration sources.

- [ ] **Step 7: 运行 Service Log Host 测试**

Compile with the Task 1 command and run:

```powershell
& RTT_elog_DMA_UART_ring_project/Tests/service_log/test_service_log.exe
```

Expected: exit code `0`, no warnings.

- [ ] **Step 8: 提交 Service Log 实现**

```powershell
git add -- `
  RTT_elog_DMA_UART_ring_project/02_Service/service_log/service_log.c `
  RTT_elog_DMA_UART_ring_project/Tests/service_log/test_service_log.c `
  RTT_elog_DMA_UART_ring_project/02_Service/service_log/service_log_cfg.h
git commit -m "feat: implement Service Log policy layer"
```

---

### Task 3: 将 APP Communication 迁移到统一日志 API

**Files:**
- Modify: `RTT_elog_DMA_UART_ring_project/01_APP/app_communication.c`
- Modify: `RTT_elog_DMA_UART_ring_project/Tests/app_communication/test_app_communication.c`

**Interfaces:**
- Consumes: `SERVICE_LOG_I()`、`SERVICE_LOG_E()`，现有 `LOG_TAG "app_comm"`。
- Produces: APP Communication 不再直接依赖 `platform_log.h`。

- [ ] **Step 1: 修改 Host Test 的日志 Fake 入口**

Inspect the existing APP Communication Host test and replace any direct Platform Log include/assumption used solely by the APP source with Service Log-compatible fakes. The test build must provide the Platform Log symbols required transitively by `service_log.h`, but the production `app_communication.c` must include only:

```c
#include "service_log.h"
```

for logging.

Add an assertion that startup still emits an INFO record with tag `app_comm`, and fatal state still emits ERROR from task context.

- [ ] **Step 2: 先修改测试构建使其期待 `service_log.h`，确认生产代码仍直接依赖 Platform Log**

Run the existing APP Communication Host test compile command used by the repository.

Expected: test or dependency check fails until production include/calls are migrated.

- [ ] **Step 3: 迁移 `app_communication.c`**

Replace:

```c
#include "platform_log.h"
```

with:

```c
#include "service_log.h"
```

Replace:

```c
platform_log_i("communication runtime started");
```

with:

```c
SERVICE_LOG_I("communication runtime started");
```

Replace:

```c
platform_log_e("communication fatal error: %d", (int)communication->context.lastError);
```

with:

```c
SERVICE_LOG_E("communication fatal error: %d", (int)communication->context.lastError);
```

Do not add RX-chunk or per-byte logging.

- [ ] **Step 4: 可选增加低频恢复日志**

Only if the existing tests can deterministically cover them, add:

```c
SERVICE_LOG_W("uart error recovered");
SERVICE_LOG_W("uart data loss recovered");
```

immediately after successful recovery counters increment.

Do not log before recovery succeeds, because the message semantics would be false.

- [ ] **Step 5: 运行 APP Communication Host Tests**

Run the repository's existing APP Communication test executable.

Expected: all current behavior tests remain PASS; new logging assertions PASS; no warnings.

- [ ] **Step 6: 静态检查 APP Communication 不再直接调用 Platform Log**

```powershell
Select-String -Path RTT_elog_DMA_UART_ring_project/01_APP/app_communication.c `
  -Pattern 'platform_log_|#include "platform_log.h"'
```

Expected: no matches.

- [ ] **Step 7: 提交 APP 日志迁移**

```powershell
git add -- `
  RTT_elog_DMA_UART_ring_project/01_APP/app_communication.c `
  RTT_elog_DMA_UART_ring_project/Tests/app_communication/test_app_communication.c
git commit -m "refactor: route APP communication logs through Service Log"
```

---

### Task 4: 将系统启动日志初始化收口到 Service Log

**Files:**
- Modify: `RTT_elog_DMA_UART_ring_project/01_APP/app_system.c`
- Modify: `RTT_elog_DMA_UART_ring_project/Core/Src/freertos.c`
- Modify: `RTT_elog_DMA_UART_ring_project/Tests/app_system/test_app_system.c`

**Interfaces:**
- Consumes: `service_log_init()`、`SERVICE_LOG_I()`、现有 `app_system_init()`。
- Produces: 启动顺序 `service_log_init() -> app_system_init()`；`app_system_init()` 成功时输出一条系统装配完成日志。

- [ ] **Step 1: 扩展 APP System Host Test**

Update test fakes so `app_system.c` can include `service_log.h` without pulling a real EasyLogger backend into Host tests.

Add assertion that a successful `app_system_init()` emits exactly one low-frequency INFO record whose format string is:

```text
system composition initialized
```

Do not assert bottom-layer `init success` messages because those must not exist.

- [ ] **Step 2: 修改 `app_system.c` 成功路径日志**

Add:

```c
#include "service_log.h"
```

Define:

```c
#define LOG_TAG "app_system"
```

Immediately after:

```c
g_isInitialized = PLATFORM_TRUE;
```

add:

```c
SERVICE_LOG_I("system composition initialized");
```

Do not add separate success logs after Platform BSP construct, thread create, or UART Service init.

- [ ] **Step 3: 运行 APP System Host Tests**

Run the existing repository APP System Host test command.

Expected: PASS, no warnings.

- [ ] **Step 4: 迁移 `freertos.c` 初始化入口**

Replace USER CODE includes:

```c
#include "platform_log.h"
```

with:

```c
#include "service_log.h"
```

Remove the unused CubeMX-level `LOG_TAG` if `freertos.c` itself does not emit normal logs.

Replace:

```c
platform_error_t result = platform_log_init();
```

with:

```c
platform_error_t result = service_log_init();
```

Keep the existing failure behavior:

```c
if (PLATFORM_ERR_OK != result) {
    Error_Handler();
}
```

Do not attempt to call `SERVICE_LOG_E()` when `service_log_init()` itself fails.

- [ ] **Step 5: 静态检查启动入口不再直接依赖 Platform Log**

```powershell
Select-String -Path RTT_elog_DMA_UART_ring_project/Core/Src/freertos.c `
  -Pattern 'platform_log_|#include "platform_log.h"'
```

Expected: no matches.

- [ ] **Step 6: 提交启动链迁移**

```powershell
git add -- `
  RTT_elog_DMA_UART_ring_project/01_APP/app_system.c `
  RTT_elog_DMA_UART_ring_project/Core/Src/freertos.c `
  RTT_elog_DMA_UART_ring_project/Tests/app_system/test_app_system.c
git commit -m "refactor: initialize logging through Service Log"
```

---

### Task 5: 全仓日志边界检查与回归验证

**Files:**
- Verify only; modify only files directly required to fix violations introduced by Tasks 1-4.

**Interfaces:**
- Consumes: completed Service Log implementation and migrations.
- Produces: verified Phase 1 architecture contract.

- [ ] **Step 1: 检查 APP 层直接 Platform Log 使用**

```powershell
Get-ChildItem RTT_elog_DMA_UART_ring_project/01_APP -Filter *.c -Recurse | `
  Select-String -Pattern 'platform_log_[ewidv]|#include "platform_log.h"'
```

Expected: no matches.

- [ ] **Step 2: 检查 CubeMX FreeRTOS 启动入口**

```powershell
Select-String -Path RTT_elog_DMA_UART_ring_project/Core/Src/freertos.c `
  -Pattern 'platform_log_[ewidv]|platform_log_init|#include "platform_log.h"'
```

Expected: no matches.

- [ ] **Step 3: 检查 ISR / Callback 中没有新增 Service Log**

Search:

```powershell
Get-ChildItem RTT_elog_DMA_UART_ring_project -Filter *.c -Recurse | `
  Select-String -Pattern 'SERVICE_LOG_[EWIDV]'
```

Manually inspect every result.

Expected: calls exist only in APP / normal Service task-context code; no UART DMA IRQ, HAL callback, Platform callback or Impl ISR path contains `SERVICE_LOG_xxx()`.

- [ ] **Step 4: 运行 Service Log Host Test**

Use the Task 1 compile command and run the executable.

Expected: PASS.

- [ ] **Step 5: 运行现有相关 Host Test 回归**

Run at minimum:

```text
Tests/platform_log
Tests/service_log
Tests/service_uart
Tests/app_communication
Tests/app_system
Tests/project_config
```

Expected: all PASS with `-Wall -Wextra -Werror` where existing test scripts/commands support it.

- [ ] **Step 6: Keil 全量编译**

Build the STM32F411 target in Keil.

Expected:

```text
0 Error(s), 0 Warning(s)
```

Confirm no missing include path for:

```text
00_Config/project_log_config.h
02_Service/service_log/service_log.h
```

- [ ] **Step 7: 板级 RTT 启动验证**

Flash to board and observe RTT.

Expected startup order contains at least:

```text
log service initialized          # only if implementation kept this optional line
system composition initialized
communication runtime started
```

If the optional Service Log self-success line was omitted, the latter two are sufficient to prove the normal log chain is operational.

- [ ] **Step 8: 验证运行期 Level 与 Output Enable**

Temporarily exercise:

```c
(void)service_log_set_level(SERVICE_LOG_LEVEL_ERROR);
(void)service_log_enable_output(false);
(void)service_log_enable_output(true);
```

Expected:

- INFO is filtered at ERROR level;
- output disable suppresses normal logs;
- output enable restores output;
- no crash or task deadlock.

Remove any temporary board-only verification code before commit.

- [ ] **Step 9: 验证 UART 高频路径没有新增刷屏日志**

Send continuous UART RX data under the project's normal DMA + IDLE / HT / TC path.

Expected:

- data path continues operating;
- no per-byte/per-chunk normal log output;
- only state transition, recovery or fatal messages appear.

- [ ] **Step 10: 最终提交验证修正（仅当存在必要修正）**

If verification required source changes:

```powershell
git add -- <only-the-files-fixed-during-verification>
git commit -m "fix: close Service Log Phase 1 verification gaps"
```

If no changes were needed, do not create an empty commit.

---

## Completion Criteria

Phase 1 is complete only when all of the following are true:

```text
[ ] service_log.h/.c provide the frozen public API
[ ] project_log_config.h owns default Level / Enable policy
[ ] service_log_init() is retry-safe on failure and idempotent after success
[ ] SERVICE_LOG macros preserve LOG_TAG / file / function / line forwarding
[ ] APP Communication no longer directly uses Platform Log
[ ] freertos.c initializes Service Log rather than Platform Log
[ ] app_system emits one composition-success log rather than bottom-layer success spam
[ ] no ISR path calls ordinary Service Log
[ ] no new RingBuffer / logger task / UART DMA backend exists
[ ] Host regression tests pass
[ ] Keil builds with 0 errors / 0 warnings
[ ] board RTT verification passes
```

Do not start CmBacktrace, Flash logging, multi-backend routing, tag registry, or Platform Log public API renaming as part of this implementation.