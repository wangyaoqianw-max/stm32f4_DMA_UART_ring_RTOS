# 工程长期记忆与交接说明

更新时间：2026-09-04

> 本文件是 AI Agent / Codex 与人工开发者恢复工程上下文时的长期入口。  
> 业务行为以 `00_Doc/00_项目需求/最终功能需求.md` 为准。  
> 最终 Phase 9 设计以 `00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md` 为准。  
> 架构合同以 `00_Doc/04_Agent/architecture.md` 为准。  
> 当前施工步骤以 `00_Doc/04_Agent/implementation_plan.md` 为准。

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

最终目标：形成一个按键 + UART 可统一控制、DHT20 + MPU6050 统一采集、UART DMA 上报、RTT 诊断的完整 FreeRTOS 数据采集系统，并沉淀可复用 Platform / Service 能力。

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

CubeMX 生成文件只承担初始化、Scheduler、IRQ / HAL Callback 和薄胶水。

---

# 3. 阶段状态

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
Final Integrated Board Test
Project Core Complete
```

原 RTOS Task/Event 阶段与 Final APP Integration 已合并为 Phase 9。

不再保留独立 Phase 10。

---

# 4. Codex 当前执行入口

Codex 开始编码前必须读取：

```text
00_Doc/00_项目需求/最终功能需求.md
00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/implementation_plan.md
00_Doc/04_Agent/handoff.md
```

当前正式执行计划：

```text
Phase 9 Final RTOS Application Integration Implementation Plan
Status: DESIGN FROZEN / READY FOR CODEX
```

必须从 Task 0 baseline verification 开始，按计划逐 Task 执行、测试和 Review。

---

# 5. Phase 8 UART 真实基线

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

保持现有 SPSC，不加普通 Mutex，不建第二套 RX。

TX：

```text
USART1 TX DMA = DMA2_Stream7 / Channel 4 / Normal
Platform async TX = IMPLEMENTED / HOST + KEIL VERIFIED
UART Service TX = IMPLEMENTED / HOST + KEIL VERIFIED
APP Communication parser = IMPLEMENTED / HOST + KEIL VERIFIED
target TX DMA verification = DEFERRED TO FINAL PHASE 9 TEST
```

正式 TX 链：

```text
Communication Task
 -> service_uart_write()
 -> platform_uart_write_async()
 -> STM32 UART Impl
 -> HAL_UART_Transmit_DMA()
 -> DMA2_Stream7
 -> TX complete callback
 -> Platform event
 -> UART Service ownerThread wake
```

冻结：

```text
one active TX transaction only
RX + TX simultaneously allowed
Communication Task = sole USART1 product TX requester
no UART Service TX RingBuffer / TX Queue / TX worker
service_uart_write() returns => DMA no longer accesses caller TX buffer
USART1 = product control/data
RTT = diagnostics
```

禁止 direct HAL blocking TX 重新进入产品正式路径。

---

# 6. UART Application Protocol

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
fixed-size storage
```

APP Communication 不保存 STOPPED / RUNNING 真值。

统一事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

Button 与 UART 均映射到同一组事件。

---

# 7. 最终四任务模型

固定 4 个产品业务 Task：

```text
Communication Task   2048 B   ABOVE_NORMAL
Control Task         1024 B   ABOVE_NORMAL
Acquisition Task     1536 B   NORMAL
Indicator Task        768 B   BELOW_NORMAL
```

资源策略：

```text
first make complete system run correctly
 -> final target scenarios
 -> measure stack high-water mark / Queue peak occupancy
 -> shrink only with evidence
```

现在不做低功耗设计；Tickless / Button EXTI wake 留作后续独立优化。

## 7.1 CubeMX defaultTask

CubeMX 自动生成 `defaultTask` 当前无法直接删除。

不要把它变成第五个产品 Task。

实现阶段只在 `StartDefaultTask()` USER CODE 中：

```c
(void)argument;
osThreadExit();
```

使其首次运行后退出。

