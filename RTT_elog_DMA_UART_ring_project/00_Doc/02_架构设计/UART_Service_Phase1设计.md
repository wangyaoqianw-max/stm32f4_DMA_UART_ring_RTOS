# UART Service Phase 1 设计

> 文档类型：专项设计 / Frozen Design Contract  
> 状态：FROZEN  
> 版本：V1.0  
> 日期：2026-08-30

---

## 1. 目标

UART Service Phase 1 在既有 Platform UART DMA RX、Platform OS Notification 和 SPSC RingBuffer 之上，建立完整的 UART RX 字节流服务：

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
  ├── RX Session
  ├── RingBuffer
  ├── Runtime State
  ├── Statistics
  └── Task Wakeup
  ↓
Communication Task
  ↓
APP 业务处理
```

Phase 1 只解决单 UART RX 字节流从 ISR 安全进入任务上下文的问题，不实现协议帧解析。

---

## 2. 固定依赖与职责边界

依赖方向保持：

```text
APP -> Service -> Platform -> Impl -> Vendor / HAL / RTOS / Hardware
```

APP 负责：

- Platform UART 对象装配与硬件 lifecycle；
- Communication Task 创建、销毁、优先级和栈；
- DMA RX Buffer 实际存储；
- RingBuffer backing storage 实际存储；
- 业务数据处理与协议解析；
- UART ERROR 后是否恢复、何时恢复；
- 多阶段启动和关闭顺序协调。

UART Service 负责：

- 绑定 Platform UART callback；
- 管理持续 RX Session 的 start / stop；
- 接收 RX_DATA / ERROR / CANCELED；
- 在 callback 中把 RX_DATA 立即复制到 RingBuffer；
- 维护数据丢失状态与运行统计；
- 使用 Platform Notify 唤醒专用 Communication Task；
- 向 APP 提供 read / wait_event / status / statistics 接口。

RingBuffer 继续保持纯容器职责，不增加 UART、RTOS、统计或错误恢复行为。

Platform UART 继续负责 UART 抽象和异步事件，不管理 RingBuffer、Task 或协议。

---

## 3. Phase 1 简化假设

固定采用：

```text
1 Platform UART
    ↓
1 UART Service
    ↓
1 专用 Communication Task
```

该 Communication Task 的 Thread Flags 在 Phase 1 中由 UART Service 独占使用。

Phase 1 不支持：

- 多 UART Service 聚合到同一个 Consumer Task；
- 多 Consumer 同时读取一个 Service；
- 多 Producer 写同一个 RX RingBuffer；
- UART Service 内创建或销毁 Task；
- UART Service 内自动重启错误 RX；
- Async TX Service；
- Protocol Parser / Frame Queue；
- `service_log` 行为；
- 动态内存分配。

---

## 4. Buffer 与对象所有权

所有实际存储由 APP / Caller 静态或预分配持有：

```text
DMA RX Buffer          -> APP / Caller owns storage
RingBuffer Storage     -> APP / Caller owns storage
Communication Task     -> APP owns lifecycle
platform_thread_t      -> APP / Platform OS owns object
platform_uart_t        -> APP / Platform owns lifecycle
service_uart_t         -> APP / Caller owns object storage
```

UART Service 只保存引用，不 malloc / free 外部资源。

DMA RX Session 活跃期间：

- DMA / STM32 Impl 是 DMA RX Buffer 的唯一写者；
- UART Service callback 对 `RX_DATA.event.data` 只读，并在 callback 返回前完成复制；
- `event.data` 不得保存供后续异步使用。

RingBuffer 使用已冻结的 SPSC 合同：

```text
Producer = UART Service RX_DATA callback
Consumer = 专用 Communication Task 调用 service_uart_read()
```

---

## 5. Platform UART callback 受控扩展

现有 Platform UART 在构造时设置 callback，但 UART Service 需要在不让 APP 感知内部 callback wiring 的情况下接管事件。

Phase 1 批准增加一个受控公共 API：

```c
platform_error_t platform_uart_set_callback(
    platform_uart_t *uart,
    platform_uart_callback_t callback,
    void *callbackContext);
