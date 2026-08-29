# Current Implementation Plan

## Metadata

- Status: READY_FOR_IMPLEMENTATION
- Phase: Log Phase 1
- Scope: Platform Log / EasyLogger Impl Boundary Cleanup
- Architecture Version: 1
- Target: STM32F411CEU6 / FreeRTOS / EasyLogger / SEGGER RTT
- Updated: 2026-08-29

---

## 1. Objective

本阶段正式收口当前日志模块的 Platform / Impl 边界，并恢复 Keil 全工程构建。

目标依赖固定为：

```text
APP / Service / Core Thin Caller
              ↓
         Platform Log
              ↓
       Impl Log Adapter
              ↓
         EasyLogger
              ↓
     EasyLogger Port / RTT
```

当前存在的错误依赖：

```text
Platform Log
    ↓ include
EasyLogger Impl Header
    ↓
EasyLogger / RTT / CMSIS-RTOS / FreeRTOS
```

必须在本阶段清除。

本阶段不是单纯把 14 个 Keil Error 改到能编译，而是建立一个稳定、可继续复用的日志抽象边界。

### 本阶段必须完成

- `platform_log.h` 移除对 `easylogger_port.h`、EasyLogger、RTT、RTOS 的直接依赖。
- Platform Log 公共错误统一使用 `platform_error_t`。
- 删除活动代码中的旧 `Platform_Log_Error_t`、`PLATFORM_LOG_OK`、`PLATFORM_LOG_ERROR_*` 语义。
- 保持 `platform_log_e/w/i/d/v` 上层调用方式。
- 建立 Platform Log 通用输出后端契约，并由 Impl 绑定 EasyLogger。
- EasyLogger 初始化、等级映射、Async Task / Semaphore、Assert Hook 留在 Impl。
- `freertos.c` 仅通过 Platform Log 使用日志，不再直接依赖 SEGGER RTT 处理日志初始化失败。
- Keil 全工程 Rebuild 恢复到 0 Error。
- 在可用硬件环境完成 RTT 日志 Smoke Test。

### 本阶段明确不实现

- Log Service。
- 日志文件持久化。
- 日志上传。
- 日志 Ring Buffer。
- 多后端动态注册。
- 运行时 Backend 切换框架。
- UART DMA / IDLE / RingBuffer / UART Service。
- `platform_types -> board_types` 技术债重构。
- UART Phase 1 代码重构。
- Vendor EasyLogger / RTT 源码重构。

---

## 2. Current State

### 2.1 Platform Log

当前：

```text
03_Platform/platform_middleware/platform_log.h
```

已经把函数声明改成 `platform_error_t`，但仍然：

- `#include "easylogger_port.h"`。
- 通过 `impl_elog_*` 宏调用 EasyLogger。
- 因此 Platform Header 间接获得 `elog.h`、SEGGER RTT、CMSIS-RTOS、FreeRTOS 依赖。
- 旧 `Platform_Log_Error_t` 枚举仅被注释，Impl 仍在使用旧符号。

### 2.2 Impl Log

当前：

```text
04_Impl/impl_middleware/impl_log/easylogger_port.c
04_Impl/impl_middleware/impl_log/easylogger_port.h
```

`easylogger_port.c` 已经承担：

- EasyLogger 初始化。
- EasyLogger Level 转换。
- Log Format 配置。
- Assert Hook。
- Async Semaphore / Task。
- Async 日志排空。

这些职责继续属于 Impl。

当前主要错误是函数实现仍使用：

```text
Platform_Log_Error_t
PLATFORM_LOG_OK
PLATFORM_LOG_ERROR_RESOURCE
PLATFORM_LOG_ERROR_INIT
PLATFORM_LOG_ERROR_PARAMETER
```

而 Platform 已不再定义这些符号。

### 2.3 EasyLogger Vendor Port

当前：

```text
05_Vendors/easylogger/port/elog_port.c
```

负责：

- EasyLogger output mutex。
- RTT 实际输出。
- 时间 / Task Name 获取。

本阶段不移动、不重写该 Vendor Port。

### 2.4 Current Keil Baseline

