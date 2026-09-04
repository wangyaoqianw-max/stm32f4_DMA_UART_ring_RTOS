# Final Acquisition System Development Roadmap

> 文档类型：Development Phase Roadmap  
> 状态：Baseline  
> 日期：2026-09-04  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 文档职责

本文件回答：

```text
有哪些开发 Phase？
各 Phase 依赖什么？
完成门槛是什么？
当前下一步是什么？
```

详细业务需求：`00_Doc/00_项目需求/最终功能需求.md`  
长期架构：`00_Doc/04_Agent/architecture.md`  
当前施工计划：`00_Doc/04_Agent/implementation_plan.md`

每个 Phase 必须先专项设计，再生成 / 替换当前 implementation plan。

---

# 2. 已验证基础能力

```text
UART Platform blocking capability             VERIFIED
UART DMA RX + IDLE / HT / TC                  VERIFIED
UART Service RX + SPSC RingBuffer              VERIFIED
APP Communication Phase 1                     VERIFIED
Platform OS                                   VERIFIED
Service Log + EasyLogger + RTT                 VERIFIED
Platform GPIO + STM32 Impl + Board Binding     VERIFIED
Software I2C                                  VERIFIED
LED / Indicator Module                        VERIFIED
Button / Button Service                       VERIFIED
DHT20 hardware + production module             VERIFIED
MPU6050 hardware + production module           VERIFIED
```

UART TX 当前必须区分：

```text
blocking TX path                               EXISTS / VERIFIED baseline
USART1 TX DMA CubeMX configuration             READY
STM32 Platform async TX implementation         IMPLEMENTED / HOST + KEIL VERIFIED
UART Service TX transaction                    IMPLEMENTED / HOST + KEIL VERIFIED
TX DMA target verification                     DEFERRED TO PHASE 9
```

最终闭环仍缺：

```text
UART application commands / report
Permanent RTOS task / event organization
Unified Acquisition Service / Task
Final APP Control FSM
Integrated target-board verification
```

---

# 3. 总体阶段

```text
Phase 1  GPIO STM32 Impl                         COMPLETED
Phase 2  Board Resource + CubeMX Configuration   COMPLETED
Phase 3  Software I2C                            COMPLETED
Phase 4  LED Module                              COMPLETED
Phase 5  Button Module                           COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 6  DHT20 Environment Module                COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 7  MPU6050 Motion Module                   COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 8  UART Application Communication          IMPLEMENTATION COMPLETED / HOST + KEIL VERIFIED
                                                  TARGET VERIFICATION DEFERRED TO PHASE 9
Phase 9  RTOS Task / Event Design
Phase 10 Final APP Integration
Final Integrated Board Test
```

---

# 4. 已完成阶段摘要

Phase 1 / 2：GPIO STM32 Impl、Board Binding、CubeMX 资源配置完成。

```text
PC13 Status LED
PA0 User Key
PB6 Soft I2C SCL
PB7 Soft I2C SDA
PA9 USART1_TX
PA10 USART1_RX
```

Phase 3：Software I2C，Master-only / 7-bit / synchronous / Platform GPIO based / Repeated START / Host + Keil + target verified。

Phase 4：Indicator Service -> Platform LED -> Platform GPIO；STOPPED/OFF、RUNNING/ON、ONCE_SUCCESS/3 blinks。

Phase 5：Platform Button + Button Service，10 ms sample / 30 ms debounce / 300 ms double / 3000 ms long，Host + Keil + target verified。

Phase 6：Platform DHT20，shared Software I2C non-owning reference，Host + Keil + RTT + logic analyzer verified。

Phase 7：Platform MPU6050，WHO_AM_I / wake / fixed config / 14-byte burst / raw + g/dps，Host + Keil + RTT + logic analyzer + physical sanity + negative smoke verified。

---

# 5. Phase 8 — UART Application Communication

正式专项设计：

```text
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
```