```

冻结语义：

- UART 对象必须已经完成 `platform_uart_init()` 构造；
- UART 状态为 `PLATFORM_OBJECT_STARTED` 时禁止修改 callback，返回 `PLATFORM_ERR_INVALID_STATE`；
- callback 绑定或解绑只能发生在没有活动异步事件源的阶段；
- `callback == NULL` 表示解绑，并同时清空 callbackContext；
- 该 API 不改变既有 RX_DATA / ERROR / CANCELED 语义。

这是对先前“Platform UART 公共 API 已冻结”的唯一受控例外。除该 setter 外，不允许在本阶段重新设计 Platform UART API。

---

## 6. 初始化与关闭顺序

推荐启动顺序：

```text
Platform UART construct
        ↓
创建 / 获得专用 Communication Task handle
        ↓
service_uart_init()
        └── bind Platform UART callback
        ↓
APP 完成 Platform UART hardware init / start
        ↓
service_uart_start()
        └── platform_uart_read_async()
```

因此：

- `service_uart_init()` 不要求 UART hardware 已 STARTED；
- `service_uart_start()` 要求 Platform UART 已处于 STARTED，并由 Platform API 校验；
- Service 不拥有 Platform UART hardware lifecycle。

推荐关闭顺序：

```text
service_uart_stop()
        ↓
APP stop Platform UART hardware
        ↓
service_uart_deinit()
        └── unbind callback
        ↓
APP deinit Platform UART hardware
```

`service_uart_deinit()` 必须在不存在活动 RX Session、Consumer 不再并发访问 Service 且 Platform UART 已允许 callback 解绑时执行。

---

## 7. 数据模型

### 7.1 Config

```c
typedef struct
{
    platform_uart_t *uart;

    uint8_t *dmaRxBuffer;
    platform_size_t dmaRxBufferSize;

    uint8_t *ringBufferStorage;
    platform_size_t ringBufferStorageSize;

    platform_thread_t *consumerThread;
} service_uart_config_t;
```

Config 在 `service_uart_init()` 时复制进 Service，因此 Config 结构体本身不需要长期存活；其中指向的外部对象和 Storage 必须按所有权合同保持有效。

Phase 1 不把 Platform Notify flag 数值暴露给 Config。

### 7.2 State

```c
typedef enum
{
    SERVICE_UART_STATE_UNINITIALIZED = 0,
    SERVICE_UART_STATE_INITIALIZED,
    SERVICE_UART_STATE_RUNNING,
    SERVICE_UART_STATE_STOPPING,
    SERVICE_UART_STATE_STOPPED,
    SERVICE_UART_STATE_ERROR,
    SERVICE_UART_STATE_MAX
} service_uart_state_t;
```

### 7.3 Context

```c
typedef struct
{
    volatile service_uart_state_t state;
    ring_buffer_t rxRingBuffer;
    volatile platform_error_t lastError;
    volatile platform_bool_t dataLossOccurred;
} service_uart_context_t;
```

`dataLossOccurred` 是当前 RX Session 的 sticky status：一旦 RingBuffer 无法完整保存 RX_DATA 即置 TRUE，直到下一次成功开启新 Session 时清除。

RingBuffer Overflow 不写入 `lastError`；`lastError` 用于记录 UART / Service runtime error 状态。

### 7.4 Statistics

```c
typedef struct
{
    volatile uint32_t rxEventCount;

    volatile uint32_t rxBytesReceived;
    volatile uint32_t rxBytesBuffered;
    volatile uint32_t rxBytesRead;
    volatile uint32_t rxBytesDropped;

    volatile uint32_t ringBufferOverflowCount;
    volatile platform_size_t ringBufferHighWaterMark;

    volatile uint32_t uartErrorCount;
    volatile uint32_t cancelCount;
} service_uart_statistics_t;
```

统计是诊断数据，32-bit 累计值允许自然回绕；Phase 1 不引入 64-bit 原子统计或锁。

实时 `currentReadableSize` / `currentFreeSize` 不缓存到 Statistics，直接查询 RingBuffer，避免双重真值。

### 7.5 Service Object

```c
#define SERVICE_UART_INITIALIZER {0}

