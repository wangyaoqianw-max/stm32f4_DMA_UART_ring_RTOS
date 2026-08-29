# Current Implementation Plan

## Metadata

- Status: READY_FOR_IMPLEMENTATION
- Phase: UART Phase 2A
- Scope: USART1 DMA RX + IDLE + Platform RX_DATA Event
- Architecture Version: 1.1
- Target: STM32F411CEU6 / USART1 / DMA2 Stream2 Channel4 / FreeRTOS / Keil
- Updated: 2026-08-29
- Design Spec: `00_Doc/02_架构设计/UART_Phase2A_DMA_RX设计.md`

---

## 1. Goal

建立并验证：

```text
USART1 RX
   ↓
DMA2 Stream2 / Channel 4
   ↓
Circular DMA Buffer
   ↓
IDLE / HT / TC
   ↓
STM32 UART Impl
   ↓
Platform UART RX_DATA Event
```

Phase 2A 只负责硬件字节流采集和 Platform 事件上报。

本阶段结束时应能够持续接收不定长 UART 数据，包括超过一次 DMA Buffer 容量的连续数据流。

---

## 2. Preconditions

UART Phase 1 已完成：

- Blocking TX/RX 板测 PASS。
- Lifecycle 板测 PASS。
- 临时测试代码已恢复。
- 恢复后 Keil Full Rebuild `0 Error(s)`。

CubeMX Phase 2A 配置已经生成并确认：

```text
USART1_RX            DMA2 Stream2 / Channel 4
Direction            Peripheral -> Memory
Peripheral Inc       Disable
Memory Inc           Enable
Peripheral Width     Byte
Memory Width         Byte
Mode                 Circular
Priority             Medium
FIFO                 Disable
USART1_IRQn           5 / 0
DMA2_Stream2_IRQn    5 / 0
```

`main.c` 初始化顺序：

```text
MX_GPIO_Init()
MX_DMA_Init()
MX_USART1_UART_Init()
```

---

## 3. Frozen Architecture Decisions

### 3.1 HAL API

使用：

```c
HAL_UARTEx_ReceiveToIdle_DMA()
```

保留：

```text
HT + TC + IDLE
```

全部转换为 Platform：

```text
PLATFORM_UART_EVENT_RX_DATA
```

不得向 Platform 暴露 DMA Stream、HT、TC、IDLE 或 HAL 类型。

### 3.2 Existing Platform API

保留现有：

```c
platform_error_t platform_uart_read_async(
    platform_uart_t *uart,
    uint8_t *buffer,
    platform_size_t bufferSize);
```

不得修改函数签名。

`read_async()` 在 Phase 2A 中表示启动持续 RX Session。

`RX_DATA` 只是新增字节事件，不结束 RX Session。

### 3.3 Buffer Ownership — Scheme A

```text
Storage owner              Caller
DMA control owner          STM32 UART Impl
RX Session writer          DMA / Impl
RX_DATA consumer           callback read-only
```

调用者提供静态 DMA Buffer。

RX Session 活跃期间调用者不得修改、释放或复用该 Buffer。

Buffer lease 在以下情况结束：

```text
cancel(RX)
RX error
lifecycle stop
```

`RX_DATA.event.data` 只保证在 callback 执行期间可访问。

### 3.4 DMA Buffer Baseline

Phase 2A 板测固定：

```text
256 bytes
```

该值属于当前 Impl / Integration Test 基准，不加入 Platform UART 公共配置。

---

## 4. Scope Guard

Phase 2A 允许修改：

```text
04_Impl/impl_mcu/impl_platform_uart.c
04_Impl/impl_mcu/impl_platform_uart.h   （仅必要的 Impl 私有声明）
Tests/impl_platform_uart/*
Core/Src/freertos.c                     （仅临时 Board Test USER CODE）
00_Doc/04_Agent/handoff.md
```

CubeMX 已生成且只应作为基础硬件资源的文件：

```text
RTT_elog_DMA_UART_ring_project.ioc
Core/Src/dma.c
Core/Inc/dma.h
Core/Src/usart.c
Core/Inc/usart.h
Core/Src/stm32f4xx_it.c
Core/Inc/stm32f4xx_it.h
Core/Src/main.c
```

不要手写覆盖 CubeMX 生成的 DMA/NVIC 配置。

本阶段禁止：

```text
UART Service
Ring Buffer
FreeRTOS Notification
Communication Task
Protocol Parser
DMA TX
Async TX
通用 DMA Platform Framework
impl_dma 通用抽象
Vendor HAL 修改
Platform UART API Redesign
动态内存
```

---

## 5. STM32 Impl Runtime Context

扩展现有私有 Context，保持最小状态：

```text
stm32_uart_impl_context_t
├── UART_HandleTypeDef *halUart
├── uint8_t *rxBuffer
├── platform_size_t rxBufferSize
├── platform_size_t rxLastPosition
└── bool rxActive
```

