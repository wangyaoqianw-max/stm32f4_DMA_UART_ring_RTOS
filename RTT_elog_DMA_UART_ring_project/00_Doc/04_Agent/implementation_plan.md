# UART Application Communication Phase 8 Implementation Plan

> 当前执行计划 / Current Active Plan  
> 状态：READY FOR IMPLEMENTATION / NOT STARTED  
> 日期：2026-09-04

**Goal:** 在保持现有 UART DMA RX + RingBuffer 稳定链路的基础上，补齐可复用 USART1 TX DMA 能力，并让 `app_communication` 完成严格 CRLF 文本命令解析、Communication-local 响应和统一 APP Control Event 出口。

**Architecture:** Platform UART 负责完整 UART 硬件能力；STM32 Impl 实现 RX Circular DMA 与 TX Normal DMA；UART Service 负责 RX RingBuffer 和面向 Task 的可靠 TX transaction；APP Communication 负责 UART 文本协议。Communication Task 是 USART1 唯一产品 TX requester，APP Control FSM 仍留到 Phase 10。

**Tech Stack:** STM32F411CEU6、STM32 HAL、DMA2、CMSIS-RTOS2 / FreeRTOS、Platform OS、SPSC RingBuffer、Keil MDK、Host Contract Tests。

**Spec:** `00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md`

## Global Constraints

```text
No production implementation has started when this plan is created.
APP -> Impl FORBIDDEN.
Service -> Impl FORBIDDEN.
UART Service must not interpret START / STOP / ONCE / STATUS / HELP business semantics.
APP Communication must not own STOPPED / RUNNING truth.
Communication Task is the sole USART1 product TX requester.
RX remains DMA Circular + RingBuffer SPSC.
TX uses DMA Normal, one active transaction only.
No TX RingBuffer / TX Queue / TX worker Task in Phase 8.
No permanent Phase 9 IPC design in Phase 8.
No Acquisition Service / APP Control FSM implementation in Phase 8.
USART1 is product communication; RTT / EasyLogger is diagnostics.
```

---

# 1. Current Hardware / Generated Baseline

CubeMX 已由人工完成 USART1 TX DMA 配置：

```text
RX: DMA2_Stream2 / Channel 4 / Peripheral->Memory / Circular / IRQ 5
TX: DMA2_Stream7 / Channel 4 / Memory->Peripheral / Normal / IRQ 5
```

当前代码状态：

```text
Platform public write_async()                 EXISTS
Platform TX_COMPLETE event type              EXISTS
STM32 Impl writeAsync Ops                    NULL / NOT IMPLEMENTED
STM32 HAL TxCplt -> Platform event            NOT IMPLEMENTED
Platform cancel TX/BOTH                       NOT IMPLEMENTED in STM32 Impl
UART Service TX                               NOT IMPLEMENTED
APP Communication command parser              NOT IMPLEMENTED
```

不得把 TX DMA 配置 READY 误标为 production TX DMA VERIFIED。

---

# 2. Task 1 — STM32 UART TX DMA Contract Tests

**Files:**

```text
Tests/... existing UART Platform / Impl test location
04_Impl/impl_mcu/impl_platform_uart.c        later production target
03_Platform/platform_mcu/uart/platform_uart.* existing contract
```

先增加最小失败测试，冻结：

```text
write_async starts one TX
second TX while active -> BUSY
TX complete releases transaction
TX cancel releases transaction
RX + TX may be active concurrently
lifecycle stop leaves no TX transaction active
```

不得为了测试引入 production-only fake abstraction。

验收：测试先 RED，原因明确来自 STM32 Impl 尚未提供 async TX。

---

# 3. Task 2 — STM32 UART Async TX Implementation

**Modify:**

```text
04_Impl/impl_mcu/impl_platform_uart.c
```

实现：

```text
TX context: caller buffer pointer / length / txActive
stm32_uart_write_async()
g_stm32UartOps.writeAsync binding
HAL_UART_Transmit_DMA()
HAL_UART_TxCpltCallback()
PLATFORM_UART_EVENT_TX_COMPLETE / TX
```

关键顺序：

```text
HAL Tx complete
 -> snapshot data pointer / length
 -> clear TX context
 -> notify Platform event
```

失败启动必须 rollback TX context。

验证 Task 1 测试 GREEN。

---

# 4. Task 3 — Platform UART TX Cancel / Lifecycle Completion

**Modify:**

```text
04_Impl/impl_mcu/impl_platform_uart.c
```

补齐：

```text
platform cancel direction TX
platform cancel direction BOTH
HAL_UART_AbortTransmit()
RX / TX independent context cleanup
lifecycle stop terminates active RX and TX
lifecycle deinit starts from clean transaction state
```

