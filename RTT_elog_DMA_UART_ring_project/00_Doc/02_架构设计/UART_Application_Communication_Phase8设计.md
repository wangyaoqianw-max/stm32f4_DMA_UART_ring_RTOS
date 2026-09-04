# UART Application Communication Phase 8 专项设计

> 文档类型：Phase Design / Architecture Decision  
> 状态：FROZEN FOR IMPLEMENTATION  
> 日期：2026-09-04  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 目标与范围

Phase 8 在现有 UART DMA RX + RingBuffer + Communication Task 基线上，完成两部分能力：

```text
A. 可复用 UART 全双工能力补齐
   RX = DMA Circular + IDLE / HT / TC
   TX = DMA Normal + TX COMPLETE

B. UART Application Communication
   byte stream -> CRLF line -> command parser -> APP control event
   local response / future control result -> UART Service -> DMA TX -> PC
```

正式数据链：

```text
PC
 ↓ START\r\n
USART1 RX
 ↓
DMA Circular
 ↓
STM32 UART Impl
 ↓ RX_DATA
Platform UART
 ↓
UART Service
 ↓
SPSC RingBuffer
 ↓
Communication Task
 ↓
CRLF Line Assembler
 ↓
Command Parser
 ↓
APP_CTRL_* event outlet
```

正式发送链：

```text
Communication Task
 ↓
UART Service
 ↓
Platform UART write_async
 ↓
STM32 UART Impl
 ↓
HAL_UART_Transmit_DMA
 ↓
USART1 TX
 ↓
PC Serial Assistant
```

本 Phase 明确不实现：

```text
APP STOPPED / RUNNING FSM
Acquisition Service / Acquisition Task
Button -> APP permanent IPC
Indicator permanent IPC
2 s sensor report production scheduling
ONCE complete business transaction
TX message queue / TX RingBuffer / buffer pool
multiple UART TX producers
JSON / binary protocol / CLI shell / parameters
case-insensitive commands / whitespace normalization
```

Phase 9 冻结永久 RTOS Task / IPC；Phase 10 实现最终 APP Control FSM 与系统闭环。

---

# 2. 当前真实基线

当前已经稳定：

```text
USART1 115200 8N1
PA9  TX
PA10 RX
RX DMA Circular
RX IDLE / HT / TC
UART Service RX callback
SPSC RingBuffer
Communication Task sole RX consumer
Platform Notify wake-hint model
```

当前代码中的 TX 真实状态：

```text
platform_uart_write()       EXISTS / blocking
platform_uart_write_async() public abstraction EXISTS
STM32 Impl writeAsync       NOT IMPLEMENTED / Ops slot is NULL
UART Service TX API         NOT IMPLEMENTED
```

2026-09-04 已由人工在 CubeMX 开启 USART1 TX DMA，生成配置：

```text
USART1_TX
DMA2_Stream7
Channel 4
Direction = Memory -> Peripheral
Memory Increment = ENABLE
Peripheral Increment = DISABLE
Data Alignment = BYTE
Mode = NORMAL
Priority = LOW
FIFO = DISABLE
IRQ priority = 5
```

RX 继续：

```text
USART1_RX
DMA2_Stream2
Channel 4
Direction = Peripheral -> Memory
Mode = CIRCULAR
Priority = MEDIUM
IRQ priority = 5
```

因此当前状态应表述为：

```text
RX DMA production path       VERIFIED
TX DMA CubeMX configuration  READY
TX DMA Platform/Service path DESIGN FROZEN / IMPLEMENTATION PENDING
```

不得把“CubeMX 已生成 TX DMA”误写为“TX DMA production path 已验证”。

---

# 3. 架构职责

## 3.1 Platform UART

负责通用 UART 硬件能力：

```text
blocking TX / RX
async TX / RX
cancel TX / RX / BOTH
TX_COMPLETE / RX_DATA / ERROR / CANCELED events
```

Platform 不理解：

```text
START / STOP / ONCE / STATUS / HELP
APP RUNNING / STOPPED
Sensor report
```

## 3.2 UART Service

负责面向 Task 的可靠通信数据服务：

```text
RX DMA stream -> SPSC RingBuffer
RX data-loss detection
Task wakeup
TX DMA transaction lifecycle
timeout / cancel / TX result
transport statistics
```