typedef struct
{
    service_uart_config_t config;
    service_uart_context_t context;
    service_uart_statistics_t statistics;
} service_uart_t;
```

该 Config / Context / Statistics 划分对应真实不同生命周期，不要求其他模块机械复制该模式。

### 7.6 Status Snapshot

```c
typedef struct
{
    service_uart_state_t state;
    platform_error_t lastError;
    platform_bool_t dataLossOccurred;
} service_uart_status_t;
```

APP 不直接依赖内部 Context。

---

## 8. Service Event

APP 只看到 Service Event，不直接调用 Platform Notify：

```c
#define SERVICE_UART_EVENT_RX_AVAILABLE    (1U << 0)
#define SERVICE_UART_EVENT_DATA_LOSS       (1U << 1)
#define SERVICE_UART_EVENT_ERROR           (1U << 2)
#define SERVICE_UART_EVENT_STOPPED         (1U << 3)
```

底层 Platform Thread Flags 只使用一个 Service 私有 WAKE flag：

```text
Platform Notify = “Task 应重新检查 Service 真值”
Service Event   = 根据 RingBuffer + Context 重建
```

因此：

- Notification 只是 wake hint；
- RingBuffer readable size 和 Service state 才是真值；
- `DATA_LOSS` 是 sticky status 型事件，而不是一次性边沿事件；
- ERROR 后如果 RingBuffer 仍有数据，可以同时返回 `RX_AVAILABLE | ERROR`；
- Overflow 后如果部分数据成功写入，可以同时返回 `RX_AVAILABLE | DATA_LOSS`。

---

## 9. 公共 API

```c
platform_error_t service_uart_init(
    service_uart_t *service,
    const service_uart_config_t *config);

platform_error_t service_uart_start(
    service_uart_t *service);

platform_error_t service_uart_stop(
    service_uart_t *service);

platform_error_t service_uart_deinit(
    service_uart_t *service);

platform_error_t service_uart_read(
    service_uart_t *service,
    uint8_t *buffer,
    platform_size_t bufferSize,
    platform_size_t *readLength);

platform_error_t service_uart_wait_event(
    service_uart_t *service,
    uint32_t timeoutMs,
    uint32_t *events);

platform_error_t service_uart_get_readable_size(
    const service_uart_t *service,
    platform_size_t *readableSize);

platform_error_t service_uart_get_status(
    const service_uart_t *service,
    service_uart_status_t *status);

platform_error_t service_uart_get_statistics(
    const service_uart_t *service,
    service_uart_statistics_t *statistics);

platform_error_t service_uart_clear_statistics(
    service_uart_t *service);
```

Phase 1 不增加 write / async TX / read_frame / parser API。

---

## 10. 生命周期状态机

```text
UNINITIALIZED
     │ init
     ▼
INITIALIZED
     │ start
     ▼
  RUNNING ───── ERROR event ─────→ ERROR
     │                                │
    stop                              │ start
     ▼                                ▼
 STOPPING ─ CANCELED ─→ STOPPED ── start ─→ RUNNING
```

允许：

```text
init:    UNINITIALIZED -> INITIALIZED
start:   INITIALIZED / STOPPED / ERROR -> RUNNING
stop:    RUNNING -> STOPPING -> STOPPED
deinit:  INITIALIZED / STOPPED / ERROR -> UNINITIALIZED
```

`start()` 表示开始一个新的 RX Session：

- reset RingBuffer；
- 清除 `dataLossOccurred`；
- 清除本 Session runtime error；
- 不清空累计 Statistics；
- 启动 `platform_uart_read_async()`。

因此 ERROR / STOPPED 后若 RingBuffer 尚有旧数据，APP 可以先 drain；一旦调用 `start()`，即明确放弃旧 Session 尚未消费的数据。

---

## 11. RX_DATA callback 数据流

只在 `SERVICE_UART_STATE_RUNNING` 处理 RX_DATA：

```text
RX_DATA callback
    ↓
rxEventCount++
rxBytesReceived += dataLength
    ↓
ring_buffer_write()
    ↓
rxBytesBuffered += writtenLength
rxBytesDropped  += dataLength - writtenLength
    ↓
