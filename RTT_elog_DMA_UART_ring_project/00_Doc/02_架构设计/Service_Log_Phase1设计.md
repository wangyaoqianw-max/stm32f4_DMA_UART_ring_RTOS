# Service Log Phase 1 设计

> 文档类型：专项设计 / Frozen Design Contract  
> 状态：FROZEN  
> 版本：V1.0  
> 日期：2026-08-31

---

## 1. 背景

当前工程已经具备可工作的日志底层链路：

```text
Platform Log
    ↓
EasyLogger Adapter
    ↓
EasyLogger
    ↓
SEGGER RTT
```

现有 `03_Platform/platform_middleware/platform_log.h` 已提供：

- 日志等级抽象；
- `platform_log_init()`；
- `platform_log_set_level()`；
- `platform_log_enable_output()`；
- `platform_log_get_output_fn()`；
- `platform_log_e/w/i/d/v()` 调用宏。

现有 `04_Impl/impl_middleware/impl_log/easylogger_port.c` 已负责：

- EasyLogger 初始化；
- EasyLogger 日志等级映射；
- 输出格式配置；
- Assert Hook；
- 可选 EasyLogger Async 模式；
- RTT 最终输出。

因此 Service Log Phase 1 不重新实现日志 Core，不增加第二套 RingBuffer、日志任务或异步队列。

当前上层仍存在两个直接依赖 Platform Log 的入口：

```text
Core/Src/freertos.c
    └── platform_log_init()

01_APP/app_communication.c
    └── platform_log_i/e()
```

本阶段的核心目标是增加一个很薄的 `service_log` 策略层，把 APP 与其他 Service 的正常日志调用统一收口，同时保持现有 Platform / Impl 日志实现不变。

---

## 2. 固定依赖规则

当前架构依赖规则继续冻结为：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

日志调用规则进一步收紧为：

```text
APP / Service
    ↓
Service Log
    ↓
Platform Log
    ↓
EasyLogger Adapter
    ↓
EasyLogger
    ↓
RTT
```

正常运行日志不允许形成：

```text
APP -> Platform Log          FORBIDDEN after migration
Service -> EasyLogger        FORBIDDEN
APP -> EasyLogger            FORBIDDEN
Platform UART -> Platform Log  NOT REQUIRED
Impl UART -> Platform Log      NOT REQUIRED
```

`platform_log` 的存在表示 Platform 向上提供“日志能力抽象”，不表示 Platform 中每个模块都必须主动打日志。

---

## 3. Phase 1 目标

本阶段只完成以下目标：

1. 建立 `service_log` 作为 APP / Service 的统一普通日志入口；
2. 提供 ERROR / WARN / INFO / DEBUG / VERBOSE 五个普通日志等级；
3. 提供日志初始化、全局等级设置和总输出开关；
4. 将项目默认日志策略集中到 `00_Config/`；
5. 将 `freertos.c` 的日志初始化入口迁移到 `service_log_init()`；
6. 将 `app_communication.c` 的直接 Platform Log 调用迁移到 `SERVICE_LOG_xxx()`；
7. 在日志服务初始化完成后，APP / Service 不再直接使用 Platform Log；
8. 明确普通错误、可恢复错误和致命异常的不同诊断路径；
9. 建立 Host 编译测试、Keil 编译和板级 RTT 验收门禁。

---

## 4. Phase 1 不做什么

本阶段明确不实现：

```text
自研日志 RingBuffer
自研日志后台任务
第二套 Async Queue
UART DMA 日志 Backend
Flash 日志持久化
多 Backend 动态路由
动态 Tag 注册
按 Tag 独立设置日志等级
日志订阅 / Event Bus
日志统计服务
远程日志上传
ISR 普通格式化日志
Crash Dump
Reset Reason 管理
CmBacktrace 集成
完整 Diagnostic Service
```

EasyLogger 已经承担格式化、过滤、线程安全和可选异步输出等 Core 能力，Service Log 不重复实现。

CmBacktrace 属于后续 Fatal Diagnostics，不属于本阶段普通日志服务。

---

## 5. 总体架构

### 5.1 正常日志链路

