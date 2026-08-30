# 工程交接说明

更新时间：2026-08-30

> 本文件维护“当前工程状态 + 已完成阶段验收摘要 + 当前阶段执行边界”。
> 已完成阶段的详细过程记录、临时板测代码和中间状态以 Git 历史及对应专项设计文档为准，
> 不再在本文件顶部保留已过期的“当前阶段”描述。

---

## 1. 当前工程快照

- 工程根目录：`RTT_elog_DMA_UART_ring_project/`
- 当前分支：`main`
- 当前活动阶段：`UART Service Phase 1`
- 当前状态：`CODE_COMPLETE_PENDING_POST_TEST_KEIL_REBUILD`
- MCU：STM32F411CEU6，Cortex-M4F
- 软件环境：STM32 HAL、CMSIS-RTOS2 / FreeRTOS、Keil MDK-ARM、EasyLogger、SEGGER RTT

固定依赖方向：

```text
APP -> Service -> Platform -> Impl -> Vendor / HAL / RTOS / Hardware
```

当前阶段建立：

```text
Platform UART RX_DATA / ERROR / CANCELED
   ↓
UART Service RX Session / Statistics
   ↓
SPSC RingBuffer
   ↓
Platform Notify
   ↓
Dedicated Communication Task
```

本阶段不接入：

```text
Protocol Parser
Async TX Service
Frame Queue
Multi-UART aggregation
Multi-Consumer
Service-created Task
Automatic UART error recovery
service_log
```

当前权威执行文档：

```text
00_Doc/02_架构设计/UART_Service_Phase1设计.md
00_Doc/04_Agent/implementation_plan.md
00_Doc/04_Agent/execution_rules.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
```

当前实施范围已冻结为：单 Platform UART、单 UART Service、单专用 Consumer Task。
APP 持有 Platform UART 硬件 lifecycle、Task lifecycle 和所有 backing storage；
UART Service 持有活动 RX Session，且是唯一允许取消该 Session 的模块。

Platform UART 公共 API 保持冻结，仅有已批准例外：

```text
UART Service Phase 1 may add platform_uart_set_callback();
all other Platform UART public API semantics remain frozen.
```

若历史段落、旧提交说明或旧阶段 Scope Guard 与上述当前文档冲突，以当前冻结专项设计和
`implementation_plan.md` 为准；如仍存在真实架构冲突，按 `execution_rules.md` 执行 STOP / BLOCKED。

---

## 2. 已完成阶段

### 2.1 Log Phase 1 — COMPLETED

已确认：

- Platform Log 与 EasyLogger / RTT 解耦；
- Host Test PASS；
- Keil Build PASS；
- RTT Runtime Smoke Test PASS。

当前日志链路：

```text
APP / Service
    ↓
Platform Log
    ↓
Impl Log Adapter
    ↓
EasyLogger / RTT
```

`02_Service/service_log/` 仍只作为占位，不因目录存在而要求实现额外转发层。

---

### 2.2 UART Phase 1 — COMPLETED

已确认：

- construct / init / start / stop / restart / deinit；
- Blocking TX；
- Blocking fixed-length RX；
- 生命周期状态保护；
- 恢复临时测试代码后的 Keil Full Rebuild `0 Error(s)`。

Platform UART 公共 API 已冻结。UART Service Phase 1 唯一批准的例外为
`platform_uart_set_callback()`；除此之外不得修改既有 Platform UART 公共语义。

---

### 2.3 UART Phase 2A — COMPLETED

专项设计：

```text
00_Doc/02_架构设计/UART_Phase2A_DMA_RX设计.md
```

实现链路：

```text
USART1 RX
    ↓
DMA Circular
    ↓
IDLE / HT / TC
    ↓
STM32 UART Impl
    ↓
Platform RX_DATA Event
```

冻结语义：