FreeRTOS Idle / Timer Service Task 是内核任务，不计入产品 Task。

---

# 8. Control Task / APP FSM

Control Task 负责：

```text
Button 10 ms polling
Button Service gesture recognition
Control Queue consumer
sole APP Control FSM
business orchestration
```

唯一业务状态：

```text
STOPPED
RUNNING
```

只有 Control Task 可以修改。

允许：

```text
onceActive
onceSource
```

作为正交 operation context，不是第三状态。

Button：

```text
SINGLE -> START
DOUBLE -> SAMPLE_ONCE
LONG   -> STOP
```

UART source 与 Button source 使用同一 FSM。

ONCE active：

```text
UART START/STOP/ONCE -> ERR BUSY
UART STATUS          -> STATUS STOPPED
Button control gesture -> ignore business action
```

Control Task 使用 deadline-driven Queue wait 保持 10 ms Button polling，不用长业务阻塞。

---

# 9. Unified Acquisition Service

Phase 9 新增：

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
START / STOP / ONCE decision
Task / Queue
UART
LED
shared I2C lifecycle
```

成功语义 all-or-nothing：

```text
DHT20 OK && MPU6050 OK -> acquisition OK
otherwise             -> FAILED
```

即使 DHT20 失败，也继续尝试 MPU6050 以提高诊断价值。

使用 temporary result；两者都成功才 commit caller output，失败不修改 caller data。

---

# 10. Acquisition Task

Acquisition Task 是：

```text
sole runtime DHT20 accessor
sole runtime MPU6050 accessor
sole runtime shared Software I2C accessor
```

因此第一版不加 I2C Mutex。

Command：

```text
START_PERIODIC
STOP_PERIODIC
SAMPLE_ONCE
```

调度：

```text
STOPPED -> command Queue WAIT_FOREVER
START -> immediate first complete sample
RUNNING -> queue_receive(timeout to absolute deadline)
nextDeadline += PROJECT_ACQUISITION_PERIOD_MS
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

超期不 catch-up 补采。

STOP 到达同步 sensor transaction 时不强制中断 transaction；允许安全收尾，但在 periodic result 发布前观察 pending STOP，若 STOP 已到则丢弃 stale result。

Acquisition Task 不直接 UART TX、不直接控制 LED。

---

# 11. APP IPC Baseline

```text
Control Queue                 depth 8
Acquisition Command Queue     depth 4
Communication Outbound Queue  depth 8
Indicator Queue               depth 4
```

全部：

```text
Platform Queue
bounded
value-copy
no temporary caller stack pointer
no infinite producer block
queue full observable
```

Control Queue 至少：

```text
CONTROL_REQUEST(event + source)
ONCE_ACQUISITION_FAILED
ONCE_TX_RESULT
```

Communication Outbound：

```text
CONTROL_RESPONSE
PERIODIC_REPORT
ONCE_REPORT
```

Indicator：

```text
STOPPED
RUNNING
ONCE_SUCCESS
```

第一版不引入 Queue Set / Event Group / APP state mutex。

---

# 12. Communication Task 最终行为

Communication Task 继续：

```text
UART Service ownerThread
sole USART1 product TX requester
strict CRLF parser
product response/report formatter
```

新增 Communication Outbound Queue consumer。

普通 APP Queue send 无法直接唤醒 UART Service private notify wait，第一版冻结：

```text
drain outbound queue nonblocking
 -> app_communication_process(20 ms)
 -> drain outbound queue nonblocking
```

因此：

```text
PROJECT_COMM_WAIT_TIMEOUT_MS = 20U
```

UART RX notify 仍可立即唤醒。

状态响应：

```text
OK START\r\n
OK STOP\r\n
ERR ALREADY_RUNNING\r\n
ERR ALREADY_STOPPED\r\n
ERR BUSY\r\n
ERR ACQUISITION_FAILED\r\n
STATUS RUNNING\r\n
STATUS STOPPED\r\n
```

产品 report：