```text
APP / Service
     │
     │ SERVICE_LOG_E/W/I/D/V
     ▼
┌──────────────────────┐
│     service_log      │
│                      │
│ 初始化策略           │
│ 默认日志等级         │
│ 输出总开关           │
│ 上层统一 API         │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│     platform_log     │
│                      │
│ 日志能力抽象         │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│ EasyLogger Adapter   │
│                      │
│ level mapping        │
│ format / async       │
└──────────┬───────────┘
           │
           ▼
      EasyLogger
           │
           ▼
          RTT
```

### 5.2 后续致命诊断链路

后续接入 CmBacktrace 后，架构目标为：

```text
Service Log -> EasyLogger ─────┐
                               ├── RTT
CmBacktrace / Fault Handler ───┘
```

两条链路可以共享 RTT 终端，但 CmBacktrace 不依赖 `service_log`、EasyLogger Async、RTOS Mutex 或普通日志任务。

---

## 6. Service Log 职责

`service_log` 是策略层 / Facade，不是新的日志框架。

职责限定为：

- 建立并初始化正常日志链；
- 应用项目默认日志等级；
- 应用项目默认输出开关；
- 为 APP / Service 提供统一调用宏；
- 转换 Service Log Level 到 Platform Log Level；
- 维持初始化幂等；
- 把 Platform Log 错误原样向上传递。

`service_log` 不负责：

- 日志字符串缓存；
- 格式化实现；
- 后端任务；
- UART / RTT 驱动；
- EasyLogger Port；
- Fault 上下文恢复。

---

## 7. 公共 API

### 7.1 日志等级

Service 层只暴露普通运行期等级：

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
```

不提供 `SERVICE_LOG_LEVEL_ASSERT`。

理由：Assert / Fault 属于不可恢复诊断，不应继续扩展普通运行日志语义。后续由 CmBacktrace / Fatal Diagnostics 独立处理。

### 7.2 控制 API

公共函数冻结为：

```c
platform_error_t service_log_init(void);
platform_error_t service_log_set_level(service_log_level_t level);
platform_error_t service_log_enable_output(bool enable);
```

继续返回工程已有 `platform_error_t`，不新增 `service_error_t`。

原因：当前 `service_uart` 等 Service 已使用 `platform_error_t` 作为跨层错误返回类型，本阶段不为日志单独建立第二套错误码体系。

### 7.3 输出宏

公共调用宏冻结为：

```c
SERVICE_LOG_E(...)
SERVICE_LOG_W(...)
SERVICE_LOG_I(...)
SERVICE_LOG_D(...)
SERVICE_LOG_V(...)
```

每个使用日志的 `.c` 文件继续定义自己的静态 `LOG_TAG`：

```c
#define LOG_TAG "app_comm"

SERVICE_LOG_I("communication runtime started");
```

Phase 1 只把 Tag 当作输出上下文，不实现 Tag Registry 或 Tag 独立等级。

### 7.4 Platform Log 兼容策略

由于现有 Platform Log 输出宏已经能够自动携带：

```text
LOG_TAG
__FILE__
__FUNCTION__
__LINE__
format + variadic arguments
```

Phase 1 的 `SERVICE_LOG_xxx()` 允许在 `service_log.h` 内部薄包装现有 `platform_log_xxx()`，避免重新实现 `printf` / `va_list` 格式化链。

该选择意味着 Service Log Phase 1 是“调用入口隔离”，而不是完全消除头文件级 Platform 依赖。

这是有意的 YAGNI 决策：当前 Platform Log 公共 API 未来仍计划单独做命名/接口重构，本阶段不同时重构底层稳定接口。

---

## 8. 初始化策略

### 8.1 启动顺序

启动顺序冻结为：

```text
MCU / HAL / RTOS 基础初始化
        ↓
service_log_init()
        ↓
platform_log_init()
        ↓
设置默认 Level / Enable
        ↓
LOG SYSTEM READY
        ↓
app_system_init()
        ↓
其他 Service / APP 正常运行
```

`service_log` 是系统最早建立的 Service 级基础设施之一。

### 8.2 初始化前行为

日志初始化前：

- 不保证普通 Service Log 可用；
- 初始化失败通过 `platform_error_t` 返回；
- 不要求日志系统使用自身报告初始化失败；
- `Error_Handler()` 或其他最底层启动失败处理不依赖普通日志链。

日志服务初始化完成后，APP / Service 统一使用 `SERVICE_LOG_xxx()`。

### 8.3 初始化幂等

`service_log_init()` 必须幂等：

```text
第一次成功初始化
    → 初始化 Platform Log
    → 应用项目默认 Level
    → 应用项目默认 Enable
    → 标记 initialized