不得把 RingBuffer、Task Handle、Queue、Mutex 或协议状态加入该 Context。

---

## 6. Task 1 — Host Test Baseline

### Files

Modify:

```text
Tests/impl_platform_uart/test_impl_platform_uart.c
Tests/impl_platform_uart/usart.h
```

### Required Fake HAL Capability

Host stub 至少需要覆盖：

```text
HAL_UARTEx_ReceiveToIdle_DMA
HAL_UART_AbortReceive
HAL UART ErrorCode
HAL UART RxState / DMA-related minimum fields needed by Impl
```

现有 config mapping tests 必须继续 PASS。

### Required New Tests

先写失败测试，再实现代码：

1. `readAsync` 成功时调用 `HAL_UARTEx_ReceiveToIdle_DMA()`，保存 Buffer/Size，进入 `rxActive`。
2. 第二次 `readAsync` 返回 `PLATFORM_ERR_BUSY`。
3. `bufferSize > 0xFFFF` 返回 `PLATFORM_ERR_OVERFLOW`，不调用 HAL。
4. HAL 启动失败时不保留 active session。
5. `Pos > lastPosition` 只上报新增区间。
6. `Pos == lastPosition` 不重复上报。
7. Wrap 情况分成尾部和头部两个连续 `RX_DATA` Event。
8. `Pos == bufferSize` 正确处理 TC 边界并归一化位置。
9. `cancel(RX)` 调用 `HAL_UART_AbortReceive()`、清理 session 并产生 CANCELED Event。
10. 无 active RX 时 `cancel(RX)` 返回 `PLATFORM_ERR_INVALID_STATE`。
11. `cancel(TX)` 返回 `PLATFORM_ERR_NOT_SUPPORTED`。
12. ORE 映射 `PLATFORM_ERR_OVERFLOW`。
13. DMA/PE/NE/FE 映射 `PLATFORM_ERR_IO`。
14. Error 后 session 被释放。
15. lifecycle stop 在 active RX 时先 Abort，成功后才切换 STOPPED。

所有 Host Test 必须使用现有 Platform Event 入口，不为测试增加新的公共生产 API。

---

## 7. Task 2 — Implement Continuous DMA RX Session

### File

```text
04_Impl/impl_mcu/impl_platform_uart.c
```

### Ops Table

将：

```text
readAsync = NULL
cancel    = NULL
```

替换为 STM32 Phase 2A 实现。

`writeAsync` 保持 `NULL`。

### readAsync Required Behavior

执行顺序：

```text
validate context
    ↓
validate buffer / size
    ↓
size <= 0xFFFF
    ↓
rxActive == false
    ↓
record session fields
    ↓
HAL_UARTEx_ReceiveToIdle_DMA()
```

返回映射：

```text
HAL_OK      -> PLATFORM_ERR_OK
HAL_BUSY    -> PLATFORM_ERR_BUSY
HAL_TIMEOUT -> PLATFORM_ERR_TIMEOUT
HAL_ERROR   -> PLATFORM_ERR_IO
```

HAL 启动失败时必须恢复：

```text
rxBuffer       = NULL
rxBufferSize   = 0
rxLastPosition = 0
rxActive       = false
```

---

## 8. Task 3 — RxEvent Position Processing

### HAL Callback

由于当前：

```text
USE_HAL_UART_REGISTER_CALLBACKS = 0
```

在自定义 Impl 源文件实现 weak callback override：

```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart,
                                uint16_t Pos);
```

只处理：

```text
huart == &huart1
```

其他 UART 直接返回。

### Position Algorithm

令：

```text
N    = rxBufferSize
last = rxLastPosition
pos  = Pos
```

规则：

```text
pos > N
    -> internal error guard，不访问 Buffer

pos == last
    -> no new data, ignore

pos > last
    -> emit [last, pos)

pos < last
    -> emit [last, N)
    -> emit [0, pos)
```

如果：

```text
pos == N
```

尾部事件完成后：

```text
rxLastPosition = 0
```

否则：

```text
rxLastPosition = pos
```

不得构造跨 Buffer 尾首的单一 Event。

每个 `RX_DATA`：

```text
type      = RX_DATA
direction = RX
data       = pointer to contiguous new segment
dataLength = segment length
error      = PLATFORM_ERR_OK
```

零长度片段不发送。

---

## 9. Task 4 — Error / Cancel / Stop

### HAL Error Callback

实现：

```c
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);
```

只处理 USART1 active RX session。

映射：

```text
HAL_UART_ERROR_ORE -> PLATFORM_ERR_OVERFLOW
HAL_UART_ERROR_DMA -> PLATFORM_ERR_IO
HAL_UART_ERROR_PE  -> PLATFORM_ERR_IO
HAL_UART_ERROR_NE  -> PLATFORM_ERR_IO
HAL_UART_ERROR_FE  -> PLATFORM_ERR_IO
```

