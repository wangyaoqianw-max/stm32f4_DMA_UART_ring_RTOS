# Service Log Phase 1 Implementation Plan

> 当前执行计划 / Current Active Plan  
> 状态：READY FOR EXECUTION  
> 日期：2026-08-31

**Goal:** 建立一个薄的 Service Log 策略层，使 APP / Service 统一通过 `SERVICE_LOG_xxx()` 输出普通日志，同时复用现有 Platform Log + EasyLogger + RTT 实现。

**Architecture:**

```text
APP / Service
    ↓
service_log
    ↓
platform_log
    ↓
EasyLogger Adapter
    ↓
EasyLogger
    ↓
RTT
```

`service_log` 只负责上层统一 API、初始化编排、默认策略和 Level 映射，不重新实现日志 Core。

---

# 0. Mandatory References

执行前必须读取：

```text
00_Doc/02_架构设计/Service_Log_Phase1设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/handoff.md
03_Platform/platform_middleware/platform_log.h
04_Impl/impl_middleware/impl_log/easylogger_port.c
01_APP/app_communication.c
01_APP/app_system.c
Core/Src/freertos.c
```

Preflight 必须确认：

```text
Service Log Phase 1 Design: READ
Coding Standard: READ
Agent Execution Rules: READ
Current repository state: INSPECTED
Unrelated user changes: PRESERVED
```

如果仓库现实与冻结设计存在实质冲突：

```text
STOP / BLOCKED
```

不得静默重新设计。

---

# 1. Frozen Scope

本阶段只实现：

1. `service_log` 统一普通日志入口；
2. ERROR / WARN / INFO / DEBUG / VERBOSE 五个等级；
3. `service_log_init()`；
4. `service_log_set_level()`；
5. `service_log_enable_output()`；
6. `SERVICE_LOG_E/W/I/D/V()`；
7. 产品默认 Level / Output Enable 配置；
8. `freertos.c` 启动入口迁移；
9. APP 现有 Platform Log 调用迁移；
10. Host Test、静态依赖检查、Keil 编译和板级 RTT 验收准备。

公共 API 冻结为：

```c
typedef enum
{
    SERVICE_LOG_LEVEL_ERROR = 0,
    SERVICE_LOG_LEVEL_WARN,
    SERVICE_LOG_LEVEL_INFO,
    SERVICE_LOG_LEVEL_DEBUG,
    SERVICE_LOG_LEVEL_VERBOSE,
    SERVICE_LOG_LEVEL_MAX
} service_log_level_t;

platform_error_t service_log_init(void);
platform_error_t service_log_set_level(service_log_level_t level);
platform_error_t service_log_enable_output(platform_bool_t enable);

#define SERVICE_LOG_E(...)
#define SERVICE_LOG_W(...)
#define SERVICE_LOG_I(...)
#define SERVICE_LOG_D(...)
#define SERVICE_LOG_V(...)
```

继续使用现有 `platform_error_t`，不得新增 `service_error_t`。

---

# 2. Explicit Non-Goals

本阶段不得：

```text
- 自研日志 RingBuffer
- 新增日志后台 Task
- 新增第二套 Async Queue
- 新增 UART DMA Log Backend
- Flash 日志持久化
- 多 Backend 动态路由
- 动态 Tag Registry
- 按 Tag 独立设置日志等级
- ISR 普通格式化日志
- Crash Dump
- CmBacktrace 集成
- Diagnostic Service
- 重构现有 Platform Log 公共 API 命名
- 修改 EasyLogger Vendor 源码
- 对无关模块进行格式化或重构
```

现有 EasyLogger Assert Hook 保持不变，Fatal Diagnostics 留给后续专项设计。

---

# 3. Logging Ownership Contract

普通底层错误：

```text
Impl / Platform
    ↓
return platform_error_t
    ↓
Service / APP 获取上下文并决定处理策略
    ↓
必要时记录一次 SERVICE_LOG_W/E
```

规则：

> 同一根因只由拥有足够上下文、并决定如何处理它的最高合适层记录一次。

禁止同一错误在 Impl / Platform / Service / APP 连续重复打印。