UART Service 不解释应用命令，不控制 LED / Sensor / APP 状态。

## 3.3 APP Communication

负责产品 UART 文本协议：

```text
byte-stream framing
command parsing
protocol-local response
APP control event conversion
future APP control result -> UART text
future acquisition result -> UART report formatting
```

APP Communication 不维护系统 STOPPED / RUNNING 真值。

---

# 4. Platform UART TX DMA 设计

## 4.1 RX / TX 独立 transaction 状态

UART device lifecycle 与 DMA transaction 状态分离。

```text
Device lifecycle:
CREATED -> INITIALIZED -> STARTED -> STOPPED

RX session:
rxActive = false / true

TX transaction:
txActive = false / true
```

USART 全双工，以下状态合法：

```text
RX active + TX idle
RX idle   + TX active
RX active + TX active
```

禁止用单一 `transferActive` 合并 RX / TX。

STM32 Impl Context 扩展方向：

```text
RX:
rxBuffer
rxBufferSize
rxLastPosition
rxActive

TX:
txBuffer
 txBufferSize
 txActive
```

TX buffer 是 caller-owned non-owning reference；Impl 不复制、不释放。

## 4.2 write_async()

`platform_uart_write_async()` 已有公共 API，Phase 8 在 STM32 Impl 中兑现：

```text
validate object / state / data / length
 -> reject when txActive == true with BUSY
 -> save TX context
 -> txActive = true
 -> HAL_UART_Transmit_DMA()
 -> HAL start failure => rollback TX context
 -> success => return OK
```

成功返回只表示：

```text
DMA TX transaction successfully started
```

不表示发送完成。

## 4.3 Buffer 生命周期

`platform_uart_write_async()` 成功返回后，直到以下任一终止事件：

```text
TX_COMPLETE
CANCELED / TX
ERROR / TX or device-level terminal error
```

调用者必须保证：

```text
buffer remains valid
buffer contents remain unchanged
```

Impl 清理 TX context 后才允许通过 completion / cancel / error 事件归还 buffer 使用权。

## 4.4 TX COMPLETE

HAL callback：

```text
HAL_UART_TxCpltCallback
 -> validate USART1 / txActive
 -> snapshot data pointer + length
 -> clear TX context first
 -> emit PLATFORM_UART_EVENT_TX_COMPLETE / TX
```

必须先清 `txActive` 再通知上层，使 callback 观察到的状态已经是真实 IDLE，允许上层随后启动下一笔 TX。

## 4.5 cancel()

STM32 Impl 当前只支持 RX cancel。Phase 8 补齐：

```text
cancel(RX)
cancel(TX)
cancel(BOTH)
```

原则：

```text
RX -> HAL_UART_AbortReceive + clear RX context
TX -> HAL_UART_AbortTransmit + clear TX context
BOTH -> 分别终止实际 active 的 RX / TX
```

显式 cancel 应按实际方向产生 `CANCELED` 事件；不得留下 DMA 继续访问 caller buffer。

## 4.6 lifecycle stop()

Platform UART lifecycle stop 成功返回后必须保证：

```text
no RX DMA session active
no TX DMA transaction active
no later callback accesses stale Platform callback context
```

lifecycle stop 属于设备整体关闭，不要求把内部 shutdown abort 伪装成业务 `CANCELED` 事件。

## 4.7 Error direction

UART line errors：

```text
PE / FE / NE / ORE -> primarily RX error
```

明确 TX DMA failure -> TX error。

无法可靠归因、影响整个 UART 设备的错误可报告 BOTH / device-level semantics；不得仅凭 `txActive` 武断把所有 HAL error 归因到 TX。

---

# 5. UART Service TX 设计

## 5.1 Service 不机械复制 Platform API

第一版 Service 公共 TX API 只提供 Task 友好的同步语义：

```c
platform_error_t service_uart_write(
    service_uart_t *service,
    const uint8_t *data,
    platform_size_t dataLength,
    uint32_t timeoutMs);
```

内部仍使用：

```text
platform_uart_write_async()
 -> DMA TX
 -> Task blocked
 -> TX_COMPLETE callback
 -> notify
 -> service_uart_write() returns
```

