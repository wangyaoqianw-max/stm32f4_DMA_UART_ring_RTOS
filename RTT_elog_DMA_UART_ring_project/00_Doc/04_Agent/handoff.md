# 工程长期记忆与交接说明

更新时间：2026-08-30

> 本文件是 AI Agent 与人工开发者恢复工程上下文时的长期入口。
> 只保存长期目标、稳定架构合同、已验证能力、当前边界、技术债和下一步。
> 已完成阶段的详细设计、执行步骤、临时板测代码和历史验证过程，以专项设计文档、Tests 与 Git history 为准。

---

# 1. 项目定位与长期目标

工程根目录：

```text
RTT_elog_DMA_UART_ring_project/
```

目标环境：

```text
MCU        : STM32F411CEU6 / Cortex-M4F
Flash      : 512 KB
RAM        : 128 KB
UART       : USART1 / 115200 8N1
RTOS       : CMSIS-RTOS2 + FreeRTOS
Toolchain  : Keil MDK-ARM + STM32CubeMX
Debug Log  : EasyLogger + SEGGER RTT
```

项目最初用于学习：

```text
UART 不定长接收 + DMA + RingBuffer + FreeRTOS
```

当前同时承担两个目标：

1. 完成一条结构清晰、行为明确、可验证的嵌入式通信垂直链路；
2. 验证 Requirements -> Design -> Plan -> AI Implementation -> Test -> Handoff 的 AI 辅助嵌入式开发流程。

固定总体链路：

```text
APP
 ↓
Service
 ↓
Platform
 ↓
Impl
 ↓
Vendor / HAL / RTOS / Hardware
```

第一阶段优先级：

```text
Correctness
    > clear boundaries
    > verifiability
    > maintainability
    > performance optimization
    > abstraction elegance
```

完整链路已经跑通后，允许进行有明确收益的小范围技术债重构；仍禁止仅为理论上的“更优抽象”反复重写已验证模块。

---

# 2. 当前工程状态

当前分支：

```text
main
```

当前阶段状态：

```text
Platform BSP UART Binding Phase 1   COMPLETED
Communication UART Binding          VERIFIED
UART Service Phase 1                COMPLETED
Base RX Vertical Slice              VERIFIED
APP Phase 1 Design                  FROZEN
APP Phase 1 Implementation          COMPLETED
Production APP RX Vertical          VERIFIED
Production APP Layer                IMPLEMENTED (Phase 1)
Current State                       READY_FOR_NEXT_DESIGN
```

`01_APP/` 已存在正式生产 APP 实现：

```text
01_APP/
├── app_system.c
├── app_system.h
├── app_communication.c
└── app_communication.h
```

当前真实 RX 垂直链路：

```text
USART1
  ↓
DMA Circular + IDLE / HT / TC
  ↓
STM32 UART Impl
  ↓
Platform UART RX_DATA / ERROR / CANCELED
  ↓
UART Service
  ↓
SPSC RingBuffer
  ↓
Platform Notify From ISR
  ↓
APP Communication Task
  ↓
Application-level byte-stream handling
```

APP Phase 1 已验证：

```text
Project Config Header Host Test      PASS
APP Communication Host Test          PASS
APP System Host Test                 PASS
Dependency Boundary Scan             PASS
Keil Full Rebuild                    PASS (0 Error(s), user-verified)
Production APP Board RX Test         PASS (1280-byte 00..FF x5, user-verified)
Content Integrity Hook               PASS (compared=1280, mismatch=0, RTT evidence)
Cleanup Rebuild                      PASS (0 Error(s), user-verified)
Lower-layer Regression               PASS
Coding Standard Review               PASS
```

后续 Agent 不得再把 APP Phase 1 判断为“正在实施”或“尚未正式接入 APP”。

当前没有新的冻结专项设计和新的实施计划。
`00_Doc/04_Agent/implementation_plan.md` 当前保存的是已完成 APP Phase 1 的历史实施计划，不能直接作为下一阶段执行入口。

---

# 3. 稳定架构合同

