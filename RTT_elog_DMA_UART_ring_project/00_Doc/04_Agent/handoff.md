# 工程长期记忆与交接说明

更新时间：2026-09-05

> 本文件是 AI Agent / Codex 与人工开发者恢复工程上下文时的长期入口。  
> 当前项目核心功能阶段已经完成并通过目标板综合验证，现作为稳定基线阶段性封版。  
> 下一步先在本工程上尝试接入显示器，用于继续验证既有分层、设备抽象与任务模型的扩展能力；显示器阶段完成后，以本工程的成熟架构与可复用模块为基底，新建独立的 Bootloader + OTA 学习工程。  
> Bootloader 不要求复制当前完整五层应用架构，应根据启动可靠性、体积和依赖最小化原则单独裁剪。

---

# 1. 项目定位

工程根目录：

```text
RTT_elog_DMA_UART_ring_project/
```

目标环境：

```text
MCU        : STM32F411CEU6 / Cortex-M4F
UART       : USART1 / 115200 8N1
RTOS       : CMSIS-RTOS2 + FreeRTOS
Debug Log  : EasyLogger + SEGGER RTT
Sensors    : DHT20 + MPU6050
I2C        : Software I2C over PB6/PB7
Input      : PA0 User Key
Indicator  : PC13 Status LED
```

项目最终形成：

```text
Button + UART unified control
 -> APP Control FSM
 -> FreeRTOS 4-task application model
 -> DHT20 + MPU6050 unified acquisition
 -> shared Software I2C
 -> UART DMA + IDLE + RingBuffer communication
 -> RTT / EasyLogger diagnostics
 -> LED semantic feedback
```

项目价值重点不是业务功能数量，而是：

```text
小型 RTOS 嵌入式系统如何分层
设备能力如何抽象与复用
ISR / Task / Service 的边界如何划分
跨 Task 数据如何设计
业务状态如何保持唯一真值
DMA / RingBuffer / Queue / Notify 如何组合
如何通过 Host + Keil + Target 三层验证完成工程闭环
```

---

# 2. 稳定分层合同

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

固定规则：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED

APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

职责概念：

```text
APP      : 业务状态、任务调度、业务编排
Service  : 可复用业务能力，不绑定具体 MCU
Platform : 设备/OS 能力抽象与统一接口
Impl     : STM32 / FreeRTOS 等具体实现适配
Vendor   : HAL / CMSIS / FreeRTOS / 第三方库
```

CubeMX 生成文件只承担：

```text
hardware initialization
scheduler bootstrap
IRQ / HAL Callback
thin glue
```

禁止把主要业务重新塞回 generated files。

---

# 3. 最终阶段状态

```text
Phase 1  GPIO STM32 Impl                         COMPLETED
Phase 2  Board Resource + CubeMX Configuration   COMPLETED
Phase 3  Software I2C                            COMPLETED
Phase 4  LED Module                              COMPLETED
Phase 5  Button Module                           COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 6  DHT20 Environment Module                COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 7  MPU6050 Motion Module                   COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 8  UART Application Communication          COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 9  Final RTOS Application Integration      COMPLETED / HOST + KEIL + TARGET VERIFIED

Final Integrated Board Test                      PASS
Project Core                                     COMPLETE / BASELINE FROZEN
```

原 RTOS Task/Event 阶段与 Final APP Integration 已合并为 Phase 9。

不存在原规划中的独立 Phase 10；当前 Phase 1~9 作为第一阶段核心工程基线正式结束。

`00_Doc/04_Agent/implementation_plan.md` 现在属于已完成的历史实施计划，不再作为新的施工入口。

显示器接入属于新的扩展阶段，开始前应重新完成硬件资源确认、数据手册蒸馏、架构边界讨论与新的 Implementation Plan，不直接复用旧计划继续施工。

---

# 4. 最终产品行为

默认启动：

```text
APP state = STOPPED
LED = OFF
UART RX active
RTT active
no periodic acquisition
```

Button：

```text
SINGLE
STOPPED -> START -> RUNNING
 -> LED ON
 -> immediate first complete sample/report
 -> then every 2 s

LONG >= 3 s
RUNNING -> STOP -> STOPPED
 -> LED OFF
 -> stop future periodic sampling

DOUBLE
STOPPED -> SAMPLE_ONCE
 -> complete DHT20 + MPU6050 sample
 -> complete UART report TX success
 -> LED blink 3 times
 -> OFF
 -> remain STOPPED
```

