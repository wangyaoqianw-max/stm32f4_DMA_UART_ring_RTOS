# 工程交接说明

更新时间：2026-08-29

## 1. 工程快照

- 工程根目录：`RTT_elog_DMA_UART_ring_project/`
- 当前分支：`main`
- 当前活动阶段：UART Phase 2A — DMA RX + IDLE + Platform RX_DATA Event
- 当前状态：`READY_FOR_IMPLEMENTATION`
- MCU：STM32F411CEU6，UFQFPN48
- 软件环境：STM32 HAL、CMSIS-RTOS2 / FreeRTOS、Keil、EasyLogger、SEGGER RTT

固定依赖方向：

```text
APP -> Service -> Platform -> Impl -> Vendor / HAL / RTOS / Hardware
```

当前阶段只建立：

```text
USART1 RX
 -> DMA Circular
 -> IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform RX_DATA Event
```

不进入 RingBuffer / UART Service / RTOS Notification。

---

## 2. 已完成阶段

### Log Phase 1 — COMPLETED

已完成：

- Platform Log 与 EasyLogger / RTT 解耦。
- Host Test PASS。
- Keil Build PASS。
- RTT Runtime Smoke Test PASS。

### UART Phase 1 — COMPLETED

真实板测已确认：

- construct PASS；
- CREATED 状态写保护 PASS；
- init/start PASS；
- Blocking TX PASS；
- Blocking fixed-length RX PASS；
- stop / STOPPED guard PASS；
- restart + TX PASS；
- deinit PASS。

USB-UART 实际收到：

```text
UART_PHASE1_TX_OK
UART_RESTART_OK
```

PC -> MCU：

```text
PING
```

RTT 确认固定长度 RX PASS。

临时测试代码已经恢复，恢复后 Keil Full Rebuild：

```text
0 Error(s), 13 Warning(s)
```

---

## 3. Keil Build 目录状态

Keil 构建输出已经整理为独立目录，生成物不再作为工程源码跟踪。

构建时仍需遵守：

```text
Agent / Git 写操作结束
    ↓
Keil Clean / Rebuild
```

如出现：

```text
C4051E
L6449E
Invalid argument
```

优先按 Keil / Windows 文件 I/O 环境问题处理，不修改 UART 业务代码规避。

---

## 4. Phase 2A CubeMX Baseline — VERIFIED

用户已完成 CubeMX 配置并 Generate Code。

当前 `.ioc` / 生成代码确认：

```text
USART1_TX              PA9
USART1_RX              PA10
115200 / 8N1

USART1_RX DMA          DMA2 Stream2 / Channel 4
Direction              Peripheral -> Memory
Peripheral Increment   Disable
Memory Increment       Enable
Peripheral Width       Byte
Memory Width           Byte
Mode                   Circular
Priority               Medium
FIFO                   Disable

USART1_IRQn            Priority 5 / Sub 0
DMA2_Stream2_IRQn      Priority 5 / Sub 0
```

生成代码已存在：

```text
DMA_HandleTypeDef hdma_usart1_rx
__HAL_LINKDMA(uartHandle, hdmarx, hdma_usart1_rx)
DMA2_Stream2_IRQHandler -> HAL_DMA_IRQHandler(...)
USART1_IRQHandler       -> HAL_UART_IRQHandler(...)
```

`main.c` 当前初始化顺序：

```text
MX_GPIO_Init()
MX_DMA_Init()
MX_USART1_UART_Init()
```

CubeMX 侧当前无阻塞项。

---

## 5. Phase 2A Design — APPROVED

设计文档：

```text
00_Doc/02_架构设计/UART_Phase2A_DMA_RX设计.md
```

状态：

```text
APPROVED / FROZEN FOR IMPLEMENTATION
```

核心方案：

```text
HAL_UARTEx_ReceiveToIdle_DMA
+ DMA Circular
+ HT / TC / IDLE
+ Platform RX_DATA Event
```

Platform 不感知 HT / TC / IDLE 的来源差异。

---

## 6. Buffer Ownership — Scheme A

方案 A 已批准并冻结。

现有 Platform API 保持：

```c
platform_uart_read_async(uart, buffer, bufferSize)
```

语义改为启动持续 RX Session。

所有权：

```text
Memory Storage Owner      Caller
DMA Control Owner         STM32 UART Impl
RX Session Buffer Writer  DMA / Impl
RX_DATA Consumer          callback read-only
```

调用者提供长期有效的静态 Buffer。

Phase 2A 板测基准：

```text
256 bytes
```

成功 `readAsync()` 后 Buffer 使用权借给 Impl，直到：

```text
cancel(RX)
RX error
lifecycle stop
```

`RX_DATA.event.data` 只保证在 callback 执行期间有效；callback 返回后不得继续保存该 DMA Buffer 指针。

基础 `architecture.md` 中旧的“DMA Buffer 属于 UART Impl”在 Phase 2A 中解释为：

> DMA Buffer 的硬件控制、位置状态和访问纪律属于 Impl；静态 Buffer 存储允许由调用者持有。