固定依赖方向：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

通信串口当前装配规则：

```text
Communication UART logical role
        ↓
Platform BSP
        ↓
USART1
```

`platform_bsp_uart_construct_communication()` 是当前通信串口逻辑角色构造入口。
APP / Service 不得直接知道 USART1、HAL UART Handle 或 Impl 私有类型。

## 3.1 APP

负责：

- 产品级流程；
- Task lifecycle 与系统启动/关闭编排；
- Service 的使用与错误决策；
- 协议解析和业务状态机；
- 必要的静态资源 / backing storage 所有权。

不得直接依赖：

```text
STM32 HAL
DMA Handle
USART instance
CMSIS-RTOS2 / FreeRTOS concrete handle
Vendor private API
Impl private API
```

## 3.2 Service

负责：

- 数据流组织；
- 软件状态和统计；
- RingBuffer 等通用数据结构组合；
- 将 Platform 事件转换为 Task 可消费的软件语义。

不得直接访问 HAL / DMA / USART 寄存器。

## 3.3 Platform

Platform 定义“上层需要什么能力”，而不是直接镜像 STM32 HAL API。

公共 Platform Header 不暴露具体 HAL / RTOS Handle。

当前已验证的重要 Platform 能力：

```text
Platform Common
Platform UART
Platform OS
Platform Log
```

## 3.4 Impl

负责 Platform 在 STM32F411 + FreeRTOS + EasyLogger / RTT 环境上的具体实现。

可以依赖：

```text
STM32 HAL
CMSIS
FreeRTOS
CubeMX Handle
DMA / IRQ
EasyLogger
SEGGER RTT
```

## 3.5 CubeMX 生成文件

`main.c`、`freertos.c`、`usart.c`、`stm32f4xx_it.c` 等只作为初始化、Scheduler、IRQ / HAL Callback 和薄适配入口。

长期业务逻辑不得重新堆积到 CubeMX 自动生成文件；必须修改时优先限制在 `USER CODE` 区域。

---

# 4. 数据、并发与内存合同

## 4.1 DMA Buffer 与 RingBuffer

```text
DMA RX Buffer
    = hardware-facing temporary storage

RingBuffer
    = software-facing unread byte-stream storage
```

RingBuffer 不知道 UART、DMA、HAL、RTOS 或协议。

## 4.2 当前 RX 所有权

```text
platform_uart_t storage       -> APP / Caller owns
service_uart_t storage        -> APP / Caller owns
DMA RX backing storage        -> APP / Caller owns
RingBuffer backing storage    -> APP / Caller owns
Consumer Task lifecycle       -> APP owns

Active RX Session             -> UART Service owns
DMA RX Buffer writer          -> DMA / STM32 Impl only
RX_DATA event data            -> callback-period read-only view
RingBuffer Producer           -> UART Service RX callback
RingBuffer Consumer           -> dedicated APP Communication Task
```

异步数据不得依赖未声明的 Buffer 生命周期。

## 4.3 RingBuffer SPSC 并发合同

当前冻结设计针对：

```text
STM32F411 / single-core Cortex-M4
Single Producer = UART Service RX callback
Single Consumer = APP Communication Task
```

索引所有权：

```text
Producer:
    writeIndex -> unique writer
    readIndex  -> read-only snapshot

Consumer:
    readIndex  -> unique writer
    writeIndex -> read-only snapshot
```

发布顺序：

```text
Producer: copy bytes -> publish writeIndex
Consumer: copy bytes -> publish readIndex
```

`volatile` 用于当前 ISR / Task 异步可见性合同，不代表通用 Mutex / Atomic / 多核线程安全。
当前设计不宣称是跨多核、跨 Cache、跨工具链的通用 lock-free RingBuffer。

## 4.4 ISR / Task 边界

ISR / HAL Callback 只允许：

```text
capture
copy necessary bytes
update lightweight state
notify
exit quickly
```

禁止 blocking、普通 Mutex、malloc/free、协议解析、大量格式化日志和非 ISR-safe RTOS API。