UART commands：

```text
START\r\n
STOP\r\n
ONCE\r\n
STATUS\r\n
HELP\r\n
```

协议约束：

```text
strict CRLF
uppercase only
case-sensitive
no trim
no arguments
fixed-size storage
```

---

# 5. UART DMA + RingBuffer 最终架构

RX：

```text
USART1 RX
 -> DMA2_Stream2 / Channel 4 / Circular
 -> IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform UART RX_DATA
 -> UART Service
 -> SPSC RingBuffer
 -> Communication Task
```

RX 原则：

```text
single producer / single consumer
RingBuffer lock-free
no ordinary mutex
no second RX path
```

TX：

```text
Communication Task
 -> service_uart_write()
 -> platform_uart_write_async()
 -> STM32 UART Impl
 -> HAL_UART_Transmit_DMA()
 -> DMA2_Stream7 / Channel 4 / Normal
 -> TX complete callback
 -> Platform event
 -> UART Service ownerThread wake
```

TX 原则：

```text
Communication Task = sole USART1 product TX requester
one active TX transaction
RX + TX simultaneously allowed
no TX RingBuffer / Queue / worker inside UART Service
service_uart_write() returns only after DMA no longer uses caller buffer
USART1 = product control/data
RTT = diagnostics
```

---

# 6. 最终四任务模型

固定 4 个产品 Task：

```text
Communication Task   2048 B   ABOVE_NORMAL
Control Task         1024 B   ABOVE_NORMAL
Acquisition Task     1536 B   NORMAL
Indicator Task        768 B   BELOW_NORMAL
```

职责：

```text
Communication Task
- UART RX parser
- UART Service ownerThread
- sole product UART TX requester
- product response/report formatting

Control Task
- Button 10 ms polling
- Button gesture processing
- sole APP Control FSM
- business orchestration

Acquisition Task
- acquisition scheduling
- sole runtime DHT20 / MPU6050 accessor
- sole shared Software I2C runtime accessor

Indicator Task
- LED semantic execution
- ONCE success blink
```

CubeMX `defaultTask` 保留生成声明，但在 USER CODE 区执行：

```c
(void)argument;
osThreadExit();
```

因此它不是第五个长期产品 Task。

Idle Task 与 Timer Service Task 是 RTOS 内核任务。

---

# 7. APP Control FSM

唯一业务状态：

```text
STOPPED
RUNNING
```

只有 Control Task 修改该状态。

`onceActive` / `onceSource` 是 operation context，不是第三业务状态。

统一控制事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

Button 与 UART 都映射到同一 FSM。

设计经验：

```text
输入源可以很多
业务状态真值只能有一个 owner
```

这避免 Button、UART、Sensor 等模块各自保存 `running` 状态造成状态漂移。

---

# 8. Unified Acquisition Service

位置：

```text
02_Service/service_acquisition/
```

职责：

```text
DHT20 read
 -> MPU6050 read
 -> one complete atomic acquisition result
```

不负责：

```text
2 s scheduling
START / STOP / ONCE
Task / Queue
UART
LED
shared I2C lifecycle
```

成功语义：

```text
DHT20 OK && MPU6050 OK -> complete acquisition success
otherwise             -> whole acquisition failed
```

即使 DHT20 失败，也继续尝试 MPU6050，用于诊断和共享总线问题区分。

采用 temporary result：两者都成功才 commit caller output，失败不产生 partial business result。

可复用设计原则：

```text
Task decides WHEN
Service decides HOW
Platform decides HOW DEVICE IS ACCESSED
```

---

# 9. Acquisition Task 调度

Command：

```text
START_PERIODIC
STOP_PERIODIC
SAMPLE_ONCE
```

周期策略：

```text
STOPPED -> Queue WAIT_FOREVER
START -> immediate first sample
RUNNING -> queue_receive(timeout until absolute deadline)
nextDeadline += 2000 ms
```

不使用简单 `delay(2000)`，避免执行耗时累积到采样周期形成长期漂移。

超期时跳过 missed periods，不做 catch-up burst。

STOP 若到达正在执行的同步 Sensor transaction：

```text
不粗暴中断 Software I2C
finish low-level transaction safely
check pending STOP before periodic publish
STOP already observed -> discard stale periodic result
```

---

# 10. APP IPC 最终结构

```text
Control Queue                  depth 8
Acquisition Command Queue      depth 4
Communication Outbound Queue   depth 8
Indicator Queue                depth 4
```

