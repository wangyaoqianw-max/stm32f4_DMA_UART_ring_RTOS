# UART Phase 2A — DMA RX + IDLE 设计

> 状态：APPROVED / FROZEN FOR IMPLEMENTATION  
> 日期：2026-08-29  
> 目标平台：STM32F411CEU6 / USART1 / STM32 HAL / FreeRTOS  
> 前置阶段：UART Phase 1 Blocking — COMPLETED

---

## 1. 阶段目标

Phase 2A 只建立以下链路：

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

本阶段证明 Platform UART 能够持续、无固定长度地向上提供新增字节数据。

本阶段不引入 UART Service、Ring Buffer、FreeRTOS Notification、协议解析和 DMA TX。

---

## 2. 已冻结的硬件配置

CubeMX 当前已经生成：

```text
USART1
TX                  PA9
RX                  PA10
Baud                115200
Data                8 bit
Parity              None
Stop                1 bit
Flow Control        None

USART1_RX DMA       DMA2 Stream2 / Channel 4
Direction           Peripheral -> Memory
Peripheral Inc      Disabled
Memory Inc          Enabled
Peripheral Width    Byte
Memory Width        Byte
Mode                Circular
Priority            Medium
FIFO                Disabled

USART1_IRQn         Preemption 5 / Sub 0
DMA2_Stream2_IRQn   Preemption 5 / Sub 0
```

`main.c` 初始化顺序保持：

```text
MX_GPIO_Init()
MX_DMA_Init()
MX_USART1_UART_Init()
```

不得在 Phase 2A 中切换到 DMA Normal Mode、其他 Stream/Channel 或 TX DMA。

---

## 3. HAL 接收策略

STM32 Impl 使用：

```c
HAL_UARTEx_ReceiveToIdle_DMA(
    &huart1,
    rxBuffer,
    rxBufferSize);
```

DMA 保持 Circular Mode。

接收事件来源保留：

```text
HT   Half Transfer
TC   Transfer Complete
IDLE UART line idle
```

三种底层触发全部统一进入 HAL `HAL_UARTEx_RxEventCallback()`，Platform 不暴露 HT / TC / IDLE 的区别。

上层只看到：

```text
PLATFORM_UART_EVENT_RX_DATA
```

原因：Platform 的职责是提供新增字节流，而不是暴露 STM32 DMA 中断来源。

---

## 4. Platform API 决策

不修改现有公共 API：

```c
platform_error_t platform_uart_read_async(
    platform_uart_t *uart,
    uint8_t *buffer,
    platform_size_t bufferSize);
```

Phase 2A 将 `read_async()` 定义为：

> 启动一个持续 RX Session，而不是提交一次“收到数据即结束”的单次异步读取。

一次成功的 `platform_uart_read_async()`：

```text
STARTED
   ↓
start RX session
   ↓
RX_DATA
RX_DATA
RX_DATA
...
   ↓
cancel / error / lifecycle stop
   ↓
RX session ends
```

在 RX Session 活跃期间再次调用 `platform_uart_read_async()`：

```text
PLATFORM_ERR_BUSY
```

`RX_DATA` 事件只表示“有新的 UART 字节可处理”，不表示：

- 一个完整协议帧；
- 一次业务消息完成；
- RX Session 结束；
- Buffer 所有权已经归还。

---

## 5. Buffer Ownership — 方案 A

本设计正式冻结方案 A。

### 5.1 Storage Owner

DMA RX Buffer 的内存存储由调用者提供并长期持有。

推荐 Phase 2A 板测：

```c
static uint8_t s_uartRxDmaBuffer[256];
```

Platform 和 Impl：

- 不动态分配 DMA RX Buffer；
- 不复制整个 DMA Buffer；
- 不释放调用者 Buffer。

### 5.2 RX Session Lease

`platform_uart_read_async()` 成功后，调用者将该 Buffer 的独占 DMA 使用权临时借给 Impl。

RX Session 期间：

```text
Memory storage owner      Caller
DMA control owner         STM32 UART Impl
Buffer writer             DMA / Impl only
RX_DATA consumer          Callback, read-only
```

调用者不得：

- 修改 Buffer；
- 清零 Buffer；
- 改变 Buffer 存储位置；
- 释放或复用 Buffer。

直到以下任一事件终止 RX Session：

```text
platform_uart_cancel(RX)
HAL RX error
lifecycle stop
```

### 5.3 Event Data Lifetime

`RX_DATA.event.data` 指向 DMA RX Buffer 中本次新增的连续片段。

其内容只保证：

> 在当前 callback 执行期间有效且稳定到足以被立即消费/复制。

Callback 返回后 DMA 仍继续运行，上层不得长期保存该指针。

后续 Phase 2B 的 UART Service 必须在 callback 中将新增字节复制到 Ring Buffer，再通过 ISR-safe 机制通知任务。

### 5.4 对基础架构文档的澄清

基础 `architecture.md` 中“DMA Buffer 属于 UART Impl”应理解为：

> DMA Buffer 的硬件控制、活动接收上下文、位置状态和访问纪律属于 UART Impl。