- `platform_uart_read_async(uart, buffer, bufferSize)` 启动持续 RX Session；
- Caller / 后续 Service 持有 DMA RX Storage；
- DMA / Impl 在活动 Session 中为唯一写者；
- `RX_DATA.event.data` 仅在 callback 执行期间有效；
- Platform 不区分 RX_DATA 来源是 IDLE / HT / TC；
- Wrap 时最多拆成两个连续 RX_DATA 片段；
- `cancel(RX)` 终止 Session 并产生 CANCELED；
- lifecycle stop 静默终止 RX，不产生 CANCELED；
- HAL ORE 映射 `PLATFORM_ERR_OVERFLOW`；DMA / PE / NE / FE 映射 `PLATFORM_ERR_IO`；
- ISR / callback 不阻塞、不 malloc、不做完整协议解析、不做大量日志。

真实板测已确认：

```text
Short + IDLE                         PASS
Multiple Bursts                     PASS
Continuous 640 Bytes                PASS
received=640 / mismatch=0
HT / TC / IDLE Boundary 300 Bytes   PASS
Cancel Event / Restart              PASS
Lifecycle Stop / Restart            PASS
Capture Overflow                    0
Error Events                        0
```

临时板测代码已恢复，后续 Keil `0 Error(s)` 已补齐，因此 UART Phase 2A 正式 `COMPLETED`。

---

### 2.4 RTOS Platform Phase 1 — COMPLETED

专项设计：

```text
00_Doc/02_架构设计/RTOS_Platform_OS设计.md
```

已实现七类 Platform OS 能力：

```text
Thread
Mutex
Semaphore
Queue
Thread Notification
Software Timer
Time / Delay
```

冻结依赖：

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

公共 Platform Header 未暴露 CMSIS / FreeRTOS Handle 或 Header。

Host / Regression 已确认：

```text
Tests/platform_os                     PASS
Platform OS Header Isolation          PASS
Tests/platform_uart                   PASS
Tests/impl_platform_uart              PASS
Tests/platform_log                    PASS
Coding Standard Review                PASS
```

Keil Integration 已完成，真实构建 `0 Error(s)`。

真实板测已确认：

```text
TIME                PASS
THREAD              PASS
MUTEX               PASS
SEMAPHORE           PASS
QUEUE               PASS
NOTIFY TASK         PASS
TIMER               PASS
NOTIFY ISR          PASS
RTOS PLATFORM BOARD TEST: PASS
```

其中 `NOTIFY ISR` 使用真实 USART1 RX_DATA ISR 路径调用 `platform_notify_set_from_isr()` 唤醒 Task。
临时板测代码已经完整恢复，恢复后的最终 Keil Rebuild 为 `0 Error(s)`。

---

## 3. RingBuffer Phase 1 历史合同

专项设计：

```text
00_Doc/02_架构设计/RingBuffer_SPSC设计.md
```

历史实施计划：

```text
00_Doc/04_Agent/implementation_plan.md
```

目标目录：

```text
02_Service/service_common/
├── ring_buffer.h
└── ring_buffer.c

Tests/ring_buffer/
└── test_ring_buffer.c
```

RingBuffer Phase 1 已完成；本节保留其冻结合同与验收记录，供 UART Service Phase 1
作为既有 SPSC 容器依赖，不重新设计其 API、`N - 1` 容量规则或并发合同。

---

## 4. RingBuffer 冻结合同

RingBuffer V1 为纯 C 字节流容器：

```text
Single Producer / Single Consumer
Caller-owned backing storage
No malloc / free
No Mutex / Semaphore / Critical Section
No RTOS API
No UART / DMA / HAL dependency
No Notification
No Log
No Statistics
No Protocol Parser
```

对象模型：

```c
typedef struct
{
    uint8_t *storage;
    platform_size_t storageSize;
    volatile platform_size_t readIndex;
    volatile platform_size_t writeIndex;
} ring_buffer_t;
```

并发所有权：

```text
Producer:
    writeIndex -> 唯一写者
    readIndex  -> 只读快照

Consumer:
    readIndex  -> 唯一写者
    writeIndex -> 只读快照
```

发布顺序：

```text
Producer: copy bytes -> publish writeIndex
Consumer: copy bytes -> publish readIndex
```

当前合同仅针对 STM32F411 单核 Cortex-M4 SPSC 场景，不宣称为通用 SMP lock-free 容器。