原则：

```text
bounded
value-copy
no temporary stack pointer
no infinite producer block
queue full observable
```

业务数据通过固定大小 message / union 按值传递。

第一版没有引入：

```text
APP state mutex
I2C mutex
Queue Set
Event Group as business event bus
runtime malloc/free for APP business data flow
```

---

# 11. ONCE 完整事务语义

成功条件：

```text
DHT20 success
AND MPU6050 success
AND complete UART report TX success
```

完整链：

```text
Control SAMPLE_ONCE
 -> Acquisition
 -> Unified Acquisition Service
 -> Communication ONCE_REPORT
 -> UART TX success
 -> ONCE_TX_RESULT OK
 -> Control
 -> Indicator ONCE_SUCCESS
 -> blink 3 times
```

重要工程原则：

```text
Queue submission success
!= business execution success

Acquisition success
!= communication success
```

只有完整事务成功才产生成功 LED 反馈。

Communication 不直接决定 LED 业务语义，Control 负责跨模块业务编排。

---

# 12. Host / Keil / Target 验证闭环

Phase 9 最终记录：

```text
Host regression : PASS / 34 of 34 test groups
Keil rebuild    : PASS / 0 Error(s)
Target test     : PASS
```

已完成目标板综合验证，包括：

```text
boot STOPPED / LED OFF
Button START / STOP / ONCE
UART START / STOP / ONCE / STATUS / HELP
immediate first acquisition
2 s periodic acquisition/report
DHT20 + MPU6050 sequential shared Software I2C
UART TX DMA
UART RX while TX active
ONCE success feedback
```

用户已于 2026-09-05 确认目标板综合功能测试完成并符合预期。

---

# 13. 当前资源基线

最终 Keil MAP 基线：

```text
Total RO Size   = 55808 B  / 54.50 KiB
Total RW Size   = 45616 B  / 44.55 KiB
Total ROM Size  = 55876 B  / 54.57 KiB
```

STM32F411CEU6：

```text
Flash = 512 KiB
SRAM  = 128 KiB
```

因此当前约：

```text
Flash usage ≈ 10.7%
RAM linked usage ≈ 34.8%
```

MAP 中当前最明显的 RAM 大项：

```text
EasyLogger async buffer / elog_async.o ZI = 20480 B
FreeRTOS heap_4 ucHeap                  = 15360 B
```

说明当前 RAM 占用主要不是五层业务架构本身，而是：

```text
logging buffers
FreeRTOS heap reservation
RTOS / middleware runtime storage
```

当前优化原则：

```text
不要因为文件多就先删分层
不要先压几百字节 Queue / RingBuffer
先找到资源 elephant
再用运行时数据决定是否优化
```

若未来 RAM 紧张，优先评估：

```text
1. EasyLogger 20 KiB async buffer 是否需要这么大
2. FreeRTOS minimum-ever free heap
3. 各 Task stack high-water mark
4. Queue peak occupancy
```

Flash 当前非常宽裕，无需为了几 KiB 提前牺牲可读性。

`app_communication` 使用浮点 `snprintf` 格式化传感器报告，会拉入 printf / float formatting library；这是未来 Flash 受限平台上的明确优化点，但当前 STM32F411 项目不需要处理。

---

# 14. 尚未完成但不阻塞项目封版的资源测量

功能项目已经完成，但以下属于后续可选 Resource Optimization Review：

```text
Communication Task stack high-water mark
Control Task stack high-water mark
Acquisition Task stack high-water mark
Indicator Task stack high-water mark

xPortGetFreeHeapSize()
xPortGetMinimumEverFreeHeapSize()

4 APP Queue observed peak occupancy
```

这些数据用于决定是否缩减：

```text
Task stack
configTOTAL_HEAP_SIZE
Queue depth
logging buffer
```

没有运行时证据前，不做拍脑袋资源压缩。

---

# 15. 本项目最值得沉淀的可复用经验

后续新对话建议逐项复盘并提炼为独立知识卡 / 模板 / README：

## 15.1 分层与 Composition Root

重点：

```text
为什么 APP / Service 不直接依赖 Impl
Platform / Impl 如何形成 MCU / RTOS Bridge
app_system 为什么应该是 Composition Root
为什么依赖先初始化、Thread 最后创建
```

可沉淀：

```text
嵌入式五层架构模板
模块依赖规则
Composition Root 初始化/回滚模板
```

## 15.2 Data / Context / Statistics 模型

