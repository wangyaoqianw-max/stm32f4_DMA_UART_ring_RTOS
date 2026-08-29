# 工程交接说明

更新时间：2026-08-29

## 1. 工程快照

- 工程根目录：`RTT_elog_DMA_UART_ring_project/`
- 当前分支：`main`
- 当前活动阶段：Log Phase 1 — Platform Log / EasyLogger Impl Boundary Cleanup
- 当前状态：`CODE_COMPLETE_PENDING_LOG_RUNTIME_VERIFICATION`
- MCU：STM32F411CEU6，UFQFPN48
- 软件环境：STM32 HAL、CMSIS-RTOS2 / FreeRTOS、Keil、EasyLogger、SEGGER RTT
- 总体目标：实现 UART + DMA + Ring Buffer + FreeRTOS，并通过分层架构隔离业务与硬件

固定依赖方向：

```text
APP -> Service -> Platform -> Impl -> Vendor / HAL / RTOS / Hardware
```

当前日志目标依赖：

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

---

## 2. 为什么当前切换到 Log Phase 1

UART Phase 1 的 STM32 Blocking Impl 代码已经完成并通过主机侧验证，但最近一次真实 Keil 全工程 Rebuild 失败：

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

这些 Error 全部来自旧日志接口迁移不完整，而不是 UART Impl。

同一次 Build 中 UART Impl 结果：

```text
impl_platform_uart.c: 1 warning, 0 errors
```

因此当前优先完成 Log Phase 1，恢复整个 Target 可构建状态，再返回 UART Phase 1 做板上验证。

---

## 3. UART Phase 1 暂停状态

### 已完成

- USART1 专用 Platform UART 构造 / 绑定入口。
- STM32 UART 私有 Context。
- Platform Config → HAL UART Config 转换。
- Lifecycle。
- Blocking TX / RX。
- HAL Error Mapping。
- 9-bit byte stream 不兼容问题已修复：所有 `PLATFORM_UART_DATA_BITS_9` 在当前 STM32 Impl 返回 `PLATFORM_ERR_NOT_SUPPORTED`。
- Platform UART host tests 已有 PASS 记录。
- Impl Config Mapping host test 已有 PASS 记录。
- Keil 工程已加入 Platform Common、Platform UART、STM32 UART Impl 源文件。
- `impl_platform_uart.c` 在真实 Keil Build 中为 0 Error。

### 尚未完成

- UART Phase 1 全 Target 0 Error 证据，因为当前被 Log Build Error 阻塞。
- 板上 Blocking TX Smoke Test。
- 板上固定长度 RX Smoke Test。
- Lifecycle Board Smoke Test。

### 当前规则

Log Phase 1 完成前：

- 不修改 UART Phase 1 已完成代码。
- 不进入 UART DMA / IDLE Phase 2。

---

## 4. 当前 Log 模块真实状态

### 4.1 Platform Log

文件：

```text
03_Platform/platform_middleware/platform_log.h
```

当前问题：

- 公共函数声明已经改为 `platform_error_t`。
- 仍直接 `#include "easylogger_port.h"`。
- `platform_log_e/w/i/d/v` 仍通过 `impl_elog_*` 宏调用 EasyLogger。
- 因此 Platform 间接依赖 EasyLogger、RTT、CMSIS-RTOS 和 FreeRTOS。
- 旧 `Platform_Log_Error_t` 枚举只被注释掉，没有完成 Impl 同步迁移。

这与 `architecture.md` 中已经冻结的日志依赖规则冲突。

### 4.2 Impl Log

文件：

```text
04_Impl/impl_middleware/impl_log/easylogger_port.c
04_Impl/impl_middleware/impl_log/easylogger_port.h
```

`easylogger_port.c` 当前已经负责：

- EasyLogger init/start。
- Platform Level → ELOG Level。
- EasyLogger format 设置。
- Assert Hook。
- Async Semaphore / Task。
- Async log drain。

这些职责继续保留在 Impl。

当前 13 个 Keil Error 来自它继续使用：

```text
Platform_Log_Error_t
PLATFORM_LOG_OK
PLATFORM_LOG_ERROR_RESOURCE
PLATFORM_LOG_ERROR_INIT
PLATFORM_LOG_ERROR_PARAMETER
```

### 4.3 EasyLogger Vendor Port

文件：