最新全工程 Rebuild 已真实执行：

```text
14 Error(s), 14 Warning(s)
Target not created
```

错误分布：

```text
Core/Src/freertos.c                         1 Error
04_Impl/impl_middleware/impl_log/
    easylogger_port.c                      13 Errors
```

这些 Error 均来自 Log API 半迁移状态。

UART Impl 在同一次 Keil Build 中：

```text
impl_platform_uart.c: 1 warning, 0 errors
```

因此禁止为了修 Log Build 修改 UART Impl。

---

## 3. Frozen Architecture Decisions

### 3.1 Dependency Direction

固定：

```text
APP / Service
      ↓
Platform Log
      ↓
Impl Log
      ↓
Vendor / RTT / RTOS
```

禁止：

```text
Platform Log -> easylogger_port.h
Platform Log -> elog.h
Platform Log -> SEGGER_RTT.h
Platform Log -> cmsis_os.h
Platform Log -> task.h
APP / Service -> EasyLogger API
```

### 3.2 No Log Service In Phase 1

当前日志只是横切基础能力。

`02_Service/service_log/` 不在本阶段启用。

只有未来出现日志缓存、持久化、上传、远程管理等服务级职责时，再重新评审是否建立 Log Service。

### 3.3 Vendor Boundary

本阶段不修改：

```text
05_Vendors/easylogger/**
05_Vendors/RTT/**
```

当前 EasyLogger Port 继续作为 Vendor 的平台适配入口。

---

## 4. Platform Log Public Interface

保留现有公共控制 API：

```c
platform_error_t Platform_Log_Init(void);
platform_error_t Platform_Log_SetLevel(Platform_Log_Level_t level);
platform_error_t Platform_Log_EnableOutput(bool enable);
```

保留现有上层日志调用形式：

```c
platform_log_e(...)
platform_log_w(...)
platform_log_i(...)
platform_log_d(...)
platform_log_v(...)
```

不要求调用方知道 EasyLogger。

---

## 5. Generic Output Backend Contract

### 5.1 Why This Contract Exists

EasyLogger 当前公开：

```c
void elog_output(uint8_t level,
                 const char *tag,
                 const char *file,
                 const char *func,
                 long line,
                 const char *format,
                 ...);
```

但没有公开 `va_list` 版本。

因此不采用：

```text
Platform_Log_Output(...)
    ↓ wrapper
elog_output(...)
```

这种普通 variadic wrapper，因为 C 中不能直接把 `...` 安全重新转发给另一个 variadic 函数。

也不采用额外 `vsnprintf + 1 KB Stack Buffer` 方案，避免增加 Task Stack 压力；当前默认 Task Stack 仅 512 bytes。

### 5.2 Platform Generic Output Function Type

在 `platform_log.h` 中定义与具体 Vendor 无关的输出函数类型：

```c
typedef void (*platform_log_output_fn_t)(
    uint8_t level,
    const char *tag,
    const char *file,
    const char *func,
    long line,
    const char *format,
    ...);
```

Platform 只描述通用日志元数据：

- Level。
- Tag。
- File。
- Function。
- Line。
- printf-style Format。

不得出现 EasyLogger 类型。

### 5.3 Backend Getter

Platform 声明：

```c
platform_log_output_fn_t Platform_Log_GetOutputFn(void);
```

说明：

- 该函数是 Platform Log 宏使用的内部公共契约。
- 上层业务通常不直接调用。
- Backend 具体指向谁由 Impl 决定。
- 不暴露可由上层随意改写的全局函数指针。

### 5.4 Log Macros

`platform_log_e/w/i/d/v` 改为通过 `Platform_Log_GetOutputFn()` 调用通用输出后端。

语义示例：

```c
#define platform_log_i(...) \
    Platform_Log_GetOutputFn()( \
        (uint8_t)PLATFORM_LOG_LEVEL_INFO, \
        LOG_TAG, \
        __FILE__, \
        __FUNCTION__, \
        (long)__LINE__, \
        __VA_ARGS__)
```

其他 Level 同理。

要求：