如未完整写入：
ringBufferOverflowCount++
dataLossOccurred = TRUE
    ↓
查询 readableSize
更新 ringBufferHighWaterMark
    ↓
platform_notify_set_from_isr(WAKE)
```

完整写入：

```text
rxBytesReceived += N
rxBytesBuffered += N
rxBytesDropped  += 0
```

Partial Write 示例：

```text
RX_DATA = 100
written = 70

rxBytesReceived          += 100
rxBytesBuffered          += 70
rxBytesDropped           += 30
ringBufferOverflowCount  += 1
dataLossOccurred          = TRUE
```

当 RingBuffer 完全满时，即使 `writtenLength == 0` 也必须尝试 WAKE Consumer，使其能够看到 `DATA_LOSS` 状态。

统计基本关系：

```text
rxBytesReceived == rxBytesBuffered + rxBytesDropped
```

在 32-bit counter 回绕前应成立；Host Test 使用不会触发回绕的数据验证该关系。

---

## 12. RingBuffer 统计语义

`ringBufferHighWaterMark` 表示自上次 `service_uart_clear_statistics()` 以来 RingBuffer 的历史最高 readable size。

写入后执行：

```text
readableSize = ring_buffer_get_readable_size()
if readableSize > ringBufferHighWaterMark:
    ringBufferHighWaterMark = readableSize
```

其上限必须满足：

```text
ringBufferHighWaterMark <= ringBufferStorageSize - 1
```

`service_uart_start()` 不清空 highWaterMark 或其他累计统计。

`service_uart_clear_statistics()` 只允许在：

```text
INITIALIZED
STOPPED
ERROR
```

且调用者必须保证 Consumer 当前不在 `service_uart_read()` 中。RUNNING / STOPPING 返回 `PLATFORM_ERR_INVALID_STATE`。

`clear_statistics()` 不清除 `dataLossOccurred` 或 `lastError`，因为 Statistics 与 Runtime Status 是不同语义。

---

## 13. Consumer read 语义

`service_uart_read()` 允许状态：

```text
RUNNING
STOPPED
ERROR
```

因此 stop / error 不会删除已经正确进入 RingBuffer 的数据。

读取为非阻塞：

```text
有数据：读取 min(readable, bufferSize)，rxBytesRead += readLength，返回 OK
无数据：readLength = 0，返回 PLATFORM_ERR_EMPTY
```

只允许配置中的专用 Consumer Task 调用 `service_uart_read()`；Phase 1 不增加运行时线程身份检查。

---

## 14. wait_event 防丢唤醒设计

`service_uart_wait_event()` 只能由配置中的专用 Consumer Task 调用。

为了处理 stale wake 和“检查状态后、进入 wait 前”到达的新事件，固定顺序：

```text
1. clear 当前线程上的 Service 私有 WAKE flag
2. 根据 RingBuffer + Context 重建 Service events
3. 如果 events != 0，立即返回
4. platform_notify_wait(WAKE, WaitAny, ClearOnExit, timeout)
5. 被唤醒后再次根据 RingBuffer + Context 重建 events
6. 返回 Service events
```

此顺序保证：

- clear 之前已经存在的数据不会丢，因为 RingBuffer / state 仍是真值；
- clear 之后、检查之前到达的数据由真值检查发现；
- 检查之后、wait 之前到达的数据会重新设置 WAKE flag，wait 立即返回。

允许 wait 状态：RUNNING / STOPPED / ERROR。INITIALIZED / STOPPING 返回 `PLATFORM_ERR_INVALID_STATE`。

---

## 15. ERROR 处理

Platform UART ERROR callback 当前来自 UART / DMA ISR 路径。

处理：

```text
RUNNING
  ↓ ERROR callback
lastError = event.error
uartErrorCount++
state = ERROR
  ↓
platform_notify_set_from_isr(WAKE)
```

Service 不在 ISR 中：

- cancel；
- restart；
- malloc；
- blocking wait；
- 完整协议解析；
- 大量日志。

Platform UART ERROR 已结束当前 RX Session，因此 Service 不再次 cancel。

APP 醒来后决定：

```text
drain remaining RingBuffer if needed
        ↓