```text
05_Vendors/easylogger/port/elog_port.c
```

当前负责：

- EasyLogger output mutex。
- `SEGGER_RTT_Write()` 实际输出。
- RTOS Tick / HAL Tick 时间。
- FreeRTOS Task Name。

Log Phase 1 不移动、不修改该文件。

### 4.4 freertos.c

当前 USER CODE 直接：

- include `SEGGER_RTT.h`。
- 使用旧 `PLATFORM_LOG_OK`。
- Log init 失败时直接 `SEGGER_RTT_printf()`。

Log Phase 1 只允许在 USER CODE 区做最小收口，使它只依赖 Platform Log 进行正常日志使用。

---

## 5. Log Phase 1 已冻结接口设计

完整设计见：

```text
00_Doc/04_Agent/implementation_plan.md
```

当前 Plan 状态：

```text
CODE_COMPLETE_PENDING_LOG_RUNTIME_VERIFICATION
```

### 5.1 保留控制 API

```c
platform_error_t Platform_Log_Init(void);
platform_error_t Platform_Log_SetLevel(Platform_Log_Level_t level);
platform_error_t Platform_Log_EnableOutput(bool enable);
```

### 5.2 保留调用形式

```c
platform_log_e(...)
platform_log_w(...)
platform_log_i(...)
platform_log_d(...)
platform_log_v(...)
```

### 5.3 新增通用 Output Backend Contract

Platform 定义：

```c
typedef void (*platform_log_output_fn_t)(
    uint8_t level,
    const char *tag,
    const char *file,
    const char *func,
    long line,
    const char *format,
    ...);

platform_log_output_fn_t Platform_Log_GetOutputFn(void);
```

Log 宏通过该 Getter 获取当前输出函数，然后把 Level / Tag / File / Function / Line / Format 直接传递给 Backend。

### 5.4 为什么使用 Backend Getter

EasyLogger 当前公开 `elog_output(..., ...)` variadic API，但没有 `va_list` 版本。

因此：

- 不使用普通 variadic wrapper 再转发 `...`。
- 不新增 `vsnprintf + 1 KB Stack Buffer`。
- 不增加额外格式化 Mutex。

Impl 内部：

```text
Before log init success
    -> no-op output backend

After log init success
    -> EasyLogger elog_output
```

这样保持现有 EasyLogger 格式化、过滤、异步输出能力，同时清除 Platform Header 对 Vendor Header 的依赖。

---

## 6. Log Error Contract

旧 Log Error Enum 不再恢复。

统一：

```text
Success / already initialized
    -> PLATFORM_ERR_OK

Async Semaphore / Task creation failed
    -> PLATFORM_ERR_NO_RESOURCE

EasyLogger init failed
    -> PLATFORM_ERR_IO

SetLevel before init
    -> PLATFORM_ERR_NOT_INITIALIZED

SetLevel invalid level
    -> PLATFORM_ERR_INVALID_PARAM

EnableOutput before init
    -> PLATFORM_ERR_NOT_INITIALIZED
```

初始化失败时必须：

- Backend 保持 no-op。
- `s_app_log_inited = false`。
- 回收已经创建的 Async Task / Semaphore。

---

## 7. Log Phase 1 修改范围

### 允许修改

```text
03_Platform/platform_middleware/platform_log.h
04_Impl/impl_middleware/impl_log/easylogger_port.c
04_Impl/impl_middleware/impl_log/easylogger_port.h
Core/Src/freertos.c                  # USER CODE only
Tests/platform_log/*                 # 如需要新增 host test
00_Doc/04_Agent/handoff.md           # 执行后更新
```

### 禁止修改

```text
05_Vendors/easylogger/**
05_Vendors/RTT/**
04_Impl/impl_mcu/impl_platform_uart.*
03_Platform/platform_mcu/uart/**
04_Impl/impl_mcu/impl_dma.*
02_Service/service_log/**
UART Service
RingBuffer
APP communication
```

`rtt_elog_port.c/.h` 当前为空/占位，不作为本阶段核心，不要为了清理占位文件扩大范围。

---

## 8. Log Phase 1 验收重点

### Architecture

必须确认：

```text
platform_log.h
```

不再依赖：

- `easylogger_port.h`
- `elog.h`
- `SEGGER_RTT.h`
- CMSIS-RTOS Header
- FreeRTOS Header
- `impl_elog_*`