普通日志上下文限制：

```text
Task Context: SERVICE_LOG_xxx() ALLOWED
ISR Context:  SERVICE_LOG_xxx() FORBIDDEN
```

ISR 只设置状态、计数或事件，由 Task 统一输出日志。

---

# 4. Target Files

## Create

```text
00_Config/project_log_config.h
Tests/service_log/test_service_log.c
```

## Modify

```text
02_Service/service_log/service_log.h
02_Service/service_log/service_log.c
01_APP/app_communication.c
01_APP/app_system.c
Core/Src/freertos.c
Tests/app_communication/test_app_communication.c   # only if existing tests require adaptation
Tests/app_system/test_app_system.c                 # only if existing tests require adaptation
```

## Delete

```text
02_Service/service_log/service_log_cfg.h
```

仅删除该文件的前提：确认它仍为空且没有真实引用。若仓库现实已发生变化，停止删除并报告。

---

# 5. Task 1 — Public Contract and Product Configuration

## 5.1 Write failing Host Test first

建立：

```text
Tests/service_log/test_service_log.c
```

测试首先覆盖：

```text
- service_log_level_t 存在
- SERVICE_LOG_E/W/I/D/V 可编译
- service_log_init() 可调用
- service_log_set_level() 可调用
- service_log_enable_output() 可调用
- LOG_TAG 能通过 Service Log Macro 传递到底层输出函数
- 格式参数能正确转发
```

使用 fake Platform Log，不引入新的测试框架。

先编译，确认因为 Service Log 接口尚未建立而失败。

## 5.2 Create product configuration

建立：

```text
00_Config/project_log_config.h
```

配置冻结为：

```c
#define PROJECT_LOG_DEFAULT_LEVEL          SERVICE_LOG_LEVEL_INFO
#define PROJECT_LOG_DEFAULT_OUTPUT_ENABLE  PLATFORM_TRUE
```

该文件只表达产品默认策略。

## 5.3 Implement `service_log.h`

要求：

```text
- include guard 符合规范
- 使用 platform_bool_t + platform_error_t
- 暴露 Service 自有 Level enum
- SERVICE_LOG_xxx 薄包装现有 platform_log_xxx
- 不暴露 ASSERT Level
- 不引入 HAL / CMSIS / FreeRTOS 类型
```

当前 Platform Log Macro 已携带 `LOG_TAG / __FILE__ / __FUNCTION__ / __LINE__`，Phase 1 不重复实现 variadic formatter。

## 5.4 Verify

重新编译 Host Test。

预期：公共头可通过编译；若 `.c` 尚未实现，失败只应来自控制函数实现缺失。

## 5.5 Commit

仅提交 Task 1 文件。

建议提交信息：

```text
feat: define Service Log public contract
```

---

# 6. Task 2 — Service Log Policy Implementation

## 6.1 Extend failing tests

必须覆盖以下场景：

```text
A. platform_log_init() 失败
   -> 原样返回错误
   -> Service 不进入 initialized
   -> 后续允许重试

B. default level 配置失败
   -> 原样返回错误
   -> 后续允许重试

C. default output enable 配置失败
   -> 原样返回错误
   -> 后续允许重试

D. 第一次成功初始化
   -> Platform Log 初始化一次
   -> 设置 PROJECT_LOG_DEFAULT_LEVEL
   -> 设置 PROJECT_LOG_DEFAULT_OUTPUT_ENABLE

E. 初始化成功后重复调用
   -> PLATFORM_ERR_OK
   -> 不重复初始化
   -> 不重新覆盖运行期已经修改的 level / enable

F. service_log_set_level(SERVICE_LOG_LEVEL_MAX)
   -> PLATFORM_ERR_INVALID_PARAM

G. ERROR/WARN/INFO/DEBUG/VERBOSE
   -> 正确映射到对应 PLATFORM_LOG_LEVEL_xxx

H. service_log_enable_output()
   -> 正确转发到 Platform Log
```

测试不得为了 reset 状态而给生产 API 增加 test-only 接口。可以通过独立测试进程、编译宏场景或测试源拆分解决。

## 6.2 Implement level conversion