不公开 `service_uart_write_async()`；真正需要完全异步的高级传输策略以后由独立 Queue / Transport 层设计，不塞进基础 UART Service。

## 5.2 ownerThread

现有 `consumerThread` 仅适合 RX-only 语义。Phase 8 将配置语义收束为：

```text
ownerThread = UART Service 所属唯一 Task execution context
```

当前产品中：

```text
ownerThread = Communication Task
```

RX：callback -> RingBuffer -> notify ownerThread。  
TX：ownerThread -> DMA -> wait -> callback -> notify ownerThread。

## 5.3 TX Context

建议最小运行上下文：

```text
txState  = IDLE / ACTIVE
txResult = latest transaction result
```

不把 COMPLETE / TIMEOUT / CANCELED / ERROR 全部建成长期 state；它们属于一次 transaction 的结果。

## 5.4 Service lifecycle 与 TX

`service_uart_write()` 仅在：

```text
SERVICE_UART_STATE_RUNNING
```

合法。

第二笔 TX 在已有 ACTIVE transaction 时：

```text
return PLATFORM_ERR_BUSY
```

不自动排队、不覆盖、不抢占。

`service_uart_stop()` 成功返回后必须保证：

```text
RX stopped
TX stopped
no DMA accesses caller TX buffer
state = STOPPED
```

若 stop 发生时 TX active，应取消 TX；被阻塞的 write 返回 `PLATFORM_ERR_CANCELED`。

`service_uart_deinit()` 不偷偷 stop；仅允许无活动 RX/TX transaction 的 INITIALIZED / STOPPED / 可清理 ERROR 状态。

## 5.5 TX_COMPLETE 是 Service 内部完成条件

Platform 必须公开 `TX_COMPLETE`；UART Service 第一版无需再把它作为公共 `SERVICE_UART_EVENT_TX_COMPLETE` 暴露给 APP。

```text
Platform TX_COMPLETE
 -> Service callback
 -> txState = IDLE
 -> txResult = OK
 -> notify ownerThread
 -> service_uart_write() returns OK
```

APP 只关心 `service_uart_write()` 返回结果。

## 5.6 TX wait 不能复用 service_uart_wait_event()

现有 `service_uart_wait_event()` 会根据 RingBuffer 真值持续重建 `RX_AVAILABLE`。若 TX wait 复用该 API，而 RingBuffer 仍有未读数据，会形成立即返回的 busy loop。

因此 Phase 8 需要内部专用等待逻辑：

```text
service_uart_wait_tx_complete()
```

它只检查：

```text
txState
txResult
ownerThread wake hint
```

不因 RingBuffer 非空而立即结束等待。

## 5.7 Timeout 使用绝对截止语义

无关 RX notification 不能延长 TX 总 timeout。

使用现有：

```c
platform_time_get_ms()
```

按 wraparound-safe elapsed / remaining time 计算总 deadline；禁止每次被 RX 唤醒后重新等待完整 `timeoutMs`。

TX timeout：

```text
deadline reached
 -> platform_uart_cancel(TX)
 -> confirm DMA transaction terminated
 -> txState = IDLE
 -> return PLATFORM_ERR_TIMEOUT
```

核心强保证：

```text
service_uart_write() 一旦返回，DMA 不得继续访问 caller TX buffer。
```

## 5.8 RX / TX error 分离

```text
RX session error
 -> Service state = ERROR
 -> APP Communication existing recovery path

single TX transaction error
 -> txState = IDLE
 -> txResult = underlying error
 -> service_uart_write() returns error
 -> Service can remain RUNNING if RX remains healthy

BOTH / device fatal error
 -> Service state = ERROR
 -> terminate active TX safely
```

RingBuffer data loss 只属于 RX，不主动取消正常 TX。

---

# 6. UART Service Statistics

保留现有 RX statistics，并增加 transport-level TX statistics：

```text
txRequestCount
txCompleteCount
txBytesCompleted
txBusyRejectCount
txTimeoutCount
txErrorCount
txCancelCount
```

不在 UART Service 统计：

```text
START count
STOP count
HELP count
sensor report count
```

这些属于 APP / protocol 语义。

---

# 7. RX 文本协议与 Line Assembler

## 7.1 输入不是 command packet

