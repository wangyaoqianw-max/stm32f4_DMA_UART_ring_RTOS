# 工程长期记忆与交接说明

更新时间：2026-08-30

> 本文件是 AI Agent 与人工开发者恢复工程上下文时的长期入口。
> 只保存长期目标、稳定架构合同、已验证能力、当前边界、技术债和下一步。
> 已完成阶段的详细设计、执行步骤、临时板测代码、提交过程和历史验证细节，以专项设计文档与 Git history 为准，不在本文件重复维护。

---

# 1. 项目定位与长期目标

工程根目录：

```text
RTT_elog_DMA_UART_ring_project/
```

目标硬件与环境：

```text
MCU        : STM32F411CEU6 / Cortex-M4F
Flash      : 512 KB
RAM        : 128 KB
UART       : USART1 / 115200 8N1
RTOS       : CMSIS-RTOS2 + FreeRTOS
Toolchain  : Keil MDK-ARM + STM32CubeMX
Debug Log  : EasyLogger + SEGGER RTT
```

本项目最初用于学习：

```text
UART 不定长接收 + DMA + RingBuffer + FreeRTOS
```

当前目标已经扩展为两个并行目标。

## 1.1 Firmware 目标

先完成一条结构清晰、行为明确、可验证的完整垂直链路：

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

重点练习：

- UART DMA 连续字节流接收；
- ISR / Task 协作；
- SPSC RingBuffer；
- Buffer ownership；
- 生命周期和错误状态；
- RTOS 与日志抽象；
- 模块职责和依赖边界；
- Host / Keil / Board 分层验证。

第一阶段优先级：

```text
Correctness
    > clear boundaries
    > verifiability
    > maintainability
    > performance optimization
    > abstraction elegance
```

完整基础链路跑通前，不因为理论上的“更优设计”反复重构已经验证的模块。

## 1.2 AI 开发工作流目标

本项目也是第一次系统验证 AI 辅助嵌入式软件开发工作流。

默认流程：

```text
Requirements
    ↓
Architecture Contract
    ↓
Phase Design
    ↓
Implementation Plan
    ↓
AI Implementation
    ↓
Host Test / Regression
    ↓
Keil Build
    ↓
Target Board Test
    ↓
Handoff
    ↓
Next Phase
```

原则：

- AI 不从模糊需求直接跳到生产代码；
- 新子系统先设计、冻结边界，再实施；
- 每次实施控制 Scope，不顺手扩大重构；
- Host Test 能自动验证的软件逻辑优先自动验证；
- Keil 工程集成与真实硬件行为必须有真实证据；
- 设计模型与执行模型可以分离，但交接必须依赖仓库文档，而不是依赖聊天上下文；
- 基础项目完成后，再单独 Review AI 工作流中哪些步骤过重、哪些文档可以压缩、哪些任务适合较低成本模型执行。

---

# 2. 当前工程状态

当前分支：

```text
main
```

最后已验证的 Firmware 功能基线：

```text
ba1d877  docs: complete uart service phase1 verification
```

该 SHA 仅表示最近一次完成 Host / Keil / Board 验证的功能基线；后续纯文档提交不会改变该验证结论，也不要求本文件记录实时 HEAD。

当前状态：

```text
UART Service Phase 1       COMPLETED
Base RX Vertical Slice     VERIFIED
Production APP Layer       NOT IMPLEMENTED
Next Phase                 APP Phase 1 Design
Next State                 READY_FOR_APP_DESIGN
```

当前真实 RX 链路已经完成到 Task Context：

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
Dedicated Consumer Task
```

`01_APP/` 当前仍没有正式生产 APP 实现。

当前 `00_Doc/04_Agent/implementation_plan.md` 属于已完成的 UART Service Phase 1 实施计划。
在新的 APP 专项设计完成前，不得继续把该旧计划当作当前执行计划。

---

# 3. 稳定架构合同

固定依赖方向：

```text
APP -> Service -> Platform -> Impl -> Vendor / HAL / RTOS / Hardware
```

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
```