显式 cancel 产生正确 direction 的 CANCELED event；device lifecycle stop 不制造无意义业务 cancel event。

测试：

```text
cancel TX
cancel BOTH with RX+TX active
stop while TX active
buffer is no longer owned after termination
```

---

# 5. Task 4 — UART Error Direction Review

**Modify as needed:**

```text
04_Impl/impl_mcu/impl_platform_uart.c
```

保持规则：

```text
PE / FE / NE / ORE -> RX-oriented error
explicit TX DMA failure -> TX error
unreliable device-wide failure -> device/BOTH semantics
```

禁止“只要 txActive 就把 HAL ErrorCallback 当 TX error”。

运行 UART Platform / Impl regression。

---

# 6. Task 5 — UART Service TX Data Model

**Modify:**

```text
02_Service/service_uart/service_uart.h
02_Service/service_uart/service_uart.c
01_APP/app_system.c                  config field rename/wiring only
```

将 RX-only `consumerThread` 语义升级为：

```text
ownerThread = UART Service sole Task execution context
```

增加最小 TX Context：

```text
txState = IDLE / ACTIVE
txResult
```

增加 TX statistics：

```text
txRequestCount
txCompleteCount
txBytesCompleted
txBusyRejectCount
txTimeoutCount
txErrorCount
txCancelCount
```

不建立 TX queue / buffer pool / public async Service API。

---

# 7. Task 6 — UART Service Synchronous Task TX API

**Produce interface:**

```c
platform_error_t service_uart_write(
    service_uart_t *service,
    const uint8_t *data,
    platform_size_t dataLength,
    uint32_t timeoutMs);
```

内部：

```text
require Service RUNNING
require TX IDLE
 -> txState ACTIVE
 -> platform_uart_write_async()
 -> wait for internal TX terminal condition
 -> return OK / TIMEOUT / CANCELED / underlying TX error
```

强保证：

```text
service_uart_write() returns
=> DMA no longer accesses caller TX buffer
```

TX single transaction error 不自动把 healthy RX Service 设为 ERROR。

---

# 8. Task 7 — Dedicated TX Wait / Timeout Semantics

**Modify:**

```text
02_Service/service_uart/service_uart.c
```

不得使用公共 `service_uart_wait_event()` 等待 TX completion，因为 RingBuffer 非空会持续重建 `RX_AVAILABLE`。

实现内部专用等待逻辑，使用：

```text
platform_notify_wait()
platform_time_get_ms()
```

要求：

```text
Notify only wake hint
TX Context is truth
unrelated RX wakeup must not extend total TX timeout
wraparound-safe elapsed / remaining timeout
```

timeout：

```text
cancel TX
 -> confirm transaction ended
 -> return PLATFORM_ERR_TIMEOUT
```

测试必须覆盖 RX notification 干扰 TX timeout 的场景。

---

# 9. Task 8 — UART Service Stop / Error Integration

**Modify:**

```text
02_Service/service_uart/service_uart.c/.h
```

冻结行为：

```text
stop while TX idle      -> stop RX -> STOPPED
stop while TX active    -> cancel TX + stop RX -> STOPPED
blocked write on stop   -> CANCELED
TX transaction error    -> write returns error; RX may remain RUNNING
RX session error        -> Service ERROR; APP Communication recovery path
BOTH/device fatal       -> Service ERROR + terminate active TX safely
```

确保 `deinit()` 不偷偷承担 `stop()` 语义。

运行现有 RX Service regression，确认 SPSC / data-loss recovery 未回归。

---

# 10. Task 9 — Remove USART1 Debug TX Bypass

**Generated/User Code files, only permitted USER CODE regions:**

```text
Core/Src/usart.c
Core/Inc/usart.h
related startup call site if USART1_mutex_Init() is still invoked
```

移除正式运行路径中的：

```text
uartMutexHandle
USART1_mutex_Init()
printf/fputc -> HAL_UART_Transmit(&huart1)
```

目标：

```text
USART1 = product communication only
RTT / EasyLogger = diagnostics only
```

不得修改 CubeMX 自动生成区的无关内容。

---

# 11. Task 10 — APP Control Event Common Contract

**Create:**

```text
01_APP/app_control_types.h
```

冻结：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

该文件不包含 UART 文本、Button gesture 或 RTOS Queue 细节。

为 Phase 8 提供逻辑 event outlet；实际 permanent consumer / Queue 在 Phase 9 冻结。

---

# 12. Task 11 — APP Communication Line Assembler Tests

**Test target:** `app_communication`

先增加失败测试：