---

## 5. RingBuffer 容量 / Overflow 语义

采用保留一个空槽的 Head/Tail 模型：

```text
storageSize = N
usable capacity = N - 1
```

```text
Empty: readIndex == writeIndex
Full : next(writeIndex) == readIndex
```

不要求 `storageSize` 为 2 的幂。

Write 使用 Partial Write：

```text
保留已经缓存的旧数据
尽量写入仍可容纳的新数据前缀
绝不静默覆盖旧数据
无法容纳的尾部丢弃
writtenLength 返回实际写入长度
请求未完整写入 -> PLATFORM_ERR_OVERFLOW
```

RingBuffer 不保存：

```text
overflowCount
droppedBytes
totalWritten
totalRead
highWaterMark
UART Error Statistics
```

这些统计信息后续由 UART Service 根据 `dataLength`、`writtenLength`、错误码和可读长度维护。

---

## 6. RingBuffer V1 公共 API

```c
platform_error_t ring_buffer_init(
    ring_buffer_t *ringBuffer,
    uint8_t *storage,
    platform_size_t storageSize);

platform_error_t ring_buffer_reset(
    ring_buffer_t *ringBuffer);

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

platform_error_t ring_buffer_get_readable_size(
    const ring_buffer_t *ringBuffer,
    platform_size_t *readableSize);

platform_error_t ring_buffer_get_free_size(
    const ring_buffer_t *ringBuffer,
    platform_size_t *freeSize);
```

V1 不增加：

```text
peek / discard
read_until / find / read_line
write_byte / read_byte
zero-copy span
protocol helpers
is_empty / is_full convenience API
```

`reset()` 仅允许在 Producer 和 Consumer 均 quiescent 时调用，不属于并发安全 API。

---

## 7. RingBuffer Phase 1 历史执行计划

执行顺序：

```text
Task 0  Preflight / Scope Confirmation
    ↓
Task 1  Public Contract + Init / Reset / Queries
    ↓
Task 2  Non-Wrapping Read / Write
    ↓
Task 3  Wrap + Full + Partial Write Overflow
    ↓
Task 4  100000+ deterministic reference-model stress
    ↓
Task 5  Full Host Regression / Coding Standard Review
    ↓
Task 6  Keil Integration
    ↓
Task 7  Keil Clean Targets -> Rebuild all target files
    ↓
Task 8  Handoff / Completion
```

开发方法：

```text
RED -> GREEN -> Refactor -> Verify -> Commit
```

实现 Agent 必须先读取：

```text
00_Doc/04_Agent/execution_rules.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/02_架构设计/RingBuffer_SPSC设计.md
00_Doc/04_Agent/implementation_plan.md
03_Platform/platform_common/platform_def.h
03_Platform/platform_common/platform_types.h
03_Platform/platform_common/platform_error.h
```

其中 `platform_def.h` 已定义 `PLATFORM_TRUE`、`PLATFORM_FALSE`、`NULL`、`ARRAY_SIZE` 等公共宏；
RingBuffer 可按需使用，但不得重复定义。

---

## 8. RingBuffer 完成条件

只有以下全部满足，才能标记：

```text
RingBuffer Phase 1 = COMPLETED
```

必须具备真实证据：

- `ring_buffer.h/.c` 完成；
- Host Test PASS；
- deterministic reference-model stress PASS；
- UART / Platform OS / Platform Log Regression PASS；
- Coding Standard Review = PASS；
- RingBuffer 已加入 Keil 工程；
- Keil Full Rebuild = `0 Error(s)`；
- handoff 更新真实结果。

本阶段为纯软件模块，不要求额外目标板 Runtime Smoke Test，也不得为此在 `Core/Src/freertos.c`
增加临时板测逻辑。

Codex 环境若无法执行真实 Keil：

```text
CODE_COMPLETE_PENDING_KEIL_VERIFICATION
```

等待人工完成 `Clean Targets -> Rebuild all target files` 后才能最终收口。

---

## 8.1 RingBuffer Phase 1 执行记录（2026-08-30）