## 3.2 Service

负责：

- 数据流组织；
- 软件状态和统计；
- RingBuffer 等通用数据结构组合；
- 将 Platform 事件转换为任务可消费的软件语义。

不得直接访问 HAL / DMA / USART 寄存器。

## 3.3 Platform

定义：

> 上层需要什么能力。

而不是：

> STM32 HAL 提供了什么函数。

公共 Platform Header 不暴露具体 HAL / RTOS Handle。

当前已验证的重要 Platform 能力：

```text
Platform Common
Platform UART
Platform OS
Platform Log
```

## 3.4 Impl

负责 Platform 在当前 STM32F411 + FreeRTOS + EasyLogger / RTT 环境上的实际实现。

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

`main.c`、`freertos.c`、`usart.c`、`stm32f4xx_it.c` 等只作为：

- 初始化入口；
- Scheduler 入口；
- IRQ / HAL Callback 入口；
- 自定义架构的薄适配入口。

长期业务逻辑不得重新堆积到 CubeMX 自动生成文件。
必须修改时优先限制在 `USER CODE` 区域。

---

# 4. 数据、并发与内存合同

## 4.1 DMA Buffer 与 RingBuffer 分离

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
RingBuffer Consumer           -> dedicated Communication Task
```

异步数据不得依赖未声明的 Buffer 生命周期。

## 4.3 ISR / Task 边界

ISR / HAL Callback：

```text
capture
copy necessary bytes
update lightweight state
notify
exit quickly
```

禁止：

- blocking；
- ordinary Mutex；
- malloc/free；
- protocol parsing；
- 大量格式化日志；
- 非 ISR-safe RTOS API。

Task Context 负责：

- RingBuffer 消费；
- 协议解析；
- 状态机；
- 日志；
- 复杂错误处理和业务逻辑。

## 4.4 内存策略

核心通信链路默认采用：

```text
Static / caller-owned allocation
```

正常收发路径不使用频繁 `malloc/free`。

## 4.5 数据模型

新增状态型模块时优先区分：

```text
Static Configuration
Runtime Context
Runtime Data / Statistics
```

这是一种设计方法，不要求所有模块机械复制完全相同的结构体模板。

---

# 5. 已验证能力基线

以下阶段均已完成，不应在无关阶段重新设计。

## 5.1 Log Phase 1 — COMPLETED

当前链路：

```text
APP / Service
    ↓
Platform Log API
    ↓
Impl Log Adapter
    ↓
EasyLogger / RTT
```

已确认：

```text
Host Test          PASS
Keil Build         PASS
RTT Runtime Test   PASS
```

Platform 公共日志头文件不依赖 EasyLogger / RTT Header。
日志具体实现位于 Impl middleware。

## 5.2 UART Phase 1 — COMPLETED

已确认：

```text
construct / init / start / stop / restart / deinit
blocking TX
blocking fixed-length RX
lifecycle validation
Keil Rebuild PASS
```

## 5.3 UART Phase 2A DMA RX — COMPLETED

实现：

```text
DMA Circular + IDLE / HT / TC
        ↓