Task Context 负责 RingBuffer 消费、协议解析、状态机、日志、复杂错误处理和业务逻辑。

## 4.5 内存策略

核心通信链路默认采用：

```text
Static / caller-owned allocation
```

正常收发路径不使用频繁 `malloc/free`。

---

# 5. 已验证能力基线

以下阶段均已完成，不应在无关阶段重新设计。

## 5.1 Log Phase 1 — COMPLETED

```text
APP / Service
    ↓
Platform Log API
    ↓
Impl Log Adapter
    ↓
EasyLogger / RTT
```

验证：Host Test PASS、Keil Build PASS、RTT Runtime PASS。
Platform 公共日志头文件不依赖 EasyLogger / RTT Header。

## 5.2 UART Phase 1 — COMPLETED

已验证：

```text
construct / init / start / stop / restart / deinit
blocking TX
blocking fixed-length RX
lifecycle validation
```

## 5.3 UART Phase 2A DMA RX — COMPLETED

实现：

```text
DMA Circular + IDLE / HT / TC
        ↓
Platform RX_DATA
```

冻结语义：

- Platform 不暴露 IDLE / HT / TC 来源差异；
- Wrap 最多拆为两个连续 RX_DATA 片段；
- `cancel(RX)` 产生 CANCELED；
- lifecycle stop 静默停止 RX；
- ORE -> `PLATFORM_ERR_OVERFLOW`；
- DMA / PE / NE / FE -> `PLATFORM_ERR_IO`。

真实板测已覆盖短包、Multiple Bursts、连续 640 Bytes、HT / TC / IDLE 边界、Cancel / Restart、Lifecycle Stop / Restart。

## 5.4 Platform OS Phase 1 — COMPLETED

已实现 Thread、Mutex、Semaphore、Queue、Thread Notification、Software Timer、Time / Delay。

依赖：

```text
APP / Service
    ↓
Platform OS
    ↓
Impl OS
    ↓
CMSIS-RTOS2
    ↓
FreeRTOS
```

Host、Header Isolation、Keil 和目标板 Runtime Test 均已 PASS；ISR Notify 已通过真实 USART1 RX ISR 路径验证。

## 5.5 RingBuffer Phase 1 — COMPLETED

冻结合同：

```text
SPSC byte stream
caller-owned storage
usable capacity = storageSize - 1
Partial Write on overflow
no malloc/free
no RTOS
no UART/DMA/HAL
no Mutex / Semaphore / Critical Section
```

验证：Host Test PASS、100000 deterministic stress PASS、Regression PASS、Keil Integration / Rebuild PASS。

## 5.6 UART Service Phase 1 — COMPLETED

核心能力：RX Session lifecycle、Platform UART callback ownership、RingBuffer、Platform Notify、read / wait_event、status / statistics、ERROR / CANCELED / DATA_LOSS handling。

Notification 只是 wake hint：

```text
RingBuffer readable size + Service runtime state = truth
```

真实板级验证：1280 bytes raw binary 全部 received / buffered / read，dropped = 0、mismatch = 0，结果 PASS。

## 5.7 APP Phase 1 — COMPLETED / VERIFIED

APP 已正式接管：

- `app_system`：Composition Root 与系统级对象装配；
- `app_communication`：Communication Task 与 UART Service 消费；
- 静态 UART / Service / DMA Buffer / RingBuffer Storage 生命周期；
- Scheduler 后启动 UART 与 RX Session；
- `wait_event()` + `service_uart_read()` 连续 drain；
- ERROR / DATA_LOSS / STOPPED 恢复语义；
- CubeMX `freertos.c` 保持薄适配。

生产 APP RX 垂直链路已通过 Host、Keil 和目标板数据完整性验证。

---

# 6. 当前冻结接口与行为

未经专项设计评审，不修改：