当前状态：

```text
COMPLETED
```

已创建或修改：

```text
02_Service/service_common/ring_buffer.h
02_Service/service_common/ring_buffer.c
Tests/ring_buffer/test_ring_buffer.c
MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
```

冻结实现合同已保持：

```text
SPSC：Producer 仅写 writeIndex，Consumer 仅写 readIndex。
Storage：调用者持有；RingBuffer 不 malloc / free。
容量：storageSize = N，实际可用容量 = N - 1。
发布顺序：复制完成后才发布对应 Index。
Overflow：Partial Write；保留旧数据，写入可容纳的新数据前缀，未完整写入返回 PLATFORM_ERR_OVERFLOW。
```

已完成验证：

```text
RingBuffer Host Test                         PASS
Deterministic Reference-Model Stress         PASS（固定种子，100000 次操作）
Platform UART Host Regression                PASS
Impl Platform UART Host Regression           PASS
Platform OS Host Regression / Header Test    PASS
Platform Log Host Regression                 PASS
Coding Standard Review                       PASS
Keil 工程静态集成                            PASS
Keil Clean Targets / Rebuild all target files   PASS（人工 Keil 确认）
```

本阶段未新增 UART Service、Platform Notify 集成、统计信息、Communication Task、DMA Buffer 所有权或协议解析。

Keil 人工验收已完成，RingBuffer Phase 1 满足完成门禁。

---

## 9. 当前 UART Service Phase 1 Scope Guard

UART Service Phase 1 仅实现单 UART RX 数据流：

```text
Platform UART RX_DATA / ERROR / CANCELED
    ↓
UART Service
    ↓
existing SPSC RingBuffer
    ↓
Platform Notify
    ↓
dedicated Communication Task
```

本阶段禁止实现：

```text
Protocol Parser
Async TX Service
Frame Queue
Multi-UART aggregation
Multi-Consumer
Service-created Task
Automatic UART error recovery
service_log
Dynamic memory allocation
HAL / DMA / RTOS concrete types in Service public API
```

仅允许对 Platform UART 增加已批准的：

```c
platform_uart_set_callback();
```

如果实现证明还必须修改其他冻结 Platform UART API、RingBuffer API、Platform Notify API、
SPSC 并发合同、`N - 1` 容量模型或 Partial Write 策略：

```text
STOP / BLOCKED
```

返回设计阶段重新评审，不得由执行 Agent 自行改变。

---

## 10. 当前 UART Service Phase 1

当前阶段按以下链路实施：

```text
Platform UART RX_DATA
        ↓ ISR Producer
UART Service
        ↓
SPSC RingBuffer
        ↓
Platform Notify From ISR
        ↓
Communication Task / APP
```

当前阶段引入：

```text
service_uart_config_t
service_uart_context_t
service_uart_statistics_t
DMA RX Storage ownership
RX overflow / data-loss statistics
UART error / cancel / restart policy
Notification wake-up semantics
```

专项冻结设计：

```text
00_Doc/02_架构设计/UART_Service_Phase1设计.md
```

只有 Host Test、既有 Regression、Coding Standard Review、Keil Full Rebuild、真实板级
ISR -> Service -> Task Smoke Test 及恢复后 Keil Rebuild 均具备真实证据时，
UART Service Phase 1 才可标记为 `COMPLETED`。

### 10.1 执行记录（2026-08-30）

当前状态：

```text
CODE_COMPLETE_PENDING_POST_TEST_KEIL_REBUILD
```

UART Service Phase 1 的代码、Host Test、既有 Host Regression、Keil 工程静态集成、
含临时板测代码的 Keil Rebuild 及真实板级 Smoke Test 已完成。
临时板测接线已从 `Core/Src/freertos.c` 完整恢复；当前环境没有可实际调用的 Keil `UV4.exe`，
恢复后的 Keil Rebuild 尚待人工执行，因此不得将本阶段标记为 `COMPLETED`。

本阶段实际修改或新增：

