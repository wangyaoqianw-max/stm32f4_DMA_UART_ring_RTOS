# Final Acquisition System Development Roadmap

> 文档类型：Development Phase Roadmap  
> 状态：Final Baseline  
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
最终 Phase 9 设计：`00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md`  
长期架构：`00_Doc/04_Agent/architecture.md`  
当前施工计划：`00_Doc/04_Agent/implementation_plan.md`

---

# 2. 已验证基础能力

```text
UART Platform blocking capability             VERIFIED
UART DMA RX + IDLE / HT / TC                  VERIFIED
UART Service RX + SPSC RingBuffer              VERIFIED
Platform OS                                   VERIFIED
Service Log + EasyLogger + RTT                 VERIFIED
Platform GPIO + STM32 Impl + Board Binding     VERIFIED
Software I2C                                  VERIFIED
LED / Indicator Module                        VERIFIED
Button / Button Service                       VERIFIED
DHT20 hardware + production module             VERIFIED
MPU6050 hardware + production module           VERIFIED
```

UART TX 当前：

```text
blocking TX baseline                           VERIFIED
USART1 TX DMA CubeMX configuration             READY
STM32 Platform async TX implementation         IMPLEMENTED / HOST + KEIL VERIFIED
UART Service TX transaction                    IMPLEMENTED / HOST + KEIL VERIFIED
APP Communication CRLF protocol                IMPLEMENTED / HOST + KEIL VERIFIED
TX DMA target verification                     DEFERRED TO PHASE 9
```

---

# 3. 最终阶段路线

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
Phase 9  Final RTOS Application Integration      IMPLEMENTED / HOST + KEIL VERIFIED
                                                  TARGET + RESOURCE VERIFICATION REQUIRED
Final Integrated Board Test                       PENDING
Project Core Complete
```

原“Phase 9 RTOS Task / Event Design”与“Phase 10 Final APP Integration”已合并。

不再保留独立 Phase 10。

理由：最终 Task、IPC、APP FSM、Acquisition Service、2 s scheduling、UART report、ONCE completion 和 Indicator 业务链已经作为一个不可分割的最终运行闭环冻结。继续人为拆成两个 Phase 会造成状态真值、IPC 合同和目标板验收重复切割。

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

Phase 8：USART1 TX DMA production path、UART Service Task-facing TX transaction、strict CRLF parser、APP Control event outlet 已实现并通过 Host + Keil；目标板 TX DMA 验证合并进入最终 Phase 9 综合测试。

---

# 5. Phase 9 — Final RTOS Application Integration

正式设计：

```text
00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md
```

当前执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
Phase 9 Final RTOS Application Integration Implementation Plan
```

本 Phase 是最终软件实现阶段。

## 5.1 四任务模型

```text
Communication Task   2048 B   ABOVE_NORMAL
Control Task         1024 B   ABOVE_NORMAL
Acquisition Task     1536 B   NORMAL
Indicator Task        768 B   BELOW_NORMAL
```

资源值故意偏宽松：先让完整系统稳定运行，最终板测后根据 stack high-water mark / Queue peak occupancy 再优化。

CubeMX `defaultTask` 不计入产品任务；实现阶段只在 USER CODE 中让其首次运行后 `osThreadExit()`。

## 5.2 Control / FSM

```text
Button 10 ms polling + Button Service
UART control request
        ↓
Control Task
        ↓
unique APP Control FSM
STOPPED / RUNNING
```

Button 与 UART 使用同一状态真值。

## 5.3 APP IPC

```text
Control Queue                 depth 8
Acquisition Command Queue     depth 4
Communication Outbound Queue  depth 8
Indicator Queue               depth 4
```

全部 bounded + value-copy；不使用临时 stack 指针。

第一版不增加 I2C mutex、Queue Set、Event Group 或第五个产品 Task。

## 5.4 Unified Acquisition

```text
Acquisition Task
 -> Unified Acquisition Service
 -> DHT20
 -> MPU6050
 -> shared Software I2C
```

Service all-or-nothing；两个传感器均成功才提交完整结果。即使 DHT20 失败，也继续尝试 MPU6050 用于诊断。

Acquisition Task 是唯一 sensor / Software I2C runtime accessor。

## 5.5 周期调度

```text
START -> immediate first sample
then every 2000 ms by absolute deadline
no catch-up burst after overrun
STOP -> no future periodic publish
```

同步 sensor transaction 不强制中途取消；STOP 到达 active transaction 时允许安全收尾，但必须在发布 periodic result 前观察 pending STOP 并丢弃 stale result。

## 5.6 UART / Communication

Communication Task 保持：

```text
UART Service ownerThread
sole USART1 product TX requester
```

新增 outbound Queue consumer。第一版使用 20 ms communication wait timeout 解决 UART notify 与普通 APP Queue 多源等待，不新增 Queue Set。

## 5.7 ONCE

```text
STOPPED DOUBLE / UART ONCE
 -> Unified Acquisition success
 -> complete UART report TX success
 -> Control completion
 -> Indicator ONCE_SUCCESS
 -> blink 3 times
```

任一 sensor 或 UART TX 失败，不执行成功闪烁。

ONCE 执行期间业务状态仍为 STOPPED；UART START/STOP/ONCE 返回 `ERR BUSY`，STATUS 返回 STOPPED。

---

# 6. Phase 9 完成门槛

必须全部完成：

```text
Unified Acquisition Service
Control Task + sole APP FSM
Acquisition Task + absolute 2 s scheduling
Indicator Task
Communication outbound integration
4 APP Queues
app_system final composition
CubeMX defaultTask self-exit
Host regression
Keil production rebuild
USART1 TX DMA target verification
Final integrated board acceptance
resource high-water / queue observation
Coding Standard Review
Architecture Review
```

最终目标板场景：

```text
boot STOPPED / LED OFF / UART RX active
Button SINGLE -> RUNNING / LED ON / immediate report / 2 s reports
Button LONG -> STOPPED / LED OFF / no future reports
Button DOUBLE in STOPPED -> one report / TX success / 3 blinks
UART START/STOP/ONCE/STATUS/HELP correct
Button + UART one state truth
DHT20 + MPU6050 shared Soft I2C stable
RX remains active during TX DMA
ONCE failure never success-blinks
RTT exposes relevant failures
```

---

# 7. 当前下一步

```text
Phase 9 production implementation completed
 -> Host regression 34/34 passed
 -> Keil rebuild 0 errors / 14 baseline warnings
 -> Final target scenarios and USART1 TX DMA verification
 -> record stack high-water marks and Queue peak occupancy
 -> Final Integrated Board Test
 -> Project Core Complete
```

当前不再进行新的架构阶段设计或新建 Phase 10。未连接目标板前，不把 Phase 9 标记为 `COMPLETED`。

第一版不讨论低功耗；Tickless Idle、Button EXTI wake、SPI/LCD、Flash、Bluetooth、姿态融合均为后续独立扩展。
