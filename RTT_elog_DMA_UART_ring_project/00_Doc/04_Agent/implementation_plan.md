# Phase 9 Final RTOS Application Integration Implementation Plan

> 当前执行计划 / Current Active Plan  
> 状态：DESIGN FROZEN / READY FOR CODEX  
> 日期：2026-09-04  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

**Goal:** 在 Phase 1~8 已完成能力上完成最终 FreeRTOS 应用闭环：Button + UART 统一控制、APP Control FSM、4 个产品 Task、Unified Acquisition Service、2 s DHT20 + MPU6050 采集上报、ONCE 完整事务、Indicator 反馈以及最终综合板测。

**Architecture:** `APP -> Service -> Platform -> Impl -> Vendor`。Control Task 是唯一 STOPPED/RUNNING 业务状态 owner；Acquisition Task 是唯一 sensor / shared Software I2C runtime accessor；Communication Task 是 UART Service ownerThread 和 USART1 唯一产品 TX requester；Indicator Task 是 LED semantic executor。

**Primary Spec:** `00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md`

**Important:** 本 Phase 已合并原“RTOS Task / Event Design”和“Final APP Integration”。不存在独立 Phase 10。完成本计划与最终综合板测后，当前项目核心目标完成。

---

# 0. Execution Rules / Stop Conditions

Codex 开始执行前必须完整读取：

```text
00_Doc/00_项目需求/最终功能需求.md
00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/implementation_plan.md
```

全局约束：

```text
APP -> Impl FORBIDDEN
Service -> Impl FORBIDDEN
no direct HAL UART TX product bypass
no second UART RX path
RX SPSC RingBuffer stays lock-free
Communication Task = sole USART1 product TX requester
Control Task = sole STOPPED/RUNNING truth owner
Acquisition Task = sole runtime DHT20/MPU6050/Soft-I2C accessor
4 product Tasks only
CubeMX defaultTask is not a product Task
no I2C mutex in first version
no Queue Set / Event Group expansion in first version
no runtime malloc/free for APP business data flow
APP Queue messages are bounded value-copy messages
initialize dependencies before creating product Threads
no low-power / Tickless / Button EXTI work in this Phase
```

每个 implementation Task：

```text
1. read nearest existing implementation + tests
2. add/adjust contract tests first where practical
3. confirm expected RED for new behavior
4. implement smallest production change
5. run focused tests
6. run regression relevant to changed module
7. perform Coding Standard Review
8. only then continue
```

若出现与冻结架构冲突的问题，不得自行引入第五个业务 Task、新 mutex、新 TX owner 或新 HAL bypass；先记录问题并按设计合同收束。

---

# 1. Task 0 — Baseline Verification

在改代码前记录当前 baseline：

```text
Phase 8 Host regression
Keil production rebuild
current warning count
current UART RX/TX architecture
current generated defaultTask behavior
```

预期 baseline：

```text
Phase 8 Host regression PASS
Keil 0 Error(s)
Phase 8 production path intact
TX DMA target verification still pending
```

本 Task 不做架构修改。

---

# 2. Task 1 — Static Config + APP IPC Contracts

**Modify/Create later:**

```text
00_Config/project_config.h
01_APP/app_ipc_types.h
```

冻结初始资源：

```text
Communication Task   2048 B   ABOVE_NORMAL
Control Task         1024 B   ABOVE_NORMAL
Acquisition Task     1536 B   NORMAL
Indicator Task        768 B   BELOW_NORMAL

Control Queue                  depth 8
Acquisition Command Queue      depth 4
Communication Outbound Queue   depth 8
Indicator Queue                depth 4

PROJECT_COMM_WAIT_TIMEOUT_MS = 20U
```

定义 APP-level value contracts，至少包含：