```text
ENV,T=25.34,H=62.18\r\n
IMU,AX=0.013,AY=-0.021,AZ=0.998,GX=0.12,GY=-0.42,GZ=0.08\r\n
```

---

# 13. ONCE 完整事务

成功条件：

```text
DHT20 success
AND MPU6050 success
AND complete report UART TX success
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

任一 acquisition / TX failure：

```text
clear onceActive
no success blink
RTT diagnostic
```

成功 ONCE 不强制追加 `OK ONCE`；完整 report TX 是成功输出。

---

# 14. Indicator Task

Indicator Task 唯一承担 LED semantic side effect：

```text
STOPPED -> OFF
RUNNING -> ON
ONCE_SUCCESS -> blink 3 times, 100 ms on/off -> OFF
```

当前约 600 ms blocking blink 可留在 Indicator Task，不得阻塞 Control / Acquisition / Communication。

如果闪烁期间 START 到达，第一版接受 RUNNING LED 最多等待当前 blink 完成；APP FSM 状态必须立即生效。

---

# 15. app_system Composition Root

`app_system.c` 是最终静态对象装配入口。

最终负责：

```text
Platform objects
Services
4 APP Queues
4 APP contexts
4 Platform Threads
fixed UART DMA/RingBuffer storage
```

最终初始化原则：

```text
Platform/hardware resources
 -> shared bus + sensors/basic devices
 -> Services
 -> Queues
 -> APP contexts
 -> Threads LAST
```

不要在线程创建后才补其必要依赖。

业务运行路径不使用 malloc/free 传递 APP data。

---

# 16. Final Target Acceptance

必须覆盖：

```text
boot -> STOPPED / LED OFF / UART RX active / no reports
Button SINGLE -> RUNNING / LED ON / immediate first report / every 2 s
Button LONG -> STOPPED / LED OFF / no future periodic report
Button DOUBLE in STOPPED -> one complete report / TX success / 3 blinks
UART START/STOP/ONCE/STATUS/HELP correct
ONCE BUSY semantics correct
Button + UART one state truth
DHT20 + MPU6050 sequential shared Soft-I2C stable
USART1 RX remains active while TX DMA runs
fragmented/coalesced UART commands remain correct
sensor/UART failures visible through RTT
ONCE failure never success-blinks
```

最终完整系统稳定后记录：

```text
Communication stack high-water mark
Control stack high-water mark
Acquisition stack high-water mark
Indicator stack high-water mark
4 APP Queue observed peak occupancy
```

第一轮目标是系统稳定，不以压缩 stack / Queue 为前置验收。

---

# 17. 当前范围外

```text
low-power / Tickless policy
Button EXTI wake
SPI / LCD / GUI
W25Q64 / AT24C02
Bluetooth
Roll/Pitch/Yaw / DMP / filters
complex binary protocol
unneeded framework expansion
```

---

# 18. 当前停止点

```text
Phase 8 code: IMPLEMENTATION COMPLETED / HOST + KEIL VERIFIED
Phase 8 UART TX DMA target verification: DEFERRED TO PHASE 9
Phase 9 final design: FROZEN
Phase 9 production code: IMPLEMENTED
Phase 9 Host regression: PASS / 34 of 34 test groups
Phase 9 Keil rebuild: PASS / 0 Error(s), 14 baseline Warning(s)
Phase 9 target integration: TARGET VERIFICATION REQUIRED
Phase 9 resource measurement: TARGET VERIFICATION REQUIRED
Phase 9 closure: NOT COMPLETE
```

当前实现包含 Unified Acquisition Service、唯一 Control FSM、Acquisition / Indicator Task、Communication outbound、4 条 APP Queue、最终 Composition Root 和 defaultTask self-exit。

下一步：连接目标板，按 `00_Doc/04_Agent/implementation_plan.md` 的目标板验证记录表完成 USART1 TX DMA、完整业务场景、Task stack high-water mark 和 Queue peak occupancy 实测。实测完成前不得标记 Phase 9 `COMPLETED`。