后续重复调用
    → 直接返回 PLATFORM_ERR_OK
    → 不覆盖运行期已经修改的 Level / Enable
```

初始化过程中任何一步失败都不得把 Service Log 标记为已初始化。

### 8.4 初始化完成日志

初始化全部成功后，可以输出一次：

```text
[INFO] log service initialized
```

但初始化失败不得依赖普通日志输出，必须以错误码为真值。

---

## 9. 静态配置

项目级日志策略放入 `00_Config/`，而不是散落在 Service 实现中。

Phase 1 新增：

```text
00_Config/
└── project_log_config.h
```

冻结配置项：

```c
#define PROJECT_LOG_DEFAULT_LEVEL          SERVICE_LOG_LEVEL_INFO
#define PROJECT_LOG_DEFAULT_OUTPUT_ENABLE  true
```

`project_log_config.h` 只描述当前产品默认策略。

现有空 `02_Service/service_log/service_log_cfg.h` 在 Phase 1 不承担产品配置；实施时可以删除，避免同时存在两套配置来源。

如果以后出现只影响 Service Log 内部实现、而不属于产品策略的编译配置，再重新引入 Service-local cfg。

---

## 10. 日志所有权与错误传播

### 10.1 普通底层错误

Impl / Platform 普通代码原则上不主动打印运行日志，而是返回明确错误：

```text
HAL / Vendor Error
      ↓
Impl return error
      ↓
Platform return / map platform_error_t
      ↓
Service / APP handling policy
      ↓
必要时 SERVICE_LOG_W/E
```

### 10.2 单一记录原则

冻结规则：

> 同一错误只由“拥有足够上下文并决定如何处理该错误”的最高合适层记录一次。

禁止：

```text
Impl:     init failed
Platform: uart init failed
Service:  service init failed
APP:      system init failed
```

同一根因跨四层重复输出。

### 10.3 当前 UART 链路

当前 `service_uart` 是通用通信 Service，APP Communication 持有错误恢复策略。

因此 Phase 1 不强制给 `service_uart` 增加日志；当前更合适的记录点仍是 `app_communication`：

```text
service_uart 返回错误
        ↓
app_communication 判断：
- 可恢复？重启 RX Session
- 数据丢失？重建 Session
- 不可恢复？进入 APP ERROR
        ↓
在拥有恢复语义的位置记录一次日志
```

这样避免 Service UART 和 APP Communication 对同一个事件重复输出。

---

## 11. APP 日志迁移

### 11.1 app_communication

现有：

```c
#include "platform_log.h"

platform_log_i("communication runtime started");
platform_log_e("communication fatal error: %d", ...);
```

迁移为：

```c
#include "service_log.h"

SERVICE_LOG_I("communication runtime started");
SERVICE_LOG_E("communication fatal error: %d", ...);
```

Phase 1 可增加低频恢复日志，但禁止在 RX 高频数据路径逐包打印。

推荐仅记录：

- Communication Runtime 成功启动；
- UART Error Recovery；
- Data Loss Recovery；
- 不可恢复 Communication Error。

### 11.2 app_system

`app_system_init()` 全部装配成功后记录一次：

```text
system composition initialized
```

该日志隐含表示此前 Platform BSP、Thread、UART Service 等初始化步骤均已成功，因此不需要底层每一步重复打印 `init success`。

---

## 12. CubeMX / FreeRTOS 启动入口

当前 `MX_FREERTOS_Init()` 直接调用：

```c
platform_log_init();
app_system_init();
```

迁移后冻结为：

```c
service_log_init();
app_system_init();
```

`freertos.c` 继续保持薄启动入口：

- 不调用 EasyLogger；
- 不调用 `platform_log_*`；
- 不承担日志策略；
- 只检查返回值并进入现有 `Error_Handler()`。

---

## 13. Task / ISR 约束

普通 Service Log 只保证 Task Context 使用。

```text
Task Context:
    SERVICE_LOG_xxx()   ALLOWED

ISR Context:
    SERVICE_LOG_xxx()   FORBIDDEN
```

ISR 中发生普通事件或可恢复错误时：

```text
ISR
 ↓
记录状态 / counter / event
 ↓
通知 Task
 ↓