```text
app_ctrl_source_t
  BUTTON
  UART

Control Queue message
  CONTROL_REQUEST(event + source)
  ONCE_ACQUISITION_FAILED(result)
  ONCE_TX_RESULT(result)

Acquisition command
  START_PERIODIC
  STOP_PERIODIC
  SAMPLE_ONCE

Communication outbound
  CONTROL_RESPONSE
  PERIODIC_REPORT
  ONCE_REPORT

Indicator command
  STOPPED
  RUNNING
  ONCE_SUCCESS
```

要求：

```text
no HAL / FreeRTOS concrete handles in APP IPC public types
no pointer to caller temporary stack data
sensor result copied by value
fixed-size message union / struct
```

完成后做 compile-contract review，确认不存在不必要跨层依赖。

---

# 3. Task 2 — Unified Acquisition Service (Test First)

**Create later:**

```text
02_Service/service_acquisition/service_acquisition.h
02_Service/service_acquisition/service_acquisition.c
```

先在现有 Host test harness 中增加最接近 Service 层的 contract tests；不要猜造第二套 test framework。

必须覆盖：

```text
DHT20 OK + MPU6050 OK
 -> Service OK
 -> complete caller data committed

DHT20 fail + MPU6050 OK
 -> whole acquisition failed
 -> MPU6050 still attempted
 -> caller data unchanged

DHT20 OK + MPU6050 fail
 -> whole acquisition failed
 -> caller data unchanged

DHT20 fail + MPU6050 fail
 -> both attempted
 -> whole acquisition failed
 -> caller data unchanged

request/success/failure/per-sensor statistics correct
last per-sensor result diagnostic state correct
```

实现约束：

```text
DHT20 first
MPU6050 second
always attempt both for diagnostics
atomic temporary result
commit only when both succeed
non-owning Platform sensor references
no Task / Queue / UART / LED logic
no ownership of shared Software I2C lifecycle
```

完成 focused test + Service regression + Coding Standard Review。

---

# 4. Task 3 — Control Task + APP Control FSM (Test First)

**Create later:**

```text
01_APP/app_control.h
01_APP/app_control.c
```

Control Task responsibilities：

```text
PA0 Button polling every 10 ms
service_button_process()
Button gesture -> APP_CTRL event
Control Queue consumer
sole STOPPED/RUNNING FSM
orchestrate Acquisition / Communication / Indicator Queues
```

FSM Context：

```text
state = STOPPED / RUNNING
onceActive
onceSource
nextButtonSampleDeadlineMs
```

`onceActive` 不是第三个业务状态。

先测试：

```text
boot state STOPPED
STOPPED + START -> RUNNING
RUNNING + START -> already running
RUNNING + STOP -> STOPPED
STOPPED + STOP -> already stopped
STOPPED + ONCE -> onceActive, remains STOPPED
RUNNING + ONCE -> already running
ONCE active + UART START/STOP/ONCE -> BUSY
ONCE active + Button START/STOP/ONCE-equivalent -> ignore
STATUS while ONCE active -> STOPPED
Button SINGLE/DOUBLE/LONG map into same FSM
ONCE acquisition failure clears onceActive and never posts success indicator
ONCE TX success clears onceActive and posts ONCE_SUCCESS exactly once
ONCE TX failure clears onceActive and never posts success indicator
```

Button 调度使用 monotonic deadline + bounded Queue receive，不用 `delay(10)` 主循环。

Queue submission failure 必须可观察；Control Task 不得永久阻塞等待 Queue 空间。

---

# 5. Task 4 — Acquisition Task Scheduling (Test First Where Practical)

**Create later:**

```text
01_APP/app_acquisition.h
01_APP/app_acquisition.c
```

Responsibilities：

```text
Acquisition Command Queue consumer
sole runtime caller of Unified Acquisition Service
periodicEnabled execution context
absolute scheduling deadline
publish periodic/ONCE results by value
```

冻结调度：

```text
STOPPED -> queue_receive(WAIT_FOREVER)
START -> immediate first sample
next deadline = first trigger time + 2000 ms
RUNNING -> queue_receive(timeout until deadline)
periodic deadline advances += 2000 ms
missed periods are skipped, never catch-up burst sampled
```