Phase 2A 不再将其解释为“Buffer 的静态存储必须由 Impl 自己声明”。

本文件对 Phase 2A 的 Buffer Ownership 语义具有优先解释权。

---

## 6. STM32 UART Impl Runtime Context

现有 USART1 Impl Context 在 Phase 2A 增加 RX Session 状态，最小模型：

```text
stm32_uart_impl_context_t
├── UART_HandleTypeDef *halUart
├── uint8_t *rxBuffer
├── platform_size_t rxBufferSize
├── platform_size_t rxLastPosition
└── bool rxActive
```

职责：

```text
halUart          绑定 huart1
rxBuffer         当前借用的 DMA Buffer
rxBufferSize     当前 DMA Circular Buffer 容量
rxLastPosition   上一次已经上报到 Platform 的位置
rxActive         RX Session 是否活动
```

不在 Context 中加入：

- Ring Buffer；
- Task Handle；
- Queue；
- Semaphore；
- 协议状态；
- 动态内存。

---

## 7. Position / Wrap 算法

HAL `HAL_UARTEx_RxEventCallback(huart, Pos)` 中的 `Pos` 视为当前 DMA Buffer 已到达的位置。

Impl 保存：

```text
lastPosition
```

### 7.1 无 Wrap

```text
Pos > lastPosition
```

新增数据：

```text
[lastPosition, Pos)
```

产生一个 `RX_DATA` Event。

### 7.2 Wrap

```text
Pos < lastPosition
```

分成两个连续片段：

```text
[lastPosition, bufferSize)
[0, Pos)
```

分别产生两个 `RX_DATA` Event。

Platform Event 中的数据必须始终是连续内存，不构造跨 Buffer 尾首的虚假连续指针。

### 7.3 Duplicate Position

```text
Pos == lastPosition
```

表示没有新增字节，直接忽略，不产生空 `RX_DATA` Event。

### 7.4 Full Buffer Position

HAL TC 可能传入：

```text
Pos == bufferSize
```

完成尾部数据上报后，内部位置可归一化为：

```text
0
```

以便 Circular DMA 下一轮继续计算。

任何：

```text
Pos > bufferSize
```

都视为内部状态异常，不得越界访问。

---

## 8. 为什么保留 HT + TC + IDLE

Phase 2A 不关闭 DMA Half Transfer 中断。

115200 / 8N1 下，一字节约占 10 bit。

256-byte Buffer：

```text
128 bytes ≈ 11.1 ms
256 bytes ≈ 22.2 ms
```

如果发送端持续发送且一直没有 IDLE：

- 只依赖 IDLE 可能长期没有交付点；
- HT 可以在半缓冲时交付前 128 bytes；
- TC 可以在完整缓冲时交付后半部分；
- IDLE 则负责及时交付尚未达到 HT/TC 的短数据。

因此：

```text
short burst       -> IDLE
continuous stream -> HT / TC
mixed traffic     -> HT / TC / IDLE
```

底层可能出现相同位置的重复触发，必须由 `lastPosition` 去重。

---

## 9. Callback / ISR 边界

当前 HAL 配置：

```text
USE_HAL_UART_REGISTER_CALLBACKS = 0
```

因此 Phase 2A 使用 HAL weak callback override：

```c
HAL_UARTEx_RxEventCallback(...)
HAL_UART_ErrorCallback(...)
```

Callback 实现放在自定义 Impl 源文件，不在 Vendor HAL 源文件中修改。

IRQ 入口继续保持 CubeMX 薄入口：

```text
USART1_IRQHandler
    -> HAL_UART_IRQHandler(&huart1)

DMA2_Stream2_IRQHandler
    -> HAL_DMA_IRQHandler(&hdma_usart1_rx)
```

ISR / HAL Callback 允许：

- 判断是否为 USART1；
- 读取/更新 RX Context；
- 计算新增 DMA 区间；
- 产生 `RX_DATA` / `ERROR` Event；
- 做必要的小块数据复制（Phase 2B）。

禁止：

- 阻塞；
- 普通 Mutex；
- malloc/free；
- 完整协议解析；
- 大量格式化日志；
- 在 Callback 内通过 USART1 打印。

Phase 2A 不创建 RTOS Notification。

---

## 10. readAsync 行为

STM32 `readAsync` Ops 执行顺序：

```text
validate object/context
        ↓
validate buffer / length
        ↓
check rxActive == false
        ↓
check length <= 0xFFFF
        ↓
record rxBuffer / size / lastPosition=0
        ↓
HAL_UARTEx_ReceiveToIdle_DMA()
        ↓
HAL_OK -> rxActive=true
HAL_BUSY -> PLATFORM_ERR_BUSY
HAL_ERROR -> PLATFORM_ERR_IO
```

如果 HAL 启动失败：

- `rxActive` 必须保持 false；
- 不得留下半有效 Buffer lease；
- `rxBuffer/rxBufferSize/rxLastPosition` 回到空闲状态。

---

## 11. Cancel 语义

Phase 2A 支持：