ERROR Event：

```text
type       = ERROR
direction  = RX
data        = NULL
dataLength  = 0
error       = mapped error
```

Error 后清理 RX session，不在 ISR 内自动重启 DMA。

### cancel(RX)

活动 session：

```text
HAL_UART_AbortReceive()
    ↓
clear RX context
    ↓
CANCELED / RX event
```

无活动 session：

```text
PLATFORM_ERR_INVALID_STATE
```

TX：

```text
PLATFORM_ERR_NOT_SUPPORTED
```

### lifecycle stop

active RX 时：

```text
AbortReceive
    ↓ only if success
clear RX context
    ↓
state -> STOPPED
```

stop 不主动产生 CANCELED Event。

Abort 失败时保持 STARTED，返回底层映射错误。

---

## 10. Task 5 — Platform Regression

不得修改 Platform API 签名。

重新运行现有：

```text
Tests/platform_uart
Tests/impl_platform_uart
Tests/platform_log
```

要求原有 Phase 1 Blocking 行为继续 PASS。

重点确认：

- Blocking TX 不回归。
- Blocking RX 在没有 active DMA RX 时仍正常。
- `platform_uart_read_async()` 仍要求 UART STARTED + callback 非 NULL。
- `platform_uart_notify_event()` 仍校验 RX_DATA / ERROR / CANCELED。

---

## 11. Task 6 — Keil Integration

执行：

```text
Clean Targets
Rebuild all target files
```

要求：

```text
0 Error(s)
```

不得为了消除已有 Warning 修改无关模块。

若再次出现：

```text
C4051E
L6449E
Invalid argument
```

按 Keil 输出目录 / 环境 I/O 问题处理，不修改 UART 逻辑绕过。

---

## 12. Task 7 — Board Smoke Test

只允许在：

```text
Core/Src/freertos.c USER CODE
```

加入临时测试。

使用：

```text
static platform_uart_t UART object
static uint8_t DMA RX buffer[256]
static capture/status storage
```

测试 callback 可能运行在 ISR：

- 不打印日志；
- 不阻塞；
- 不使用普通 Mutex；
- 只复制必要测试数据、更新计数/flag。

由 Task Context 根据 flag 输出 RTT 结果。

### Scenario A — Short / IDLE

发送：

```text
HELLO
```

停止发送。

验证累计数据：

```text
length = 5
content = HELLO
```

### Scenario B — Continuous > Buffer

连续发送至少：

```text
600 bytes
```

使用已知递增/重复 Pattern。

验证：

- total length = expected；
- byte-by-byte pattern correct；
- >256 bytes 后仍持续接收；
- no duplicate；
- no loss；
- order correct。

### Scenario C — Mixed Boundaries

使用会跨过：

```text
128-byte HT
256-byte TC
IDLE
```

的数据块组合。

验证累计结果与发送端完全一致。

### Scenario D — Cancel + Restart

```text
readAsync
receive
cancel(RX)
expect CANCELED
readAsync again
receive again
```

### Scenario E — Lifecycle Stop + Restart

active RX 时：

```text
stop
start
readAsync again
```

确认旧 DMA Session 不继续产生事件，新 Session 可正常收数。

---

## 13. Temporary Test Restore

全部板测通过后：

- 恢复 `freertos.c` 正常周期日志任务；
- 不保留 Board Smoke Test 业务代码；
- 再次 Keil Full Rebuild。

要求：

```text
0 Error(s)
```

最终提交状态必须基于恢复后的正常代码。

---

## 14. Completion Criteria

只有以下全部有真实证据才可写：

```text
UART Phase 2A = COMPLETED
```

条件：

1. CubeMX DMA / IRQ 配置与本计划一致。
2. Host tests 全部 PASS。
3. `readAsync()` 启动 Circular ReceiveToIdle DMA PASS。
4. Short + IDLE PASS。
5. Continuous >256 bytes PASS。
6. HT / TC / IDLE 混合边界无丢失、无重复、顺序正确。
7. Cancel + Restart PASS。
8. Lifecycle Stop + Restart PASS。
9. Error path 至少由 Host Test 覆盖。
10. Platform Phase 1 regression PASS。
11. 临时测试代码已恢复。
12. 恢复后的 Keil Full Rebuild 为 `0 Error(s)`。
13. `handoff.md` 更新真实结果。

无硬件验证时只能标记：

```text
CODE_COMPLETE_PENDING_HARDWARE_VERIFICATION
```

需要修改冻结 Platform UART API 时：

```text
BLOCKED
```

停止实现并重新设计。

---

## 15. After Phase 2A

只有 Phase 2A 完成后才进入 Phase 2B：

```text
Platform RX_DATA Callback
       ↓
UART Service
       ↓
Ring Buffer
       ↓
ISR-safe Notification
       ↓
Communication Task
```

不得在 Phase 2A 中提前实现 Phase 2B。