### Error

活动代码不得再存在：

```text
Platform_Log_Error_t
PLATFORM_LOG_OK
PLATFORM_LOG_ERROR_*
```

### Host / Static

推荐新增：

```text
Tests/platform_log/test_platform_log.c
```

使用 Fake Output Backend 验证 Platform 宏的：

- Level。
- Tag。
- Format。
- Vendor-free Header 编译。

### Keil

必须重新 Full Rebuild。

验收目标：

```text
0 Error(s)
```

本阶段不要求清零所有历史 Warning，但本次修改不得新增未解释 Warning。

### Runtime

硬件可用时至少验证：

- `Platform_Log_Init()`。
- `platform_log_i()` 能在 RTT Viewer 实际看到。
- `Platform_Log_SetLevel()` 基本过滤行为。
- `Platform_Log_EnableOutput(false/true)` 基本行为。

没有真实硬件证据时不得写 PASS。

---

## 9. 当前 Build Baseline

最近一次 Keil Full Rebuild 已执行，不再写“未执行”。

实际结果：

```text
14 Error(s), 14 Warning(s)
Target not created
```

主要 Error：

```text
freertos.c:
    PLATFORM_LOG_OK undefined

easylogger_port.c:
    Platform_Log_Error_t undefined
    PLATFORM_LOG_OK undefined
    PLATFORM_LOG_ERROR_RESOURCE undefined
    PLATFORM_LOG_ERROR_INIT undefined
    PLATFORM_LOG_ERROR_PARAMETER undefined
```

该结果作为 Log Phase 1 修复前 Baseline。

---

## 10. Git / Workspace 注意事项

近期 Keil 运行已经产生并提交过部分 IDE / Build Artifact 变化。

执行 Agent 仍必须首先执行：

```text
git status --short
git log --oneline -n 10
```

要求：

- 不覆盖用户未提交修改。
- 不为了本阶段顺带清理所有 Keil Artifact。
- 不把无关 IDE UI 状态变化混入 Log 业务提交。
- 如本地状态与 GitHub main 不同，以本地真实工作区为准，并记录差异。

---

## 11. 执行 Agent 必读顺序

1. `00_Doc/04_Agent/handoff.md`
2. `00_Doc/04_Agent/architecture.md`
3. `00_Doc/04_Agent/requirements.md`
4. `00_Doc/04_Agent/implementation_plan.md`
5. `03_Platform/platform_middleware/platform_log.h`
6. `04_Impl/impl_middleware/impl_log/easylogger_port.h`
7. `04_Impl/impl_middleware/impl_log/easylogger_port.c`
8. `05_Vendors/easylogger/inc/elog.h`
9. `05_Vendors/easylogger/port/elog_port.c`
10. `Core/Src/freertos.c`
11. 最新 Keil Build Log

项目自研代码默认使用中文注释。

---

## 12. 当前推荐动作

完成 Log Phase 1 的 RTT Runtime Smoke Test：

```text
Platform_Log_Init()
    ↓
platform_log_i(...)
    ↓
RTT Viewer 实际可见
```

RTT 验证通过后，标记 Log Phase 1 为 `COMPLETED`，再返回 UART Phase 1 板上 TX / RX / Lifecycle 验证。不得开始 UART DMA / IDLE Phase 2。

---

## 13. Log Phase 1 完成状态规则

满足：

```text
Architecture boundary PASS
+ Error contract PASS
+ Host/static verification PASS
+ Keil 0 Error
+ RTT runtime verification PASS
```

才可标记：

```text
COMPLETED
```

代码和 Keil Build 完成，但没有硬件 RTT 验证：

```text
CODE_COMPLETE_PENDING_LOG_RUNTIME_VERIFICATION
```

Keil Build 仍失败：

```text
BLOCKED_BY_BUILD
```

---

## 14. Log Phase 1 之后

Log Phase 1 完成后，不直接进入 DMA。

先回到 UART Phase 1：

```text
Log Phase 1 COMPLETED
        ↓
UART Phase 1 Board TX/RX/Lifecycle Test
        ↓
UART Phase 1 COMPLETED
        ↓
再由设计阶段确定 UART Phase 2 DMA + IDLE
```

UART DMA / IDLE 仍未设计，不得提前实现。

---

## 15. Log Phase 1 实施记录（2026-08-29）