- 五层依赖方向；
- Platform Object / Device 基础模型；
- Platform Lifecycle 基本语义；
- Platform UART 现有公共语义；
- Platform UART 不暴露 HAL / DMA / RTOS Handle；
- DMA Buffer 与 RingBuffer 职责分离；
- RingBuffer SPSC / `N - 1` / Partial Write 合同；
- ISR 与 Task 职责分离；
- UART Service 单 Producer / 单 Consumer 基线；
- APP Phase 1 已验证的 RX 生命周期和恢复语义。

`platform_uart_set_callback()` 已属于当前 Platform UART 正式基线。
后续如需改变 callback ownership、RingBuffer SPSC 合同或其他公共 UART API，必须重新设计评审。

---

# 7. 可复用资产与沉淀方向

高复用候选：

```text
Platform OS
Platform Log
Platform Error / Common infrastructure
```

已验证但语义更具体：

```text
Platform UART
SPSC RingBuffer
```

当前项目组合层：

```text
UART Service
APP Communication Task
具体 Buffer 参数
具体 UART error recovery policy
Protocol / business logic
```

未来 GPIO、I2C、SPI、Watchdog、ADC / Timer / PWM 等 Platform 抽象只在出现真实设备需求时设计，不提前构建“万能 Platform”。

---

# 8. 已知技术债、限制与文档债

以下事项已经识别，但不得在无关阶段顺手扩大重构。

## 8.1 Platform Types 依赖 Impl 类型

当前存在：

```text
platform_types.h
    ↓
board_types.h
```

需要后续重新确认基础类型归属；正式迁移设计前不扩大该依赖。

## 8.2 CubeMX 遗留胶水代码

仍存在部分历史代码，例如 `USART1_mutex_Init()`、`fputc()` / UART printf 相关逻辑以及少量日志 / 默认 Task 遗留入口。

随着真实需求逐步清理，不进行无关的大规模改写。

## 8.3 service_log 占位目录

`02_Service/service_log/` 当前只是占位。
Platform Log 已可直接供 APP / Service 使用，不因为目录存在而强行增加 Service 转发层。

## 8.4 architecture.md 部分描述已过期

`architecture.md` 仍有少量旧 Log 依赖描述及旧 `docs/agent/...` 路径示例。
真实 Agent 文档位置为：

```text
00_Doc/04_Agent/
```

属于文档债，不影响当前代码基线。

## 8.5 README 占位文件较多

根目录和部分分层 README 仍为空；不阻塞开发，项目收尾或形成简历 / 示例工程时统一整理。

## 8.6 RingBuffer 并发审查结论

最近 Coding Standard Review 提出“UART callback 与 Task 并发访问 RingBuffer，索引同步与所有权契约需明确”。

当前结论：

```text
REVIEWED
NO STRUCTURAL REFACTOR REQUIRED FOR CURRENT TARGET
```

原因：冻结的 `RingBuffer_SPSC设计.md` 已明确 STM32F411 单核目标、SPSC 所有权、单写索引和“copy -> publish index”顺序；现有 Service 也严格映射为单 Producer / 单 Consumer。

后续仅在以下情况重新评审 memory ordering / barrier / atomic：

- 更换编译器或显著调整优化模型；
- 迁移到不同 CPU / Cache 架构；
- 引入多核；
- 改变 Producer / Consumer 数量。

不得为了消除该审查提示直接给当前 ISR Producer 路径加入普通 Mutex。

## 8.7 Platform Log Naming Refactor — IMPLEMENTED / HOST_VERIFIED

Platform Log 公共 API 和同模块私有适配符号已完成 V2.0 命名迁移：

```text
Public API Naming                  V2.0 COMPLIANT
Platform Log Host Regression       PASS
Platform Log Header Isolation      PASS
APP Communication Host Regression  PASS
Active-Code Old Symbol Scan        PASS
Coding Standard Review             PASS
Architecture / Runtime Behavior    UNCHANGED
```

本机未发现 Keil 可执行环境，且未连接目标板，因此以下验证尚未执行：

```text
Keil Full Rebuild                  NOT VERIFIED
RTT Runtime Regression             NOT RUN
```

在获得 Keil `0 Error(s)` 和目标板 RTT 冒烟验证前，本项不得标记为 `COMPLETED`。