```text
START / STOP / ONCE / STATUS / HELP
fragmented START
multiple commands in one chunk
CRLF split
LF-only reject
lowercase reject
leading/trailing whitespace reject
empty line ignore
line boundary
line overflow discard until CRLF
malformed CR recovery
invalid input does not enter APP fatal ERROR
```

新增产品配置计划：

```text
PROJECT_COMM_COMMAND_LINE_BUFFER_SIZE = 32U
```

31 chars usable + one NUL；CRLF 不存入 line buffer。

---

# 13. Task 12 — APP Communication Parser / Context

**Modify:**

```text
01_APP/app_communication.h
01_APP/app_communication.c
00_Config/project_config.h
```

Context 增加：

```text
commandLine
commandLength
pendingCr
discardLine
```

`app_communication_drain_rx()` 从“读取并丢弃”升级为：

```text
service_uart_read
 -> feed bytes
 -> line assembler
 -> parse complete line
```

Command enum 可保持 Communication 私有。

Protocol invalid input 属于正常外部输入，不进入 `APP_COMMUNICATION_STATE_ERROR`。

---

# 14. Task 13 — Command -> APP Control Event Outlet

**Modify:**

```text
01_APP/app_communication.h/.c
01_APP/app_system.c
```

Config 增加逻辑 handler：

```text
controlHandler
controlContext
```

映射：

```text
START  -> APP_CTRL_START
STOP   -> APP_CTRL_STOP
ONCE   -> APP_CTRL_SAMPLE_ONCE
STATUS -> APP_CTRL_GET_STATUS
HELP   -> Communication local
```

Phase 8 测试使用 test sink / minimal non-production adapter 验证 exactly-once submission。

不得在 `app_communication` 增加 systemRunning / STOPPED / RUNNING truth。

handler 返回值只表示 request submission result，不代表业务执行结果。

---

# 15. Task 14 — Communication-local TX Responses

**Modify:**

```text
01_APP/app_communication.c
```

通过唯一正式链：

```text
app_communication
 -> service_uart_write
 -> Platform async TX
 -> DMA2_Stream7
```

Phase 8 可直接响应：

```text
HELP START STOP ONCE STATUS HELP\r\n
ERR UNKNOWN_COMMAND\r\n
ERR COMMAND_TOO_LONG\r\n
```

状态相关文本格式可定义，但不能伪造业务执行：

```text
OK START
OK STOP
ERR ALREADY_RUNNING
ERR ALREADY_STOPPED
STATUS RUNNING / STOPPED
```

这些发送条件等待 Phase 10 APP FSM result。

`ONCE` 不得在 request submission 时提前报告完整业务成功。

---

# 16. Task 15 — APP Communication Statistics

增加：

```text
commandReceivedCount
commandInvalidCount
commandOverflowCount
controlEventSubmittedCount
controlEventSubmitFailureCount
localResponseCount
localResponseFailureCount
```

不复制 UART Service transport statistics，不做每条命令独立计数。

---

# 17. Task 16 — Host Regression / Architecture Review

至少确认：

```text
all existing tests PASS
new UART TX tests PASS
new parser tests PASS
RX SPSC remains lock-free
no Service -> Impl
no APP -> Impl
no command semantics in UART Service
no APP running truth in Communication
no TX queue / mutex / second TX owner
no dynamic allocation
```

编码阶段必须完整读取：

```text
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
```

并执行 Coding Standard Review。

---

# 18. Task 17 — Keil / Target Smoke

Keil：

```text
0 compile errors
no new Phase 8 production warnings
```

目标板 PC 串口助手：

```text
HELP response uses TX DMA
unknown / overflow response correct
fragmented command correct
multiple commands correct
RX continues while TX DMA active
command arriving during TX is retained by RX DMA + RingBuffer
```

必要时用 debugger / logic analyzer 确认：

```text
TX = DMA2_Stream7 Normal
RX = DMA2_Stream2 Circular
```

RTT 只记录必要 init / complete command / error 摘要，不逐 byte / DMA step 刷日志。

---

# 19. Task 18 — Cleanup / Final Documentation

目标板验证后：

```text
remove temporary Phase 8 smoke harness
restore normal production startup
Keil production rebuild
update implementation_plan execution record
update handoff
update roadmap Phase 8 status
Coding Standard Review recorded
```

只有真实 Host + Keil + target evidence 后，Phase 8 才能标记 COMPLETED。

---

# 20. Stop Point

Phase 8 完成后停止，不自动实现：

```text
Phase 9 permanent Task / IPC
Unified Acquisition Service / Task
APP Control FSM
final state-dependent UART responses
2 s integrated report flow
ONCE integrated LED feedback
```

当前计划状态：

```text
READY FOR IMPLEMENTATION / NOT STARTED
```