STOP during active synchronous transaction：

```text
no unsafe mid-I2C cancellation
finish current low-level transaction safely
process pending STOP before periodic publish
if STOP observed -> discard stale periodic result
no later periodic acquisition until next START
```

ONCE：

```text
sample success -> Communication Outbound Queue / ONCE_REPORT
sample failure -> Control Queue / ONCE_ACQUISITION_FAILED
```

不得直接 UART TX，不得直接 LED control。

---

# 6. Task 5 — Indicator Task

**Create later:**

```text
01_APP/app_indicator.h
01_APP/app_indicator.c
```

第一版：

```text
Indicator Queue WAIT_FOREVER
 -> map APP indicator command
 -> service_indicator_handle_event()
```

语义：

```text
STOPPED -> OFF
RUNNING -> ON
ONCE_SUCCESS -> blink 3 times, 100 ms on/off -> OFF
```

现有约 600 ms blocking blink 只允许阻塞 Indicator Task。

第一版接受：ONCE 闪烁期间若 START 到达，RUNNING LED event 最多等待本次闪烁完成；业务状态本身必须已经由 Control FSM 更新。

---

# 7. Task 6 — Communication Outbound Integration

**Modify later:**

```text
01_APP/app_communication.h
01_APP/app_communication.c
```

保留现有：

```text
strict CRLF parser
UART Service ownerThread
RX DMA + RingBuffer SPSC
Communication-local HELP / invalid / overflow handling
```

新增：

```text
Communication Outbound Queue binding
nonblocking outbound drain
business response formatting
periodic acquisition report formatting
ONCE report formatting
ONCE TX completion -> Control Queue
```

第一版 loop：

```text
drain outbound queue nonblocking
 -> app_communication_process(PROJECT_COMM_WAIT_TIMEOUT_MS = 20 ms)
 -> drain outbound queue nonblocking
```

不引入 Queue Set 或第二套 wake abstraction。

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

保留：

```text
HELP START STOP ONCE STATUS HELP\r\n
ERR UNKNOWN_COMMAND\r\n
ERR COMMAND_TOO_LONG\r\n
```

完整 report：

```text
ENV,T=...,...\r\n
IMU,AX=...,AY=...,AZ=...,GX=...,GY=...,GZ=...\r\n
```

ONCE：

```text
complete report TX OK -> ONCE_TX_RESULT OK exactly once
any report TX failure -> ONCE_TX_RESULT failure exactly once
```

成功 ONCE 不强制追加 `OK ONCE`；完整 report TX 本身即成功输出。

Queue-full submission of UART control request：

```text
respond ERR BUSY
increment submit failure statistic
```

---

# 8. Task 7 — app_system Final Composition Root

**Modify later:**

```text
01_APP/app_system.c
```

静态持有并装配：

```text
Platform UART / LED / Button / Software I2C / DHT20 / MPU6050
Service UART / Button / Indicator / Acquisition
4 Platform Queues
4 APP contexts
4 Platform Threads
fixed UART DMA / RingBuffer storage
```

初始化顺序必须是：

```text
hardware/platform objects
 -> shared Software I2C
 -> DHT20 / MPU6050
 -> LED / Button
 -> Services
 -> Queues
 -> APP contexts
 -> Threads LAST
```

修复 Phase 8 的临时 `controlHandler = NULL` wiring，Communication control handler 只负责将 UART request 以 `source = UART` 投递到 Control Queue。

Threads 必须全部依赖有效后才创建。

不得在 composition root 引入业务循环。

---

# 9. Task 8 — CubeMX defaultTask Self-Exit

**Modify later only in generated USER CODE area:**

```text
Core/Src/freertos.c
```

最终：

```c
(void)argument;
osThreadExit();
```