Platform RX_DATA
```

关键语义：

- Platform 不暴露 IDLE / HT / TC 来源差异；
- Wrap 最多拆为两个连续 RX_DATA 片段；
- `cancel(RX)` 产生 CANCELED；
- lifecycle stop 静默停止 RX；
- ORE -> `PLATFORM_ERR_OVERFLOW`；
- DMA / PE / NE / FE -> `PLATFORM_ERR_IO`。

真实板测包括：

```text
Short + IDLE                         PASS
Multiple Bursts                     PASS
Continuous 640 Bytes                PASS
HT / TC / IDLE Boundary 300 Bytes   PASS
Cancel / Restart                    PASS
Lifecycle Stop / Restart            PASS
```

## 5.4 Platform OS Phase 1 — COMPLETED

已实现：

```text
Thread
Mutex
Semaphore
Queue
Thread Notification
Software Timer
Time / Delay
```

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

Host、Header Isolation、Keil、目标板 Runtime Test 均已 PASS。
`NOTIFY ISR` 已通过真实 USART1 RX ISR 路径验证。

## 5.5 RingBuffer Phase 1 — COMPLETED

冻结合同：

```text
SPSC byte stream
caller-owned storage
no malloc/free
no RTOS
no UART/DMA/HAL
no Mutex / Semaphore / Critical Section
```

容量：

```text
storageSize = N
usable capacity = N - 1
```

Overflow：

```text
Partial Write
保留旧数据
尽量保存新数据前缀
不静默覆盖
未完整写入 -> PLATFORM_ERR_OVERFLOW
```

验证：

```text
Host Test                         PASS
100000 deterministic stress      PASS
Regression                       PASS
Keil Integration / Rebuild       PASS
```

## 5.6 UART Service Phase 1 — COMPLETED

核心能力：

```text
RX Session lifecycle
Platform UART callback ownership
RingBuffer integration
Platform Notify wakeup
read / wait_event
status / statistics
ERROR / CANCELED / DATA_LOSS handling
```

Service Event：

```text
RX_AVAILABLE
DATA_LOSS
ERROR
STOPPED
```

Notification 只是 wake hint：

```text
RingBuffer readable size + Service runtime state = truth
```

真实板级验证：

```text
Input      : 1280 bytes raw binary, 00..FF repeated
received   : 1280
buffered   : 1280
read_total : 1280
dropped    : 0
mismatch   : 0
high_water : 128
ERROR      : not observed
DATA_LOSS  : not observed
```

完整链路：

```text
USART1 RX DMA
-> Platform callback
-> UART Service
-> RingBuffer
-> Consumer Task
-> RTT observation
```

结果：`PASS`。

临时板测代码已从 `Core/Src/freertos.c` 恢复，恢复后 Keil Rebuild `0 Error(s)`。

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
- UART Service 单 Producer / 单 Consumer 基线。

UART Service Phase 1 已正式纳入的 Platform UART API：

```c
platform_uart_set_callback();
```

它已不再是“待实现例外”，而是当前 Platform UART 基线的一部分。
后续如需改变 callback ownership 或其他 UART 公共接口，必须重新设计评审。

---

# 7. 可复用资产与后续沉淀方向

本项目除了完成 UART 功能，也用于识别哪些基础设施值得跨项目复用。

## 7.1 高复用候选

```text
Platform OS
Platform Log
Platform Error / Common infrastructure
```

其中：

- Platform OS 已覆盖常用 RTOS 能力，并隔离 CMSIS / FreeRTOS concrete type；
- Platform Log 已形成 Platform API -> Impl Adapter -> EasyLogger / RTT 的边界；
- Error / 基础类型等适合作为后续公共基础设施，但基础类型归属仍有技术债需要处理。

## 7.2 已验证但语义更具体

```text
Platform UART
SPSC RingBuffer
```

可复用，但必须保持其已声明的模型边界，不能宣称为所有 UART / 所有并发模型的万能抽象。

## 7.3 当前项目组合层

```text
UART Service
Communication Task
具体 Buffer 参数
具体 UART error recovery policy
APP protocol / business logic
```

这些应首先服务于真实项目需求，不急于抽取成通用框架。

## 7.4 后续外设 Platform 原则

未来可根据真实项目逐步验证：

```text
GPIO
I2C
SPI
Watchdog
ADC / Timer / PWM ...
```

但不提前一次性编写“万能 Platform”。
只有在真实设备 / 第二个项目中出现需求时再设计，并通过第二次实际复用检查抽象是否合理。

---

# 8. 已知技术债与文档债

以下问题已识别，不允许在无关阶段顺手大规模重构。

## 8.1 Platform Types 依赖 Impl 类型

当前存在：

```text
platform_types.h
    ↓