Phase 2A 以已批准设计文档为该语义的优先解释。

---

## 7. RX Event / Position Contract

HAL：

```c
HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart,
                           uint16_t Pos)
```

Impl 保存：

```text
rxBuffer
rxBufferSize
rxLastPosition
rxActive
```

新增数据算法：

```text
Pos > last
    -> [last, Pos)

Pos < last
    -> [last, bufferSize)
    -> [0, Pos)

Pos == last
    -> duplicate/no new data, ignore

Pos == bufferSize
    -> emit tail then normalize lastPosition to 0
```

每个连续片段独立产生：

```text
PLATFORM_UART_EVENT_RX_DATA
```

不得把跨 Buffer 尾首的数据伪装成一个连续指针。

---

## 8. Why HT + TC + IDLE

256-byte DMA Buffer / 115200 8N1：

```text
128 bytes ≈ 11.1 ms
256 bytes ≈ 22.2 ms
```

因此：

```text
short burst       -> IDLE
continuous stream -> HT / TC
mixed traffic     -> HT / TC / IDLE
```

不关闭 HT。

`lastPosition` 负责去除可能的重复位置事件。

---

## 9. Callback / ISR Boundary

当前：

```text
USE_HAL_UART_REGISTER_CALLBACKS = 0
```

Phase 2A 由自定义 STM32 UART Impl override：

```c
HAL_UARTEx_RxEventCallback(...)
HAL_UART_ErrorCallback(...)
```

不得修改 Vendor HAL。

Callback / ISR 只允许：

- 判断 USART1；
- 更新轻量 RX Context；
- 计算新增数据位置；
- 产生 Platform Event；
- 板测时做必要的静态 Buffer 数据复制/计数。

禁止：

- 阻塞；
- 普通 Mutex；
- malloc/free；
- 完整协议解析；
- 大量日志；
- USART1 debug print。

Phase 2A 不创建 RTOS Notification。

---

## 10. Cancel / Stop / Error Contract

### cancel(RX)

活动 RX：

```text
HAL_UART_AbortReceive
 -> clear RX session
 -> CANCELED / RX Event
```

无活动 RX：

```text
PLATFORM_ERR_INVALID_STATE
```

Phase 2A：

```text
cancel(TX)  -> NOT_SUPPORTED
writeAsync  -> NOT_SUPPORTED
```

### lifecycle stop

活动 RX 时先 Abort，只有成功后才进入 STOPPED。

stop 不发送 CANCELED Event。

### HAL RX Error

映射：

```text
ORE              -> PLATFORM_ERR_OVERFLOW
DMA / PE / NE / FE -> PLATFORM_ERR_IO
```

ERROR 后释放 RX Session，不在 ISR 中自动重启 DMA。

---

## 11. Current Implementation Plan

文件：

```text
00_Doc/04_Agent/implementation_plan.md
```

当前：

```text
Status: READY_FOR_IMPLEMENTATION
Phase: UART Phase 2A
```

执行顺序：

```text
Host tests first
    ↓
STM32 readAsync + RX context
    ↓
RxEvent position processing
    ↓
Cancel / Stop / Error
    ↓
Platform regression
    ↓
Keil Clean Rebuild
    ↓
Board Smoke Test
    ↓
restore temporary test code
    ↓
final Rebuild
```

---

## 12. Required Board Verification

最低场景：

### Short / IDLE

```text
HELLO
```

确认总长度 5、内容一致。

### Continuous

连续至少：

```text
600 bytes
```

确认：

- >256 bytes 后继续接收；
- 无丢失；
- 无重复；
- 顺序正确。

### Mixed Boundary

覆盖：

```text
HT 128
TC 256
IDLE
```

### Cancel + Restart

取消后重新启动 `readAsync()`，再次正常接收。

### Lifecycle Stop + Restart

活动 DMA RX 中 stop；restart 后重新 `readAsync()` 正常。

---

## 13. Phase 2A Scope Guard

禁止提前实现：

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
```

如果实现证明必须修改冻结 Platform API：

```text
BLOCKED
```

停止并重新设计。

---

## 14. Completion Rule

只有以下都有真实证据，才能写：

```text
UART Phase 2A = COMPLETED
```

至少包括：

- Host tests PASS；
- Keil Full Rebuild 0 Error；
- Short/IDLE PASS；
- Continuous >256 bytes PASS；
- HT/TC/IDLE 混合边界无丢失/重复；
- Cancel + Restart PASS；
- Stop + Restart PASS；
- Phase 1 regression PASS；
- 临时测试代码恢复；
- 恢复后最终 Rebuild 0 Error；
- 本 handoff 更新真实结果。

没有板测证据时只能写：

```text
CODE_COMPLETE_PENDING_HARDWARE_VERIFICATION
```

---

## 15. Next Phase

Phase 2A 完成后才进入 Phase 2B：

```text
Platform RX_DATA Event
       ↓ ISR
UART Service
       ↓
Ring Buffer
       ↓
ISR-safe Notification
       ↓
Communication Task
```

Phase 2A 完成前不得开始 Phase 2B。