重点：

```text
Config = 静态设置 / 构造依赖
Context = 生命周期 + runtime state
Data = 业务数据
Statistics = 诊断统计
```

适用于 UART、Button、Sensor、Service 等模块。

## 15.3 UART DMA + RingBuffer

重点：

```text
Circular DMA + IDLE / HT / TC
DMA producer position -> delta calculation
SPSC RingBuffer ownership
ISR only captures/events
Task performs parsing/business
TX async implementation + task-facing synchronous completion
```

这是项目最重要的技术主线之一。

## 15.4 RTOS IPC 与任务边界

重点：

```text
什么时候拆 Task
什么时候不要拆 Task
Queue value-copy
Notify 用于轻量 wake/completion
唯一 owner 降低 mutex 需求
```

## 15.5 唯一状态真值

重点：

```text
Button / UART 是输入源
Control FSM 是唯一 state owner
模块之间传 semantic event，不复制业务状态
```

## 15.6 时间调度

重点：

```text
absolute deadline vs delay(period)
事件 Queue + timeout 合并事件驱动和周期任务
missed deadline skip vs catch-up
```

## 15.7 完整事务语义

重点：

```text
submission success != execution success
partial acquisition != complete acquisition
TX success 才能形成完整 ONCE success
```

## 15.8 ISR 设计边界

重点：

```text
ISR / HAL callback:
- capture state
- update light context
- ISR-safe notification

Task context:
- parser
- I2C
- logs
- business FSM
- blocking LED blink
```

## 15.9 工程验证流程

```text
Design Freeze
 -> Implementation Plan
 -> Host Test
 -> Keil Build
 -> Target Test
 -> Documentation Closure
```

后续可讨论如何把这一套流程复用到新的 STM32 模块或项目。

## 15.10 MAP / Runtime Resource Review

重点：

```text
MAP = static resource ownership
High Water Mark = Task stack runtime peak
Minimum Ever Free Heap = RTOS heap worst-case margin
Queue Peak = IPC capacity evidence
```

不要只看 `Program Size` 一行。

## 15.11 复用层级

本阶段复盘形成以下区分：

```text
代码级复用
- RingBuffer
- platform_common
- platform_os
- 部分 platform_mcu

能力级复用
- UART Service
- Log interface / backend binding
- UART + DMA + RingBuffer 异步字节流基础设施

设计模式级复用
- Unified Acquisition Service
- Control FSM
- Composition Root
- Data / Context / Statistics
- Task / Service responsibility split
```

不要把“可复用”简单理解为下个项目原样复制文件。

---

# 16. 后续面试讲解主线

后续新对话应把项目从“功能说明”升级成“工程问题与设计决策说明”。

不建议只说：

```text
做了 DHT20 + MPU6050 采集和串口打印
```

更合适的项目定位：

```text
以 STM32F411 + FreeRTOS 为平台，围绕 UART DMA + RingBuffer 数据链路，
设计 APP / Service / Platform / Impl / Vendor 五层架构，
逐步接入 Button、LED、Software I2C、DHT20、MPU6050，
通过 4 个业务 Task、Queue、Task Notify 和统一 Control FSM，
完成控制、周期采集、异步通信、诊断与目标板验证闭环。
```

面试深挖建议按以下层次讨论：

```text
Level 1 — 项目整体数据流
Level 2 — UART DMA + RingBuffer
Level 3 — FreeRTOS Task / Queue / Notify
Level 4 — 五层架构与依赖倒置
Level 5 — APP FSM / ownership / concurrency
Level 6 — Software I2C 与共享资源
Level 7 — 异常、回滚、诊断与测试
Level 8 — 资源分析与进一步优化
Level 9 — 如果换 MCU / RTOS / Sensor，哪些层需要改
Level 10 — 哪些设计是为了学习工程化，实际产品中会如何裁剪
```

后续应准备：

```text
30 秒项目介绍
2 分钟项目介绍
5~10 分钟完整架构讲解
UART DMA 深挖问答
FreeRTOS 深挖问答
软件架构深挖问答
异常与并发场景问答
资源优化问答
设计取舍 / 反思题
```

---

# 17. 后续新对话推荐入口

当前核心阶段已经结束，后续讨论分为以下几条主线：