board_types.h
```

需要后续重新确认基础类型归属。
在正式迁移设计前不扩大该依赖。

## 8.2 CubeMX 遗留胶水代码

当前 CubeMX 文件仍存在部分历史代码，例如：

- `USART1_mutex_Init()`；
- `fputc()` / UART printf 相关逻辑；
- 部分日志初始化或默认 Task 遗留入口。

随着 APP / Service 正式接入逐步迁移，不进行无关的大规模清理。

## 8.3 service_log 占位目录

```text
02_Service/service_log/
```

当前只是占位。
Platform Log 已可以直接供 APP / Service 使用，不因为目录存在而强行增加无意义的 Service 转发层。

## 8.4 architecture.md 部分描述已过期

当前代码已完成 Log Phase 1 解耦，但 `architecture.md` 中仍保留早期：

```text
Platform Log -> EasyLogger Header
```

作为技术债的描述，该段已与当前代码状态不一致。

此外 `architecture.md` Agent 路径示例仍使用旧的：

```text
docs/agent/...
```

当前真实位置为：

```text
00_Doc/04_Agent/
```

这些属于文档债；后续专门整理架构文档时修正，不影响当前代码基线。

## 8.5 README 占位文件较多

根目录及多个分层目录 README 仍为空文件。
这不阻塞当前开发，项目收尾或形成简历 / 示例工程时再统一补齐。

---

# 9. 当前 Agent 工作规则

开始新的阶段前至少读取：

```text
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/execution_rules.md
当前阶段专项设计
当前阶段 implementation_plan
```

若当前阶段尚无专项设计 / implementation plan：

```text
先设计
-> 人工确认 / 冻结
-> 再创建 implementation plan
-> 再执行代码
```

实施过程中：

- 不为了消除编译错误制造跨层依赖；
- 不修改与当前任务无关的模块；
- 不修改 Vendor 源码，除非计划明确要求；
- 不在 CubeMX 文件中增加大量业务逻辑；
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

不得用静态检查结果代替真实 Keil / Board 证据。

---

# 10. 下一阶段入口

下一阶段：

```text
APP Phase 1 Design
```

当前不是生产代码实施阶段。

APP Phase 1 的设计目标应是：

```text
将已经验证的 UART Service 垂直链路
从“临时板测 Task”正式接入 01_APP/
```

设计时重点回答：

- APP 如何作为 Composition Root 持有 UART / Service / Task / backing storage；
- Communication Task 的正式生命周期由谁管理；
- APP 如何 `wait_event()` / drain RingBuffer；
- RX_AVAILABLE / DATA_LOSS / ERROR / STOPPED 如何进入产品级行为；
- CubeMX `freertos.c` 如何保持薄适配；
- APP Phase 1 是否只做字节流消费，暂不引入 Protocol Parser / Async TX；
- 如何建立 Host / Keil / Board 验收门禁。

在 APP 专项设计冻结之前：

```text
DO NOT implement production APP code.
DO NOT modify completed UART / RingBuffer / OS contracts for APP convenience.
```

---

# 11. 历史追溯位置

详细历史请使用：

```text
00_Doc/02_架构设计/                  -> 各阶段冻结专项设计
00_Doc/04_Agent/requirements.md     -> 项目需求基线
00_Doc/04_Agent/architecture.md     -> 完整架构合同
Git history                         -> 实施过程、阶段提交和中间状态
Tests/                              -> Host 验证代码
```

本文件不再长期保存：

- 已完成阶段的逐 Task implementation plan；
- 大段公共 API 原型；
- 历史 Scope Guard 全文；
- 临时测试代码；
- 每个 commit 的流水记录；
- 已被专项设计文档保存的重复合同。

原则：

> `handoff.md` 负责快速恢复“工程现在是什么、哪些不能动、下一步是什么”；
> 专项设计负责“为什么这样设计”；
> implementation plan 负责“当前阶段怎么执行”；
> Git history 负责“过去具体发生了什么”。