删除 defaultTask 产品运行意义；不把它重用为 Control / Acquisition / Communication / Indicator。

验证：

```text
CubeMX user-code-safe placement
no infinite osDelay(1) default loop
4 product Tasks remain the only application Tasks
Idle / Timer Service remain normal RTOS kernel Tasks
```

不得修改无关 CubeMX generated region。

---

# 10. Task 9 — Host Regression + Architecture / Coding Review

运行全部现有 Host regression，并加入 Phase 9 新测试。

至少确认：

```text
Unified Acquisition all-or-nothing contract PASS
Control FSM matrix PASS
Button/UART share one FSM PASS
ONCE completion semantics PASS
Acquisition scheduling contract PASS
Communication formatting/outbound contract PASS
existing UART RX/TX tests PASS
existing Button/Indicator/Sensor tests PASS
```

静态 Review：

```text
4 product Tasks only
no APP -> Impl
no Service -> Impl
no direct HAL UART product TX
no duplicate systemRunning truth
no second Software I2C accessor
no I2C mutex
no APP temporary pointer queued
no Queue Set expansion
no business runtime malloc/free
threads created after dependencies
```

执行 `嵌入式项目C代码设计规范.md` 的 Coding Standard Review。

---

# 11. Task 10 — Keil Production Rebuild

要求：

```text
0 compile errors
no new Phase 9 production warnings
```

检查：

```text
all new production files included in Keil groups
all include paths correct
no host-only symbol leaked to production build
FreeRTOS heap still has safe creation margin with loose initial Task stacks/Queues
```

不要因为第一版 RAM 使用偏宽松而提前压栈。

---

# 12. Task 11 — Final Target Integrated Verification

## 12.1 Boot

```text
boot complete
STOPPED
LED OFF
UART RX active
RTT active
no periodic sensor report
```

## 12.2 Button START

```text
STOPPED SINGLE
 -> RUNNING
 -> LED ON
 -> immediate first complete DHT20 + MPU6050 report
 -> then every 2 s
```

## 12.3 Button STOP

```text
RUNNING LONG >= 3 s
 -> STOPPED
 -> LED OFF
 -> no future periodic report
```

若 STOP 到达 active sensor transaction，允许 transaction 安全收尾，但不得发送 STOP 后的 stale periodic report。

## 12.4 Button ONCE

```text
STOPPED DOUBLE
 -> one complete DHT20 + MPU6050 sample
 -> one complete UART report
 -> TX success
 -> LED blink 3 times
 -> OFF
 -> remain STOPPED
```

失败不成功闪烁。

## 12.5 UART Commands

验证：

```text
START
STOP
ONCE
STATUS
HELP
fragmented command
multiple commands in one RX chunk
RX while TX DMA active
ERR ALREADY_RUNNING
ERR ALREADY_STOPPED
ERR BUSY
ERR ACQUISITION_FAILED
```

Button 与 UART 必须观察同一个 APP state truth。

## 12.6 Shared Software I2C

验证周期运行和 ONCE 下：

```text
DHT20 complete transaction
 -> MPU6050 complete transaction
no interleaving second accessor
```

## 12.7 Failure Visibility

通过 RTT 验证至少：

```text
DHT20 failure
MPU6050 failure
UART TX failure/timeout path where feasible
RingBuffer overflow/error path where feasible
Queue internal fault counters/logging where feasible
```

ONCE sample/TX failure不得成功闪烁。

---

# 13. Task 12 — Resource Measurement, Not Premature Optimization

完整系统稳定运行后记录：

```text
Communication Task stack high-water mark
Control Task stack high-water mark
Acquisition Task stack high-water mark
Indicator Task stack high-water mark
Control Queue peak occupancy
Acquisition Queue peak occupancy
Communication Outbound Queue peak occupancy
Indicator Queue peak occupancy
```

第一轮目标是证明资源足够，不是立刻缩小。

只有在完整业务压力场景有证据后才考虑：