service_uart_start() 重新开始新 Session
```

---

## 16. CANCELED 与 stop

当前 STM32 UART Impl 的 `platform_uart_cancel(RX)` 在 Task Context 同步产生 CANCELED callback。

正常 stop：

```text
service_uart_stop()
    ↓
state = STOPPING
    ↓
platform_uart_cancel(RX)
    ↓
CANCELED callback
    ↓
STOPPING -> STOPPED
    ↓
callback return
    ↓
platform_uart_cancel() return
    ↓
platform_notify_set(WAKE)   // Task Context
    ↓
service_uart_stop() return
```

因此 CANCELED callback 本身不调用任何 Notify API，避免在 callback 中判断当前到底是 ISR 还是 Task Context。

如果 cancel 返回失败：

```text
STOPPING -> RUNNING
return original error
```

Phase 1 规定 UART Service 是活动 RX Session 的唯一取消者。APP 不得绕过 Service 直接 `platform_uart_cancel(RX)`。

若出现非预期 CANCELED：

- 记录 `cancelCount`；
- 状态转 STOPPED；
- callback 内不额外通知；
- 视为违反组合合同，不为该异常路径增加复杂上下文识别机制。

---

## 17. ISR / Task 并发约束

唯一 Producer：UART RX callback。  
唯一 Consumer：专用 Communication Task。

Producer 主要写：

```text
RingBuffer.writeIndex
rxEventCount
rxBytesReceived
rxBytesBuffered
rxBytesDropped
ringBufferOverflowCount
ringBufferHighWaterMark
uartErrorCount
runtime error/data-loss state
```

Consumer 主要写：

```text
RingBuffer.readIndex
rxBytesRead
```

Shared scalar 使用 32-bit 对齐访问并按项目规范使用 volatile 保证可见性；volatile 不代表原子复合操作或锁。

`service_uart_get_statistics()` 返回 best-effort 非事务性快照：各字段可能来自非常接近但并非完全相同的瞬间。Phase 1 不为了统计快照增加关中断、Mutex 或 Critical Section。

`service_uart_clear_statistics()`、`start()` 的 RingBuffer reset、`deinit()` 均要求相关 Producer / Consumer 处于设计规定的 quiescent 阶段。

---

## 18. Callback 禁止事项

UART Service callback 禁止：

```text
malloc / free
Mutex / Semaphore wait
platform_notify_wait
阻塞 UART API
完整协议解析
大量格式化日志
自动 Error Recovery
APP callback
动态替换 Platform UART callback
```

允许：

```text
RingBuffer write
轻量统计更新
状态赋值
HighWaterMark 计算
ISR-safe notify
```

---

## 19. Scope Guard

UART Service Phase 1 不允许顺带实现：

```text
Async TX Service
Frame Queue
Protocol Parser
多 UART 聚合
多 Consumer
Service 创建 Task
自动 UART Error Recovery
service_log
HAL / UART_HandleTypeDef 进入 Service
DMA Handle / Stream / Channel 进入 Service
动态内存管理
```

除 `platform_uart_set_callback()` 外，如实现证明还必须修改已冻结 Platform UART API、RingBuffer API、Platform Notify API 或 SPSC 合同：

```text
STOP / BLOCKED
```

返回架构评审。

---

## 20. 验收条件

UART Service Phase 1 完成必须至少证明：

- Platform UART callback setter Host Test PASS；
- UART Service init / start / stop / restart / deinit Host Test PASS；
- RX_DATA 正常写入、Partial Write、Full Drop Host Test PASS；
- RingBuffer statistics 与 data-loss 语义 Host Test PASS；
- wait_event 无丢唤醒关键时序 Host Test PASS；
- ERROR / CANCELED 状态机 Host Test PASS；
- RingBuffer、Platform UART、Impl UART、Platform OS、Platform Log regression PASS；
- Coding Standard Review PASS；
- Keil Full Rebuild `0 Error(s)`；
- 真实板上 ISR -> Service -> RingBuffer -> Communication Task 链路 smoke test PASS；
- 临时板测代码恢复后再次 Keil Rebuild `0 Error(s)`；
- handoff 记录真实结果。