```text
02_Service/service_uart/service_uart.h
02_Service/service_uart/service_uart.c
03_Platform/platform_mcu/uart/platform_uart.h
03_Platform/platform_mcu/uart/platform_uart.c
Tests/service_uart/test_service_uart.c
Tests/platform_uart/test_platform_uart.c
MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
00_Doc/04_Agent/handoff.md
```

主要提交：

```text
376aa07 docs: start uart service phase1
7f055c9 feat(uart): add callback binding api
09fecb5 feat(service-uart): add service object lifecycle
7764e9c docs(service-uart): document public api
1ce857b docs(service-uart): document service data model
da7b072 feat(service-uart): add rx session lifecycle
37cb914 feat(service-uart): buffer rx events and statistics
007a252 feat(service-uart): add read status and statistics api
5864ebf feat(service-uart): add wait events and error handling
214b07f build: integrate uart service into keil
```

真实验证记录：

```text
UART Service Host Test                         PASS
RingBuffer Host Regression                     PASS
Platform UART Host Regression                  PASS
Platform UART Type Contract                    PASS
Impl Platform UART Host Regression             PASS
Platform OS Host Regression                    PASS
Platform OS Header Isolation                   PASS
Platform Log Host Regression                   PASS
Coding Standard Review: PASS
Keil 工程静态集成（Include Path / Source Group） PASS
Keil Clean Targets / Rebuild all target files PASS（人工 Keil 确认；含临时板测代码）
Board ISR -> Service -> Task Smoke Test        PASS（真实 USART1 RX + RTT，见下方）
Post-test restored Keil Rebuild                PENDING_MANUAL_VERIFICATION
```

真实板级 Smoke Test（2026-08-30）：

```text
配置：USART1 115200 / 8N1；串口助手发送 00..FF 循环的 1280 字节原始二进制文件；RTT Viewer 观察。
Lifecycle：restart stop_result=0 state=4（STOPPED）；restart start_result=0 state=2（RUNNING）。
Data path：received=1280 / buffered=1280 / read_total=1280 / dropped=0 / high_water=128 / mismatch=0。
Error path：未观察到 ERROR 或 DATA_LOSS 事件。
结论：USART1 RX DMA -> Platform callback -> UART Service -> RingBuffer -> 默认任务 -> RTT 链路 PASS。
```

用于上述板测的 `Core/Src/freertos.c` 临时 USER CODE 已完整删除，未作为生产代码提交。

`MDK-ARM/uart_phase1_baseline_rebuild.log` 和 `MDK-ARM/uart_phase1_smoke_rebuild.log`
属于仓库既有日志，不能作为本次 UART Service 集成后的 Keil 验收依据；其中 smoke 日志末尾记录了
`Error: C4051E: couldn't write file ... elog_port.o` 及 `Target not created.`。

冻结生命周期：

```text
construct Platform UART
-> service_uart_init(bind)
-> Platform hardware start
-> service_uart_start
-> service_uart_stop
-> Platform hardware stop
-> service_uart_deinit(unbind)
-> Platform hardware deinit
```

受控 Platform API 变更仅为：

```text
platform_uart_set_callback()
```

该 API 用于上层绑定异步事件所有者；除此以外没有改变任何 Platform UART 公共合同。

生命周期补充语义：`service_uart_stop()` 若在 `cancel()` 已使 Service 进入 `STOPPED` 后，
于通知阶段失败，则返回该通知错误。调用者不得仅凭该返回值推断 Service 仍处于运行状态，
应调用 `service_uart_get_status()` 获取真实状态。

---

## 11. 历史追溯说明

旧版 `handoff.md` 曾按 UART Phase 2A、RTOS Platform Phase 1 的执行过程持续追加中间状态，
因此顶部“当前活动阶段”和部分旧 Scope Guard 在阶段完成后产生了时效冲突。

从本版本开始：

```text
handoff.md                -> 当前状态 + 完成摘要 + 当前执行边界
专项设计文档              -> 冻结设计合同
implementation_plan.md    -> 当前阶段任务顺序
Git history               -> 已完成阶段详细过程 / 中间状态 / 临时验证记录
```

这样旧阶段的历史限制不再覆盖新阶段执行计划。