```text
reduce Task stack
reduce Queue depth
adjust priority if measured latency shows need
```

不得为了“看起来省 RAM”牺牲第一版稳定性。

---

# 14. Task 13 — Documentation / Project Closure

只有 Host + Keil + Final Target verification 完成后才能：

```text
mark Phase 9 COMPLETED
record actual verification results
record stack high-water marks / Queue observations
update handoff
update roadmap
update implementation execution record
```

最终路线：

```text
Phase 1 ~ 8
 -> Phase 9 Final RTOS Application Integration
 -> Final Integrated Board Test
 -> PROJECT CORE COMPLETE
```

不要创建 Phase 10。

---

# 15. Execution Record

当前：

```text
Phase 9 design: FROZEN
Phase 9 implementation: IMPLEMENTED
Phase 9 Coding Standard Review: PASS
Phase 9 Architecture Review: PASS
Phase 9 Host tests: PASS / 34 of 34 test groups
Phase 9 Keil build: PASS / 0 Error(s), 14 baseline Warning(s)
Phase 9 target integration: TARGET VERIFICATION REQUIRED
Phase 9 resource measurement: TARGET VERIFICATION REQUIRED
Phase 9 closure: NOT COMPLETE
```

2026-09-04 实现记录：

```text
Production tasks: Communication / Control / Acquisition / Indicator
CubeMX defaultTask: exits through osThreadExit()
APP Queues: 4 bounded value-copy queues
Host compiler policy: -Wall -Wextra -Werror
Keil ARMCC: V5.06 update 7 (build 960)
Program Size: Code=53904 RO-data=1904 RW-data=340 ZI-data=45276
Phase 9 added build warnings: 0
```

目标板验证记录表（连接 STM32F411CEU6 后逐项填写实际观察值）：

```text
[ ] Boot: STOPPED / LED OFF / UART RX active / RTT active / no periodic report
[ ] Button SINGLE: LED ON / immediate complete report / subsequent interval 2000 ms
[ ] Button LONG >= 3 s: LED OFF / no report after STOP
[ ] STOP during sensor transaction: transaction finishes safely / stale report suppressed
[ ] Button DOUBLE in STOPPED: exactly one complete report / TX success / blink 3 times / OFF
[ ] UART: START / STOP / ONCE / STATUS / HELP responses match frozen protocol
[ ] UART framing: fragmented command and multiple commands in one RX chunk
[ ] UART concurrency: USART1 RX remains active while TX DMA is active
[ ] FSM: Button and UART observe the same STOPPED/RUNNING truth
[ ] ONCE busy: START / STOP / ONCE return ERR BUSY; STATUS returns STATUS STOPPED
[ ] I2C: DHT20 transaction then MPU6050 transaction with no second accessor interleaving
[ ] Failure: sensor or UART TX failure is visible through RTT and never success-blinks
[ ] Recovery: RingBuffer overflow/error path and Queue fault counters are observable where feasible
```

资源记录表（完成上述压力场景后填写，不依据静态配置猜测）：

```text
Communication Task stack high-water mark: PENDING TARGET MEASUREMENT
Control Task stack high-water mark: PENDING TARGET MEASUREMENT
Acquisition Task stack high-water mark: PENDING TARGET MEASUREMENT
Indicator Task stack high-water mark: PENDING TARGET MEASUREMENT
Control Queue peak occupancy: PENDING TARGET MEASUREMENT
Acquisition Queue peak occupancy: PENDING TARGET MEASUREMENT
Communication Outbound Queue peak occupancy: PENDING TARGET MEASUREMENT
Indicator Queue peak occupancy: PENDING TARGET MEASUREMENT
```

Phase 8 已有 baseline：

```text
Host regression: PASS
Keil production rebuild: PASS / 0 Error(s)
UART TX DMA target verification: DEFERRED TO PHASE 9
```

不得在目标板场景和资源记录完成前把 Phase 9 标记为 `COMPLETED`。