RingBuffer / `service_uart_read()` 提供任意 byte chunk，读取边界不是命令边界。

必须支持：

```text
"STA" + "RT\r\n"
"START\r\nSTATUS\r\nHELP\r\n" in one read
"START\r" + "\n"
```

数据流：

```text
service_uart_read()
 -> byte-by-byte feed
 -> CRLF Line Assembler
 -> complete line
 -> Command Parser
```

## 7.2 Framing

第一版严格使用：

```text
CRLF = \r\n
```

只接受：

```text
START\r\n
STOP\r\n
ONCE\r\n
STATUS\r\n
HELP\r\n
```

不接受：

```text
START\n
start\r\n
 START\r\n
START \r\n
```

第一版：

```text
case-sensitive
uppercase only
no trim
no arguments
no dynamic allocation
```

## 7.3 Command buffer

产品配置建议新增：

```text
PROJECT_COMM_COMMAND_LINE_BUFFER_SIZE = 32U
```

语义：

```text
31 command chars maximum
1 byte reserved for '\0'
CRLF is not stored
```

## 7.4 APP Communication Context

Line Assembler 运行状态属于 Context：

```text
commandLine[]
commandLength
pendingCr
discardLine
```

命令解析结果是一次瞬时 Data，不保存为长期业务状态。

## 7.5 Malformed / overflow recovery

一条 line 一旦确认 framing 破坏或长度 overflow：

```text
discardLine = true
 -> discard remainder of this line
 -> recover only after next complete CRLF
```

禁止 buffer 满后立即 reset 并从同行剩余字节重新解析，否则恶意/错误长行尾部可能被错误识别为合法 `START`。

空行可直接忽略。

Protocol invalid input 不进入 `APP_COMMUNICATION_STATE_ERROR`。

---

# 8. Command Parser 与统一控制事件

Communication 内部命令可表示：

```text
START
STOP
ONCE
STATUS
HELP
INVALID
```

真正跨 APP 模块的公共合同独立放置，建议：

```text
01_APP/app_control_types.h
```

冻结事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

映射：

```text
UART START  -> APP_CTRL_START
UART STOP   -> APP_CTRL_STOP
UART ONCE   -> APP_CTRL_SAMPLE_ONCE
UART STATUS -> APP_CTRL_GET_STATUS
```

`HELP` 是 Communication-local command，不进入 APP FSM。

Button 后续也映射到相同 `APP_CTRL_*`，APP FSM 不认识 `"START\r\n"` 或 SINGLE / DOUBLE / LONG 等来源细节。

---

# 9. Control Event Outlet

Phase 8 只冻结逻辑事件出口，不冻结 Phase 9 RTOS IPC。

建议抽象：

```c
typedef platform_error_t (*app_control_event_handler_t)(
    void *context,
    app_ctrl_event_t event);
```

`app_communication_config_t` 保存：

```text
controlHandler
controlContext
```

handler 返回值只表示：

```text
control request delivery / submission result
```

不表示业务执行结果。

Phase 8 禁止由此假设：

```text
APP Control 一定同步执行
APP Control 一定有独立 Task
APP Control 一定使用 Queue
```

这些由 Phase 9 冻结。

---

# 10. Response Ownership

## 10.1 Communication-local responses

Phase 8 可直接实现并验证：

```text
HELP START STOP ONCE STATUS HELP\r\n
ERR UNKNOWN_COMMAND\r\n
ERR COMMAND_TOO_LONG\r\n
```

## 10.2 State-dependent responses

以下响应的格式可以定义，但发送条件必须由未来 APP Control FSM 结果决定：

```text
OK START\r\n
OK STOP\r\n
ERR ALREADY_RUNNING\r\n
ERR ALREADY_STOPPED\r\n
STATUS RUNNING\r\n
STATUS STOPPED\r\n
```

Communication 不得通过私有 `running` flag 猜测结果。

`ONCE` 不得在仅“收到命令”时立即认定业务成功；最终成功仍要求完整 acquisition + UART report 成功，具体 result/response 留到 Phase 10。

---

# 11. USART1 业务 TX 所有权

第一阶段冻结：

```text
Communication Task = sole USART1 product TX requester
```

未来 Acquisition Task 不直接调用 `service_uart_write()`：