在 `service_log.c` 内使用 file-local helper：

```c
static platform_log_level_t service_log_convert_level(service_log_level_t level);
```

映射：

```text
SERVICE ERROR   -> PLATFORM ERROR
SERVICE WARN    -> PLATFORM WARN
SERVICE INFO    -> PLATFORM INFO
SERVICE DEBUG   -> PLATFORM DEBUG
SERVICE VERBOSE -> PLATFORM VERBOSE
```

非法 Level 在公共 API 入口拒绝，不依赖 helper 的 default 分支承担参数验证。

## 6.3 Implement control APIs

```c
platform_error_t service_log_set_level(service_log_level_t level);
platform_error_t service_log_enable_output(platform_bool_t enable);
```

不要在 Service 再保存一份独立 runtime Level/Enable 真值；Platform Log / EasyLogger 保持过滤状态真值。

## 6.4 Implement idempotent initialization

初始化顺序冻结：

```text
platform_log_init()
    ↓
service_log_set_level(PROJECT_LOG_DEFAULT_LEVEL)
    ↓
service_log_enable_output(PROJECT_LOG_DEFAULT_OUTPUT_ENABLE)
    ↓
mark initialized
    ↓
SERVICE_LOG_I("log service initialized")
```

任一步失败：

```text
return error
initialized remains false
```

成功后的重复调用：

```text
return PLATFORM_ERR_OK
```

不得重新应用默认 Level / Enable。

## 6.5 Remove empty local cfg

确认：

```text
02_Service/service_log/service_log_cfg.h
```

仍为空且无引用后删除。

## 6.6 Run Host Tests

要求：

```text
-Wall -Wextra -Werror
0 warning
all Service Log scenarios PASS
```

## 6.7 Commit

建议提交信息：

```text
feat: implement Service Log policy
```

---

# 7. Task 3 — Migrate Startup and APP Call Sites

## 7.1 `Core/Src/freertos.c`

现有：

```c
platform_log_init();
app_system_init();
```

迁移为：

```c
service_log_init();
app_system_init();
```

要求：

```text
- 移除 freertos.c 对 platform_log.h 的直接依赖
- 引入 service_log.h
- 保持 Error_Handler() 错误处理不变
- 不在 CubeMX 文件中增加新的业务逻辑
```

## 7.2 `01_APP/app_communication.c`

现有 Platform Log 调用全部迁移：

```text
platform_log_i -> SERVICE_LOG_I
platform_log_e -> SERVICE_LOG_E
```

保留模块 Tag：

```c
#define LOG_TAG "app_comm"
```

至少保留：

```text
communication runtime started
communication fatal error
```

可以增加低频恢复日志：

```text
UART error recovery
DATA_LOSS recovery
```

但不得在正常 RX 高频数据路径按 chunk / byte 打日志。

## 7.3 `01_APP/app_system.c`

装配全部成功后增加一次：

```c
SERVICE_LOG_I("system composition initialized");
```

该日志即代表其前置构造 / Thread / UART Service 初始化链成功，不增加底层逐项 `init success` 日志。

为该文件定义明确 `LOG_TAG`。

## 7.4 Update affected Host Tests

仅当现有 APP Host Test 因日志依赖变化而需要 fake / include 调整时修改：

```text
Tests/app_communication/test_app_communication.c
Tests/app_system/test_app_system.c
```

不得借此重新设计 APP 测试体系。

## 7.5 Commit

建议提交信息：

```text
refactor: route application logging through Service Log
```

---

# 8. Task 4 — Verification and Design Review

## 8.1 Repository-wide static checks

搜索生产代码，确认 APP / Service 迁移边界。

除 `service_log` 本身和明确的 Platform/Impl 实现外，不应在 APP / Service 正常代码中继续存在：

```text
platform_log_e(
platform_log_w(
platform_log_i(
platform_log_d(
platform_log_v(
platform_log_init(
```

注意：

```text
03_Platform/platform_middleware/platform_log.h
04_Impl/impl_middleware/impl_log/*
Tests/platform_log/*
```

属于底层实现或专项测试，不应被误判为违规。

## 8.2 ISR check