当前执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
UART Application Communication Phase 8 Implementation Plan
Status: IMPLEMENTATION COMPLETED / HOST + KEIL VERIFIED
TARGET VERIFICATION DEFERRED TO PHASE 9
```

Phase 8 分为三块：

```text
8A — Reusable UART TX DMA
8B — UART Service TX transaction
8C — APP Communication CRLF protocol / control event outlet
```

## 5.1 RX 保持现有稳定链

```text
USART1 RX
 -> DMA2_Stream2 Circular
 -> IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform UART RX_DATA
 -> UART Service
 -> SPSC RingBuffer
 -> Communication Task
```

不得建立第二套 UART RX 路径，不给当前 SPSC 增加普通 Mutex。

## 5.2 TX DMA 补齐

CubeMX 已于 2026-09-04 由人工开启：

```text
USART1_TX
DMA2_Stream7
Channel 4
Memory -> Peripheral
Mode = Normal
IRQ priority = 5
```

Phase 8 production 已实现：

```text
Platform UART write_async
 -> STM32 HAL_UART_Transmit_DMA
 -> TX_COMPLETE / cancel / error
 -> UART Service synchronous Task-facing write
```

本次 Keil production build 已通过；独立目标板验证延期到 Phase 9。

## 5.3 UART Service

```text
RX = long-lived DMA stream + RingBuffer
TX = one-shot DMA transaction
```

第一版：

```text
one active TX transaction only
no TX RingBuffer
no TX Queue
no TX worker Task
no public Service async TX API
```

`service_uart_write()` 对 Task 表现为同步完成，但内部使用 DMA + notify wait；返回后 DMA 不得继续访问 caller buffer。

## 5.4 Command protocol

严格命令：

```text
START\r\n
STOP\r\n
ONCE\r\n
STATUS\r\n
HELP\r\n
```

规则：

```text
strict CRLF
uppercase only
case-sensitive
no trim
no arguments
fixed-size caller/static storage
```

必须处理 fragmented / coalesced arbitrary byte chunks。

## 5.5 APP Control Event outlet

统一事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

Phase 8 只冻结逻辑 handler 出口，不冻结 Queue / Task / direct-call 机制。

```text
HELP -> Communication local
START/STOP/ONCE/STATUS -> APP Control event outlet
```

Communication 不维护 STOPPED / RUNNING。

## 5.6 Product TX ownership

冻结：

```text
Communication Task = sole USART1 product TX requester
```

未来 Acquisition Task 只交付 acquisition result，不直接调用 UART TX。

正式通道：

```text
USART1 = product control + product data
RTT    = diagnostics
```

现有 `printf/fputc -> HAL_UART_Transmit(&huart1)` 旁路在 Phase 8 production implementation 中退出正式运行路径。

## 5.7 Phase 8 不实现

```text
APP Control FSM
permanent Control Queue / Task
Acquisition Service / Task
Button permanent IPC
Indicator permanent IPC
2 s integrated report scheduling
final ONCE business transaction
```

---

# 6. Phase 9 — RTOS Task / Event Design

已确认方向：

```text
Communication Task
Acquisition Task
Indicator Task
```

本 Phase 冻结：

```text
permanent Button execution context
Button -> APP control IPC
APP control event consumer execution context
Acquisition result -> Communication IPC
Indicator event delivery
priority / stack / buffering / overflow policy
Unified Acquisition Service / Acquisition Task
```

统一 Acquisition Service 串行调用 DHT20 + MPU6050；不按设备数量机械创建独立 Task。

---

# 7. Phase 10 — Final APP Integration

APP 唯一业务状态：

```text
STOPPED
RUNNING
```

最终闭环：

```text
Button / UART
 -> APP Control FSM
 -> Acquisition / Indicator / Communication
```

RUNNING 统一采集 / 上报周期：

```text
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

Phase 10 负责最终 state-dependent UART response、ONCE 完整业务结果和系统状态转换。

---

# 8. 当前下一步

```text
Phase 5 Button      CLOSED
Phase 6 DHT20       CLOSED / TARGET VERIFIED
Phase 7 MPU6050     CLOSED / TARGET VERIFIED
Phase 8 UART App    IMPLEMENTATION COMPLETED / HOST + KEIL VERIFIED
                    TARGET VERIFICATION DEFERRED TO PHASE 9
```

当前停止点：

```text
Phase 8 implementation completed
 -> Host regression and Keil production build verified
 -> target-board validation deferred to Phase 9 RTOS Task / Event / IPC integration
```