- 保留现有 `LOG_TAG` 使用方式。
- Platform Header 不再 include `elog.h`。
- Platform Header 不再依赖 `impl_elog_*`。
- 不增加动态内存。

---

## 6. Impl Backend Binding

在 `easylogger_port.c` 内：

### 6.1 No-op Backend

实现一个静态 no-op variadic output：

```text
Impl_Elog_NoOutput(...)
```

初始化前 Backend 指向 no-op，避免未初始化时上层日志调用进入 Vendor。

### 6.2 Active Backend

维护 Impl 私有：

```c
static platform_log_output_fn_t s_log_output_fn;
```

状态：

```text
Before Platform_Log_Init success
    -> Impl_Elog_NoOutput

After Platform_Log_Init success
    -> elog_output
```

`Platform_Log_GetOutputFn()` 只返回当前函数指针。

上层不能修改该绑定。

### 6.3 Why Direct Binding Is Allowed

`easylogger_port.c` 位于 Impl，可以直接依赖 EasyLogger。

绑定发生在：

```text
Impl -> Vendor
```

而不是：

```text
Platform Header -> Vendor Header
```

因此符合架构依赖方向。

---

## 7. Error Semantics

公共接口统一返回 `platform_error_t`。

### 7.1 Platform_Log_Init

```text
already initialized
    -> PLATFORM_ERR_OK

Semaphore creation failed
    -> PLATFORM_ERR_NO_RESOURCE

Async Task creation failed
    -> PLATFORM_ERR_NO_RESOURCE

elog_init failed
    -> PLATFORM_ERR_IO

success
    -> PLATFORM_ERR_OK
```

初始化失败时：

- Backend 保持 no-op。
- `s_app_log_inited = false`。
- 已创建的 Async Task / Semaphore 必须回收。
- 不留下“部分初始化成功”的 Platform 状态。

### 7.2 Platform_Log_SetLevel

```text
not initialized
    -> PLATFORM_ERR_NOT_INITIALIZED

invalid level
    -> PLATFORM_ERR_INVALID_PARAM

success
    -> PLATFORM_ERR_OK
```

### 7.3 Platform_Log_EnableOutput

```text
not initialized
    -> PLATFORM_ERR_NOT_INITIALIZED

success
    -> PLATFORM_ERR_OK
```

不得重新引入单独的 Log Error Enum。

---

## 8. Impl Header Policy

`easylogger_port.h` 只允许作为 Impl 私有 Header。

本阶段要求：

- Platform 不再 include 它。
- 移除或停止使用 `impl_elog_e/w/i/d/v` 这组向上泄漏的宏。
- Vendor / RTOS Header 可以继续存在于 Impl 私有区域。

执行 Agent 应先做引用扫描。

如果确认 `easylogger_port.h` 仅被 `easylogger_port.c` 自身使用，可选择：

### Preferred

保留文件，但将其收缩为 Impl 私有依赖 Header，避免本阶段额外做文件删除和 Keil 配置清理。

不要因为“可以删”扩大任务。

---

## 9. RTT / EasyLogger Responsibilities

本阶段维持当前运行关系：

```text
Platform Log API
      ↓
Impl EasyLogger Adapter
      ↓
EasyLogger Core
      ↓
EasyLogger Vendor Port
      ↓
SEGGER RTT
```

其中：

### Platform

负责：

- 公共 Level。
- 公共 Init / SetLevel / EnableOutput。
- 通用 Output Backend Contract。
- 上层日志宏。

### Impl

负责：

- Platform Level → ELOG Level。
- EasyLogger init/start。
- Async Task / Semaphore。
- Assert Hook。
- Format 设置。
- Platform Error 映射。
- Backend 绑定。

### Vendor Port

继续负责：

- RTT 实际输出。
- EasyLogger output lock。
- time / thread info。

本阶段不移动这些职责。

---

## 10. Core / FreeRTOS Boundary

允许修改：

```text
Core/Src/freertos.c
```

但仅限 `USER CODE` 区域。

目标：