```text
platform_uart_cancel(..., RX)
```

活动 RX Session 中调用：

```text
HAL_UART_AbortReceive()
        ↓
clear rxActive/context
        ↓
PLATFORM_UART_EVENT_CANCELED / RX
```

Buffer lease 在 CANCELED callback 执行后结束。

没有活动 RX Session 时取消 RX：

```text
PLATFORM_ERR_INVALID_STATE
```

Phase 2A：

```text
cancel(TX)   -> PLATFORM_ERR_NOT_SUPPORTED
```

`writeAsync()` 仍然不实现。

生命周期 stop 不通过公共 cancel 发送 CANCELED Event，而是执行内部 RX abort 后再切换对象状态。

---

## 12. Lifecycle 语义

### init

继续应用 Platform UART Config 并执行 HAL UART Init。

DMA 的基础 Handle / NVIC / Link 由 CubeMX MSP 配置负责。

### start

仅进入：

```text
STARTED / POWER_ACTIVE
```

不自动启动 DMA RX。

RX Session 必须显式调用：

```c
platform_uart_read_async(...)
```

### stop

如果 `rxActive == true`：

```text
HAL_UART_AbortReceive()
```

成功后：

- 清空 RX Session Context；
- 关闭后续事件源；
- 再进入 STOPPED。

如果 Abort 失败：

- stop 返回错误；
- 不得先把 Platform Object 标记为 STOPPED。

### deinit

仍要求从 STOPPED 进入，并执行 HAL UART DeInit。

---

## 13. Error 语义

DMA RX 期间 `HAL_UART_ErrorCallback()`：

```text
HAL_UART_ERROR_ORE
    -> PLATFORM_ERR_OVERFLOW

HAL_UART_ERROR_DMA
HAL_UART_ERROR_PE
HAL_UART_ERROR_NE
HAL_UART_ERROR_FE
    -> PLATFORM_ERR_IO
```

ERROR Event：

```text
type      = PLATFORM_UART_EVENT_ERROR
direction = PLATFORM_UART_DIRECTION_RX
data       = NULL
dataLength = 0
error      = mapped platform_error_t
```

错误导致 HAL RX 被终止后：

- `rxActive=false`；
- 清理 Session Context；
- ERROR callback 返回后 Buffer lease 结束。

Phase 2A 不在 ISR 内自动重启 DMA；重启策略留给后续 Service。

---

## 14. TX 行为

Phase 1 Blocking TX 保持不变。

Phase 2A 不实现：

```text
HAL_UART_Transmit_DMA
platform_uart_write_async
TX DMA
TX Complete callback
```

允许 Blocking TX 与 DMA RX 同时存在；若 HAL 返回 Busy/Error，沿用当前错误映射。

---

## 15. Phase 2A 板测 Buffer

Phase 2A Integration Test 固定先使用：

```text
DMA RX Buffer Size = 256 bytes
```

这是测试/当前实现基准，不进入 Platform 公共 Config，不暴露 DMA 概念到 Platform API。

未来 Service 可以提供其他静态 Buffer Size，只要：

```text
1 <= bufferSize <= 65535
```

并满足长期静态生命周期。

---

## 16. 验收场景

至少完成：

### A. Short + IDLE

发送小于 128 bytes 的短数据并停止发送。

验证：

- IDLE 能产生 RX_DATA；
- 总字节数正确；
- 内容正确。

### B. Continuous > DMA Buffer

连续发送超过 256 bytes 的已知 Pattern。

验证：

- HT / TC 能持续交付；
- 超过一圈后继续接收；
- 无重复；
- 无丢失；
- 顺序正确。

### C. Mixed Boundary

发送跨越 HT / TC / IDLE 边界的数据。

验证 `lastPosition` 去重和 Wrap 逻辑。

### D. Cancel + Restart

```text
readAsync
 -> receive data
 -> cancel(RX)
 -> CANCELED
 -> readAsync again
 -> receive data again
```

### E. Lifecycle Stop

活动 RX Session 中调用 stop，验证：

- DMA RX 被关闭；
- 不再出现尾部非法 callback；
- 状态正确进入 STOPPED；
- restart 后可以重新 readAsync。

---

## 17. Phase 2A 明确不实现

```text
UART Service
Ring Buffer
FreeRTOS Task Notification
Protocol Parser
APP Communication Protocol
DMA TX
Async TX
Dynamic Memory
通用 DMA Platform Framework
impl_dma 通用抽象
Vendor HAL 修改
```

`04_Impl/impl_mcu/impl_dma.c/.h` 保持非本阶段核心，不为了“有 DMA”而建立额外抽象层。

---

## 18. Phase 2B 接口

Phase 2A 通过后，下一阶段才建立：

```text
Platform RX_DATA Callback
       ↓ ISR context
UART Service
       ↓ copy
Ring Buffer
       ↓
ISR-safe Notification
       ↓
Communication Task
```

Phase 2B 不应重新设计 STM32 DMA 数据采集机制，除非 Phase 2A 板测证明当前方案存在真实缺陷。