搜索 `SERVICE_LOG_` 的所有调用点，确认不存在 UART / DMA ISR、HAL Callback 或其他中断上下文调用。

## 8.3 Run Host Tests

至少运行：

```text
Tests/service_log
Tests/app_communication
Tests/app_system
Tests/platform_log
```

并运行当前工程可用的相关回归测试。

要求：

```text
0 compile error
0 warning under existing Host Test flags
all relevant tests PASS
```

## 8.4 Keil build

如果执行环境具备 Keil：

```text
Build target
0 Error(s)
0 Warning(s)
```

如果环境没有 Keil：

```text
NOT RUN — requires local Keil verification
```

不得虚构结果。

## 8.5 Board RTT validation

如果执行环境没有真实板卡：标记为人工验收，不得声称 PASS。

人工板测至少检查：

```text
log service initialized
system composition initialized
communication runtime started
```

并验证：

```text
- 默认 INFO Level 生效
- set_level 生效
- output enable/disable 生效
- fatal APP error 仍能输出
- 正常 RX 高频路径不刷屏
```

## 8.6 Frozen-design review

完成实现后单独审查：

1. 是否引入第二套日志 Core / RingBuffer / Task；
2. 是否仍有 APP / Service 绕过 `service_log`；
3. 是否存在 ISR 普通日志；
4. 是否存在同一错误跨层重复打印；
5. 是否引入 `service_error_t`；
6. 是否改动 Platform Log / EasyLogger 接口语义；
7. 是否存在无关重构；
8. 初始化失败、重试、幂等路径是否都有测试；
9. Project Config 是否只有一个默认策略来源。

发现范围内问题：直接修复并重新验证。

发现范围外问题：记录，不擅自扩大任务。

---

# 9. Git Discipline

每个逻辑 Task 单独提交。

推荐提交：

```text
feat: define Service Log public contract
feat: implement Service Log policy
refactor: route application logging through Service Log
test: verify Service Log Phase 1 integration   # only if a separate verification commit is actually needed
```

要求：

```text
- 不提交无关文件
- 不覆盖用户未提交修改
- 不使用 force push
- 不做 unrelated cleanup
```

---

# 10. Completion Report

Codex 最终必须输出：

```text
1. 实际完成的 Task
2. 修改 / 新增 / 删除文件
3. Service Log Host Test 结果
4. APP / Platform Log 回归测试结果
5. 静态依赖检查结果
6. ISR 日志检查结果
7. Keil 编译结果（或 NOT RUN 原因）
8. 尚需人工板测项目
9. 实际提交 SHA
10. 与 Service_Log_Phase1设计.md 是否存在偏差
11. 未完成项及明确原因
```

不得用 TODO 代替未完成说明。

---

# 11. Exit Criteria

只有同时满足以下条件才可声明 Phase 1 软件实现完成：

```text
[x] service_log public contract implemented
[x] default project log policy centralized
[x] init failure/retry/idempotency tested
[x] level mapping tested
[x] output enable tested
[x] APP logging migrated
[x] freertos startup migrated
[x] no APP/Service direct Platform Log call remains outside allowed boundary
[x] no ISR Service Log call exists
[x] relevant Host Tests pass
[x] design review passes
```

Keil 与真实板 RTT 若受执行环境限制，可以作为明确列出的人工 Gate 保留，但不得报告为已验证。

---

# 12. Execution Result

```text
Status       SOFTWARE IMPLEMENTATION COMPLETE / HOST VERIFIED
Task 1 SHA   43eca5a
Task 2 SHA   377d5ab
Task 3 SHA   4fe5f42
Task 4 SHA   recorded in Git history after this verification commit
Keil         NOT RUN — UV4/UV5 unavailable in the execution environment
Board RTT    NOT YET VERIFIED — requires a real target board and RTT session
```

本次实现按用户约束将 Service Log 的布尔参数定义为 `platform_bool_t`，使用 `PLATFORM_TRUE / PLATFORM_FALSE`；该项覆盖本计划原先的 `bool` 草案。Keil 工程已登记 `service_log.c` 和其头文件目录，待具备 Keil 环境后执行真实构建。