## 8.8 USART1 Callback 当前为单实例路由

当前 STM32 UART Impl 使用 `g_usart1Context`，HAL RX / Error Callback 直接路由到 USART1 Context。

状态：

```text
KNOWN CURRENT LIMITATION
DEFERRED
NOT A CURRENT DEFECT
```

当前项目明确只有 `Communication UART -> USART1`，APP Phase 1 也未要求多 UART，因此现在不提前增加实例注册表或通用 Dispatcher。

当出现第二个真实 UART 角色时，再专项设计：

```text
HAL UART Handle
    ↓ lookup / registry
STM32 UART Impl Context
    ↓
platform_uart_t instance
```

优先静态注册 / 路由，不因扩展性预期引入不必要的动态分配。

---

# 9. 当前 Agent 工作规则

开始新阶段前至少读取：

```text
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/execution_rules.md
当前阶段专项设计（如存在）
当前阶段 implementation_plan（如存在）
```

如果下一阶段尚无专项设计 / implementation plan：

```text
先设计
-> 人工确认 / 冻结
-> 创建 implementation plan
-> 再执行代码
```

实施过程中：

- 不为了消除编译错误制造跨层依赖；
- 不修改与当前任务无关的模块；
- 不修改 Vendor 源码，除非计划明确要求；
- 不在 CubeMX 文件中堆积业务逻辑；
- 不擅自修改冻结公共接口；
- 新增依赖前检查方向；
- 发现真实架构冲突时 STOP / BLOCKED，记录原因并返回设计阶段。

验证结论必须区分：

```text
Host Verified
Keil Build Verified
Target Board Verified
Not Yet Verified
```

不得用静态检查代替真实 Keil / Board 证据。

---

# 10. 下一阶段入口

APP Phase 1 已完成，不再继续执行原 APP Phase 1 implementation plan。

当前状态：

```text
READY_FOR_NEXT_DESIGN
```

推荐近期顺序：

```text
Small Technical Debt Gate
    ├─ RingBuffer SPSC toolchain / memory-order contract review
    └─ Platform Log API naming refactor design
            ↓
Protocol / Application Behavior Design
```

其中：

- RingBuffer 当前不预设需要代码重构；只有复核证明现有目标工具链合同不足时才进入修改；
- Platform Log rename 属于明确的公共 API 重构，需要独立设计与实施计划；
- USART1 多实例路由继续 DEFERRED，不进入当前技术债 Gate；
- 如果决定先推进业务，也可以将 Platform Log rename 延后，但必须继续保留本文件中的已知技术债记录。

协议 / Application Behavior 阶段建议首先明确：

- UART 字节流之上的 framing / parser 边界；
- 帧长度、同步字、校验和错误重同步策略；
- Parser 位于 APP 还是独立 Service 的职责依据；
- Communication Task 与 Parser / business state machine 的调用关系；
- 输入队列、输出模型和 backpressure；
- Host 可测的软件边界；
- 当前 RX 链路哪些冻结合同不得为 Parser 便利而改变。

正式实施前必须先形成新的专项设计和新的 `implementation_plan.md`。

---

# 11. 历史追溯位置

详细历史使用：

```text
00_Doc/02_架构设计/              -> 各阶段冻结专项设计
00_Doc/04_Agent/requirements.md -> 项目需求基线
00_Doc/04_Agent/architecture.md -> 完整架构合同
Git history                     -> 实施过程、阶段提交和中间状态
Tests/                          -> Host 验证代码
```

本文件不长期保存：

- 已完成阶段的逐 Task implementation plan；
- 大段公共 API 原型；
- 历史 Scope Guard 全文；
- 临时测试代码；
- 每个 commit 的流水记录；
- 已由专项设计保存的重复合同。

原则：

> `handoff.md` 负责快速恢复“工程现在是什么、哪些不能动、下一步是什么”；
> 专项设计负责“为什么这样设计”；
> implementation plan 负责“当前阶段怎么执行”；
> Git history 负责“过去具体发生了什么”。