```text
A. Display Extension（近期下一阶段）
- 先确认 P169H002-V5-CTP 显示模组原理图 / 接口 / 引脚资源
- 收集并蒸馏 LCD Controller / CTP 数据手册
- 判断 SPI、触摸中断、背光等硬件资源占用
- 讨论 Platform / BSP / Service / App 边界
- 决定显示刷新由哪个 Task/Service owner
- 设计冻结后建立新的 Implementation Plan
- Agent 实现 -> Keil -> Target 验证 -> 更新本交接文档

B. 可复用经验沉淀
- 哪些 Platform / Service 可以直接迁移到新项目
- 哪些代码模板值得独立出来
- 哪些文档模板可以作为以后项目标准件
- 哪些设计思想应该写成知识卡

C. 项目面试讲解
- 项目故事线
- 架构图 / 数据流图
- 每个关键设计为什么这样选
- 面试官可能追问什么
- 如何区分“学习型工程化”与“真实产品合理裁剪”

D. Resource Optimization Review（可选）
- Task stack high-water mark
- minimum-ever free heap
- Queue peak occupancy
- EasyLogger async buffer
- MAP 文件模块级 Flash / RAM 分析

E. Bootloader + OTA 新工程（显示器扩展之后）
- 不在当前工程中继续堆叠 Bootloader 复杂度
- 以当前工程为软件资产与设计经验基底创建新的独立工程
- Application 侧可选择复用 platform_common / platform_mcu / UART Service / RingBuffer 等成熟资产
- Bootloader 侧采用 Minimal Core + Minimal Platform + Protocol + Config 思路
- Bootloader 优先目标：小、稳定、确定、依赖少、可恢复
- 重点重新设计 Flash Layout、Image Metadata、Upgrade State、CRC/Hash、Confirmed/Rollback、App Jump
- OTA 下载/复杂通信能力优先放在 Application，Bootloader 只承担必要的校验、安装、回滚和跳转职责
```

恢复当前工程上下文时优先读取：

```text
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/architecture.md
00_Doc/00_项目需求/最终功能需求.md
00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
```

开始 Bootloader + OTA 新工程时，不要直接把本工程 `implementation_plan.md` 当作施工计划；只把本工程作为架构资产、模块资产和开发流程参考。

---

# 18. 当前停止点

```text
Current core phase: CLOSED / BASELINE FROZEN
Project architecture: FROZEN for Phase 1~9 baseline
Production code: COMPLETE
Host tests: PASS
Keil build: PASS
Target integrated test: PASS
Project Core: COMPLETE

Optional runtime resource measurement: PENDING
Optional resource optimization: NOT STARTED

Immediate next phase:
Display integration / P169H002-V5-CTP exploration
Status: NOT DESIGNED / NOT STARTED

Planned following project:
New Bootloader + OTA project derived from this project's reusable assets
Status: PLANNED / SEPARATE PROJECT
```

当前基线除修复缺陷外原则上保持稳定。

显示器接入视为新的增量开发阶段，应继续沿用已经形成的流程：

```text
需求定义
 -> 硬件事实确认
 -> Datasheet / Reference Manual 收集
 -> 项目化资料蒸馏
 -> 架构与接口设计
 -> 施工方案讨论
 -> Design Freeze
 -> Documentation + Implementation Plan + Agent Handoff
 -> Agent Implementation
 -> Keil Integration
 -> Target Test
 -> Documentation Closure
```

---

# 19. 阶段性封版决定（2026-09-05）

当前项目 Phase 1~9 的核心目标已经达成，本阶段正式结束。

阶段封版结论：

```text
1. 现有 STM32F411 + FreeRTOS 应用工程作为后续实验的稳定 Application Baseline。
2. UART + DMA + RingBuffer + UART Service 作为通用异步串行通信能力继续保留和复用。
3. platform_common / platform_os / platform_mcu / log interface 作为优先验证的跨项目复用资产。
4. Unified Acquisition Service 主要复用其边界和设计模式，不假设具体代码可无条件迁移。
5. 下一阶段优先尝试显示器接入，用于验证本架构继续扩展 UI / Display 设备后的边界是否合理。
6. 显示器阶段完成后，以当前项目为基底创建新的 Bootloader + OTA 工程。
7. Bootloader 不追求与 Application 架构形式统一，优先采用更轻量的软件结构；当前五层架构仅作为设计经验和可选接口资产来源。
```

后续 Agent 恢复上下文时，必须先判断当前任务属于：

```text
当前 Application 基线维护
Display Extension
资源/复用/面试复盘
新的 Bootloader + OTA 工程
```

不要把这些阶段混成一个持续膨胀的单一施工计划。