Task 获取完整上下文
 ↓
必要时 SERVICE_LOG_xxx()
```

理由：普通格式化日志可能涉及锁、异步缓冲、RTOS 同步和不可控执行时间，不应进入 UART / DMA 中断路径。

---

## 14. Fatal Diagnostics 边界

普通错误：

```text
return platform_error_t
```

普通运行信息与可恢复错误：

```text
SERVICE_LOG_xxx()
```

不可恢复异常：

```text
Assert / HardFault / BusFault / UsageFault
        ↓
后续 CmBacktrace / Fatal Diagnostics
```

Phase 1 保留现有 EasyLogger Assert Hook，不在本阶段同时重构 Assert 链。

后续 CmBacktrace 设计必须避免依赖：

- Service Log 初始化状态；
- EasyLogger Async Task；
- 普通 RTOS Mutex；
- 可能已经损坏的任务调度链。

---

## 15. 文件结构

实施后目标结构：

```text
00_Config/
└── project_log_config.h

02_Service/
└── service_log/
    ├── service_log.h
    └── service_log.c

Tests/
└── service_log/
    └── test_service_log.c
```

Phase 1 删除当前空的：

```text
02_Service/service_log/service_log_cfg.h
```

现有 `service_log/README.md` 可保留为空，不作为 Phase 1 的设计真值；设计真值是本文档。

---

## 16. 验收条件

### 16.1 Host 测试

必须验证：

1. `service_log_init()` 会依次初始化 Platform Log、应用默认等级和默认输出开关；
2. 任一步失败时原样返回对应 `platform_error_t`；
3. 初始化失败后允许再次初始化；
4. 初始化成功后重复调用保持幂等，不覆盖运行期配置；
5. `service_log_set_level()` 对非法 Level 返回 `PLATFORM_ERR_INVALID_PARAM`；
6. Service Level 正确映射到 Platform Level；
7. `service_log_enable_output()` 正确转发总开关；
8. `SERVICE_LOG_E/W/I/D/V()` 正确使用当前 `LOG_TAG` 并转发格式参数。

### 16.2 静态依赖检查

迁移完成后：

```text
01_APP/*.c
Core/Src/freertos.c
```

不再直接调用：

```text
platform_log_init
platform_log_e
platform_log_w
platform_log_i
platform_log_d
platform_log_v
```

`service_log.c/.h` 是上层唯一允许直接依赖 `platform_log` 的日志策略入口。

### 16.3 Keil 编译

要求：

- 0 error；
- 0 warning；
- 不引入新的 HAL / RTOS 类型到 Service Log 公共 API；
- 不修改 EasyLogger Vendor 源码。

### 16.4 板级 RTT 验证

至少观察到：

```text
log service initialized
system composition initialized
communication runtime started
```

并通过人为触发或现有错误路径验证：

- ERROR / WARN / INFO 等级过滤有效；
- 总输出开关有效；
- Communication fatal error 仍可通过统一 Service Log 输出；
- RX 正常高频路径不会逐包刷日志。

---

## 17. 参考设计取舍

本设计借鉴但不照搬以下思路：

- EasyLogger：日志 Core、等级过滤、格式和异步输出交给成熟中间件；
- `rxi/log.c`：公共日志 API 保持足够小；
- ESP-IDF Logging：Level + Tag + Macro 的调用方式；
- Zephyr Logging：Source 与 Backend 解耦；
- `logger-rb-freertos`：Logger 与 Transport 解耦，但本项目不重复实现其 RingBuffer / UART DMA Logger。

本项目 Phase 1 的最终取舍是：

```text
Service Log = 统一上层 API + 项目默认策略 + 初始化编排
Platform Log = 日志能力抽象
EasyLogger Adapter = 第三方中间件适配
EasyLogger = 日志 Core
RTT = 当前调试输出终端
```

---

## 18. 最终冻结结论

Service Log Phase 1 冻结为一个薄策略层。

它不解决“如何实现一个日志框架”，只解决：

```text
上层应该调用谁？
系统什么时候建立日志能力？
默认日志策略在哪里？
错误应该在哪一层记录？
普通日志与致命诊断如何分界？
```

实现完成后，正常日志调用边界冻结为：

```text
APP / Service -> service_log -> platform_log -> EasyLogger -> RTT
```

后续扩展必须优先保持这一上层调用契约稳定。