- 阶段状态：`CODE_COMPLETE_PENDING_LOG_RUNTIME_VERIFICATION`

### Completed

- Platform Log 已移除对 `easylogger_port.h` 和 `impl_elog_*` 的依赖。
- 已建立 `platform_log_output_fn_t` 与 `Platform_Log_GetOutputFn()` 通用输出后端契约。
- Impl 在初始化前和失败后使用 no-op 后端；EasyLogger 初始化成功后才绑定 `elog_output`。
- 公共日志控制 API 已统一使用 `platform_error_t`，并完成既有旧错误符号的清除。
- `freertos.c` 的 USER CODE 不再包含或调用 SEGGER RTT，日志初始化改为 Platform API 调用。
- 保持 EasyLogger Async Semaphore、Async Task、Assert Hook 与格式配置在 Impl 内部。

### Changed Files

- `03_Platform/platform_middleware/platform_log.h`
- `04_Impl/impl_middleware/impl_log/easylogger_port.c`
- `04_Impl/impl_middleware/impl_log/easylogger_port.h`
- `Core/Src/freertos.c`（仅 USER CODE Includes / StartDefaultTask）
- `Tests/platform_log/test_platform_log.c`（新增 Host Test）
- 本文件

### Build Status

- 2026-08-29 使用本机 Keil MDK 5.38 执行 Full Rebuild。
- 实际 Build Log：`0 Error(s), 13 Warning(s)`，Target 已创建。
- 修复前基线为 `14 Error(s), 14 Warning(s)`；本阶段未引入新的 Warning，`freertos.c` 的旧 `LOG_TAG` 重定义 Warning 已消失。
- Keil 批处理进程退出码为 `1`，但实际构建日志明确为 `0 Error(s)`；验收以 Keil 输出日志为准。

### Test Status

- Host Test：`Tests/platform_log/test_platform_log.c` 使用 MinGW GCC 编译并运行，exit code `0`。
- 测试在不提供 EasyLogger、RTT、CMSIS-RTOS 或 FreeRTOS include 路径的条件下通过，验证 Platform Header 独立性及 `platform_log_i/e` 的 Level、Tag、Format、可变参数转发。
- 静态扫描通过：`platform_log.h` 无 EasyLogger / RTT / RTOS / `impl_elog_*` 依赖；非 Vendor 活动 C/H 无旧 `Platform_Log_Error_t`、`PLATFORM_LOG_OK`、`PLATFORM_LOG_ERROR_*` 符号；`freertos.c` 无直接 RTT 日志输出。
- RTT 硬件 Smoke Test：`NOT_VERIFIED`，当前环境未连接可验证板卡 / RTT Viewer。

### Deviations From Plan

- 无架构或接口偏差。
- 新增 Host Test 使用 Fake Output Backend，仅测试 Platform 宏与 Header 契约；EasyLogger 实际绑定由 Keil 全工程链接验证，仍需板上 RTT Smoke Test 验证运行行为。

### Known Issues

- Keil 仍有 13 个既有 Warning：`usart.h` 旧式函数声明、Platform UART 无符号比较、占位 `rtt_elog_port.c` 与 Vendor `elog_port.c` 缺少末尾换行。均不属于本阶段范围。
- Keil 启动过程中更新了 `.uvoptx/.uvguix.*` IDE 视图状态；这些非业务文件不属于本阶段修改范围，未作为实现内容处理。

### Blockers

- 无代码或构建 Blocker。
- 完成状态受硬件 RTT Runtime Smoke Test 缺失限制。

### Decisions Made During Implementation

- 采用计划指定的函数指针 Getter，而非 variadic wrapper 或额外 `vsnprintf` 缓冲区。
- 不新增动态内存、Log Service 或 Vendor 修改。
- 初始化失败时恢复 no-op 后端，并保留已有 Async Task / Semaphore 回收路径。

### Recommended Next Action

1. 连接目标板与 RTT Viewer，验证 INFO 输出、`Platform_Log_SetLevel()` 过滤及 `Platform_Log_EnableOutput(false/true)` 行为。
2. 该 RTT 验证通过后，将 Log Phase 1 标记为 `COMPLETED`。
3. 然后返回 UART Phase 1，完成板上 Blocking TX / 固定长度 RX / Lifecycle 验证；不得开始 UART Phase 2 DMA / IDLE。