```text
Acquisition Task
 -> acquisition result IPC
 -> Communication Task
 -> report formatting
 -> service_uart_write()
```

这样天然串行：

```text
command response
sensor report
HELP / STATUS response
```

当前不增加 TX mutex / TX queue / TX worker task。

---

# 12. USART1 / RTT 通道隔离

正式职责：

```text
USART1 -> product control + product data
RTT    -> diagnostics / EasyLogger
```

当前 CubeMX USER CODE 中仍存在旧：

```text
printf -> fputc -> HAL_UART_Transmit(&huart1)
uartMutexHandle / USART1_mutex_Init()
```

Phase 8 production implementation 应移除该 USART1 debug bypass 或确保其彻底退出正式运行路径。

禁止形成：

```text
UART Service -> DMA TX
+
printf -> direct HAL_UART_Transmit
```

两个独立 owner 同时访问同一 `huart1`。

---

# 13. APP Communication Statistics

保留现有：

```text
processedChunkCount
processedByteCount
dataLossRecoveryCount
uartErrorRecoveryCount
fatalErrorCount
```

建议增加：

```text
commandReceivedCount
commandInvalidCount
commandOverflowCount
controlEventSubmittedCount
controlEventSubmitFailureCount
localResponseCount
localResponseFailureCount
```

不机械增加每条命令单独计数。

---

# 14. Phase 8 测试合同

## 14.1 Host / contract tests

至少覆盖：

```text
exact START / STOP / ONCE / STATUS / HELP
unknown command
fragmented command across multiple reads
multiple commands in one read
CRLF split across reads
empty line
strict LF-only rejection
case / whitespace rejection
max line boundary
overflow discard and recovery
malformed CR recovery
control event exactly once
HELP local handling
invalid / overflow does not enter APP fatal ERROR
TX busy rejection
TX complete
TX error
TX timeout + cancel
TX caller buffer lifetime contract
RX remains functional while TX active
existing RX / RingBuffer recovery regression
```

Host tests不得为了覆盖率引入与 production 无关的大型 fake framework；沿用当前项目契约测试风格。

## 14.2 Keil

验收：

```text
0 compile errors
no new UART production warnings
no dependency violation
```

## 14.3 Target board smoke

PC Serial Assistant 至少确认：

```text
HELP response via DMA TX
unknown / overflow response
fragmented commands are parsed correctly
back-to-back commands are parsed correctly
TX DMA complete does not break RX DMA
RX command can arrive while TX is active and is processed after owner Task resumes
```

逻辑分析 / debugger 可辅助确认：

```text
USART1 TX uses DMA2_Stream7
USART1 RX remains DMA2_Stream2 Circular
no direct production fputc bypass
```

临时 Phase 8 smoke 完成后必须删除，恢复 normal production startup 并重新 build。

---

# 15. Phase 8 不解决的问题

Phase 8 结束时允许存在：

```text
START / STOP / ONCE / STATUS parsed and submitted
but no production APP Control consumer yet
```

这是预期阶段边界，不是缺陷。

Phase 9 冻结：

```text
Control Event RTOS IPC
Button permanent execution context
Acquisition result -> Communication IPC
Indicator event delivery
Task priority / stack / buffering
Unified Acquisition Service / Task
```

Phase 10 冻结并实现：

```text
STOPPED / RUNNING FSM
START / STOP / ONCE / STATUS business result
final response semantics
2 s periodic acquisition/report
ONCE success LED feedback
```

---

# 16. 最终冻结结论

Phase 8 第一版：

```text
RX:
DMA Circular
 -> RingBuffer
 -> strict CRLF parser
 -> APP_CTRL event outlet

TX:
Communication Task
 -> UART Service synchronous Task API
 -> Platform UART async API
 -> DMA Normal
 -> TX_COMPLETE

Ownership:
Communication Task sole product TX requester
UART Service owns transport transaction semantics
APP Control FSM owns STOPPED / RUNNING truth
RTT owns diagnostics
```

不引入：

```text
TX RingBuffer
TX Queue
TX worker Task
UART command semantics inside UART Service
APP running state inside Communication
multiple UART TX owners
```

状态：`FROZEN FOR IMPLEMENTATION`。