- 删除业务侧对 `SEGGER_RTT.h` 的直接 include。
- 删除 `SEGGER_RTT_printf()` 作为日志初始化失败处理。
- 使用 `PLATFORM_ERR_OK` 判断 `Platform_Log_Init()`。
- 只有 Platform Log 初始化成功时才执行测试日志输出，或允许 Backend no-op 安全吸收未初始化日志。
- 不把更多日志业务堆进 `freertos.c`。

保留现有 UART Mutex 等与本阶段无关逻辑，不顺带重构。

---

## 11. Files In Scope

### Modify

```text
03_Platform/platform_middleware/platform_log.h
04_Impl/impl_middleware/impl_log/easylogger_port.c
04_Impl/impl_middleware/impl_log/easylogger_port.h
Core/Src/freertos.c                 # USER CODE only
00_Doc/04_Agent/handoff.md          # execution result
```

### Create If Needed

推荐新增最小 Host Test：

```text
Tests/platform_log/test_platform_log.c
```

用于验证：

- `platform_log.h` 不依赖 EasyLogger。
- Macro Level / Tag / Format 能到达 Fake Backend。
- 不需要真实 RTT / FreeRTOS / EasyLogger。

### Do Not Modify

```text
05_Vendors/easylogger/**
05_Vendors/RTT/**
04_Impl/impl_mcu/impl_platform_uart.*
03_Platform/platform_mcu/uart/**
04_Impl/impl_mcu/impl_dma.*
02_Service/service_log/**
UART Service / RingBuffer / APP
```

`rtt_elog_port.c/.h` 当前为空/占位，不是 Phase 1 设计核心；不要为了清理占位文件扩大任务。

---

## 12. Implementation Steps

### Step 1 — Preflight

执行：

```text
git status --short
git log --oneline -n 10
```

确认：

- 保留用户未提交改动。
- 不提交 Keil 生成物作为本阶段业务修改，除非用户明确要求。
- 读取最新 Keil Build Log。
- 确认 Baseline 为 14 Errors / 14 Warnings。

### Step 2 — Decouple Platform Header

修改 `platform_log.h`：

- 删除 `easylogger_port.h` include。
- 删除旧错误枚举注释残留。
- 保留 Platform Level。
- 定义 `platform_log_output_fn_t`。
- 声明 `Platform_Log_GetOutputFn()`。
- 重写 `platform_log_e/w/i/d/v` 宏。
- 更新 API 注释为 `platform_error_t` 语义。

### Step 3 — Update Impl Error Contract

修改 `easylogger_port.c`：

- 三个 `Platform_Log_*` 实现全部改为 `platform_error_t`。
- 按第 7 章执行 Error Mapping。
- 清除所有活动的旧 Log Error Symbol。

### Step 4 — Bind Output Backend

在 Impl：

- 添加 no-op output。
- 添加私有 output function pointer。
- 实现 `Platform_Log_GetOutputFn()`。
- Init 成功后绑定 `elog_output`。
- Init 失败保持 no-op。

不得在 Platform 中 include Vendor Header。

### Step 5 — Keep Async EasyLogger Behavior

保持现有：

- Async Semaphore。
- Async Task。
- `elog_async_output_notice()`。
- `Impl_Elog_AsyncTask()`。
- Assert Hook。
- Format 配置。

仅修改为新 Error 契约所需内容。

### Step 6 — Clean Impl Header Boundary

修改 `easylogger_port.h`：

- 删除 `impl_elog_e/w/i/d/v` 对上层无意义宏。
- 保持其为 Impl 私有。
- 不让 Platform 再 include。

### Step 7 — Update freertos.c

仅 USER CODE：

- 去掉直接 RTT 日志依赖。
- `PLATFORM_ERR_OK` 判断初始化结果。
- 保持最小测试日志。
- 不新增业务逻辑。

### Step 8 — Host / Static Verification

至少执行：

- Platform Log Header 独立编译测试。
- Platform Log Vendor dependency scan。
- 旧 Log Error Symbol scan。
- 如新增 `Tests/platform_log/test_platform_log.c`，执行并要求 exit code 0。

### Step 9 — Keil Full Rebuild

执行完整 Target Rebuild。

必须读取实际 Build Log。

验收：

```text
0 Error(s)
```

Warning：

- 本阶段不要求顺带清零历史 Warning。
- 本次修改不得新增未解释 Warning。
- 最终 Warning 数量和来源记录到 handoff。

### Step 10 — RTT Runtime Smoke Test

硬件可用时验证：

```text
Platform_Log_Init()
    ↓
platform_log_i("...")
    ↓
Impl
    ↓
EasyLogger
    ↓
RTT Viewer 实际可见
```

至少验证：

- INFO 输出。
- `Platform_Log_SetLevel()` 基本过滤行为。
- `Platform_Log_EnableOutput(false/true)` 基本行为。

如硬件不可访问，不得伪造 PASS。

### Step 11 — Update Handoff

记录：

- 修改文件。
- API 最终形态。
- Dependency scan。
- Host test。
- Keil Error / Warning 数量。
- RTT Smoke Test。
- 偏差 / Blocker。
- UART Phase 1 后续状态。

---

## 13. Verification Checklist

### Architecture

- [ ] `platform_log.h` 不 include `easylogger_port.h`。
- [ ] `platform_log.h` 不 include `elog.h`。
- [ ] `platform_log.h` 不 include `SEGGER_RTT.h`。
- [ ] `platform_log.h` 不 include CMSIS-RTOS / FreeRTOS Header。
- [ ] `freertos.c` 不通过 RTT 直接承担正常日志输出。
- [ ] Platform 不出现 `impl_elog_*`。

### Error Contract

- [ ] `Platform_Log_Init()` 返回 `platform_error_t`。
- [ ] `Platform_Log_SetLevel()` 返回 `platform_error_t`。
- [ ] `Platform_Log_EnableOutput()` 返回 `platform_error_t`。
- [ ] 活动代码无 `Platform_Log_Error_t`。
- [ ] 活动代码无 `PLATFORM_LOG_OK` / `PLATFORM_LOG_ERROR_*`。

### Backend

- [ ] Init 前 Output Backend 为 no-op。
- [ ] Init 成功后 Backend 为 EasyLogger output。
- [ ] Init 失败后 Backend 仍为 no-op。
- [ ] 不需要额外 1 KB Task Stack Buffer。
- [ ] 不需要动态内存。

### Build

- [ ] Platform Log host/static check PASS。
- [ ] Keil full target 0 Error。
- [ ] Build Log 已记录 Warning 数量。

### Runtime

- [ ] RTT INFO Log 可见，或明确记录硬件未验证。
- [ ] Level Set 行为验证，或明确记录硬件未验证。
- [ ] EnableOutput 行为验证，或明确记录硬件未验证。

---

## 14. Completion Criteria

Log Phase 1 只有满足以下条件才能标记 `COMPLETED`：

1. Platform Log 不再依赖 Impl / Vendor Header。
2. 公共错误统一为 `platform_error_t`。
3. 通用 Output Backend Contract 已实现。
4. EasyLogger 绑定仅存在于 Impl。
5. Async EasyLogger 原有能力未被破坏。
6. `freertos.c` 不再使用旧 `PLATFORM_LOG_*` Error Symbol。
7. Keil 全工程构建达到 0 Error。
8. RTT Runtime Smoke Test 有真实证据。
9. Handoff 已更新为实际状态。

如果代码与 Keil 构建通过，但缺少硬件 RTT 验证：

```text
CODE_COMPLETE_PENDING_LOG_RUNTIME_VERIFICATION
```

如果 Keil 仍失败：

```text
BLOCKED_BY_BUILD
```

不得在 Build 失败时标记 `COMPLETED`。

---

## 15. After Log Phase 1

Log Phase 1 完成后，不直接进入 UART DMA Phase 2。

先返回 UART Phase 1 完成剩余硬件验证：

```text
Log Phase 1
    ↓
Keil Target build restored
    ↓
UART Phase 1 Board TX/RX/Lifecycle Smoke Test
    ↓
UART Phase 1 COMPLETED
    ↓
再设计 UART Phase 2 DMA + IDLE
```

未经重新设计，不得提前实现 DMA / IDLE。
