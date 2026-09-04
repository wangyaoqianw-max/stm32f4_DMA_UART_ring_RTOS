# Embedded Firmware Architecture Contract

> 文档类型：Architecture Contract  
> 状态：Final Phase 9 Baseline  
> 版本：V3.0  
> 更新时间：2026-09-04  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 总体分层

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

固定依赖：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

APP / Service 不直接依赖 HAL、CubeMX Handle、Impl 私有接口或 FreeRTOS concrete handle。

CubeMX 文件只承担初始化、Scheduler、IRQ / HAL Callback 和薄胶水，不承载长期业务逻辑。

---

# 2. 最终产品运行架构

```text
PA0 Button
 -> Platform Button
 -> Button Service
 -> Control Task
                    \
                     -> APP Control FSM
                    /
USART1 RX DMA
 -> UART Service
 -> RingBuffer
 -> Communication Task
 -> Control Queue
```

APP FSM 输出：

```text
                  +-> Acquisition Command Queue
                  |      -> Acquisition Task
                  |      -> Acquisition Service
APP Control FSM --+      -> DHT20 -> MPU6050 -> Shared Soft I2C
                  |      -> Communication Outbound Queue
                  |
                  +-> Indicator Queue -> Indicator Task -> Indicator Service -> LED
                  |
                  +-> Communication Outbound Queue -> Communication Task -> UART TX DMA
```

核心原则：

```text
Control Task / APP FSM = 唯一 STOPPED / RUNNING 业务状态源
Button / UART          = 控制输入
Acquisition Task       = 唯一 sensor / shared Soft-I2C runtime accessor
Acquisition Service    = 一次完整 DHT20 + MPU6050 采集语义
Communication Task     = UART protocol I/O + sole product TX requester
Indicator Task         = LED semantic executor
UART Service           = transport service
RTT                     = diagnostics
```

---

# 3. 最终四任务合同

产品业务 Task 固定为：

| Task | Responsibility | Initial Stack | Priority |
| --- | --- | ---: | --- |
| Communication | UART RX parser、outbound format/TX | 2048 B | ABOVE_NORMAL |
| Control | Button 10 ms polling、唯一 APP FSM | 1024 B | ABOVE_NORMAL |
| Acquisition | periodic/ONCE scheduling、sensor execution | 1536 B | NORMAL |
| Indicator | LED semantic execution | 768 B | BELOW_NORMAL |

以上为稳定 bring-up 初始值；完整系统目标板运行后用 stack high-water mark / Queue peak occupancy 再优化。

不得增加按硬件模块机械拆分的第五个产品 Task。

CubeMX 自动生成 `defaultTask` 不属于产品任务。实现阶段仅在 USER CODE 中：

```c
(void)argument;
osThreadExit();
```

使其首次执行后退出。不得把它改造成 Control / Acquisition / Communication / Indicator。

FreeRTOS Idle / Timer Service Task 是内核任务，不计入产品任务模型。

---

# 4. APP Control Contract

唯一业务状态：

```text
STOPPED
RUNNING
```

只有 Control Task 可以修改该状态。

允许独立 operation context：

```text
onceActive
onceSource
```

它们不构成第三个业务状态。

统一控制事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

来源：

```text
BUTTON
UART
```

映射：

```text
Button SINGLE -> START
Button DOUBLE -> SAMPLE_ONCE
Button LONG   -> STOP
UART START    -> START
UART STOP     -> STOP
UART ONCE     -> SAMPLE_ONCE
UART STATUS   -> GET_STATUS
HELP          -> Communication local
```

Button / UART 不得维护各自 running flag。

ONCE active 时业务状态仍为 STOPPED；UART START/STOP/ONCE 返回 `ERR BUSY`，STATUS 返回 STOPPED。

---

# 5. APP IPC Contract

第一版永久 APP Queue：

```text
Control Queue                 depth 8
Acquisition Command Queue     depth 4
Communication Outbound Queue  depth 8
Indicator Queue               depth 4
```

全部使用 Platform Queue，bounded + copy-by-value。

禁止：

```text
Queue item points to temporary caller stack buffer
APP business runtime malloc/free
ordinary mutex for APP state orchestration
Queue Set expansion in first version
```

Control Queue 至少承载：

```text
CONTROL_REQUEST(event + source)
ONCE_ACQUISITION_FAILED
ONCE_TX_RESULT
```

Acquisition Command：

```text
START_PERIODIC
STOP_PERIODIC
SAMPLE_ONCE
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

Button event 在 Control Task 内生成，直接进入同一个 FSM，不需要 self-queue。

---

# 6. Unified Acquisition Service Contract

新增统一 Service：

```text
02_Service/service_acquisition/
```

职责：

```text
bind DHT20 + MPU6050 Platform objects
DHT20 read
 -> MPU6050 read
 -> one complete atomic acquisition result
```

不负责：

```text
START / STOP / ONCE business
2 s scheduling
Task / Queue
UART
LED
shared I2C lifecycle
```

第一版成功语义：

```text
DHT20 OK && MPU6050 OK -> acquisition OK
otherwise             -> whole acquisition failed
```

即使 DHT20 失败也继续尝试 MPU6050，以区分 device-specific failure 与 shared bus failure。

使用 temporary data；仅两者都成功才 commit caller output。失败不得修改 caller output。

---

# 7. Acquisition Task Contract

Acquisition Task 是 DHT20、MPU6050、共享 Software I2C 的唯一运行时访问上下文。

Software I2C 当前不增加 Mutex。

执行 context：

```text
periodicEnabled
nextSampleDeadlineMs
```

只是 execution state，不是 APP business truth。

调度：

```text
STOPPED -> Acquisition Command Queue WAIT_FOREVER
START   -> immediate first complete sample
RUNNING -> Queue receive with timeout until absolute deadline
period  -> 2000 ms
next deadline += 2000 ms
```

超期不补采，不连续追赶历史周期。

STOP 到达 active synchronous sensor transaction 时不强制中途取消；允许当前 transaction 安全收尾，但在 periodic result 发布前必须观察 pending STOP，STOP 已到达则丢弃 stale periodic result。

Acquisition Task 不直接 TX，不直接控制 LED。

---

# 8. Communication / UART Contract

## 8.1 RX

稳定路径不变：

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

当前：

```text
Producer = UART Service RX callback
Consumer = Communication Task
```

不得增加普通 Mutex，不建第二套 RX。

## 8.2 TX

正式链：

```text
Communication Task
 -> service_uart_write()
 -> Platform UART write_async()
 -> STM32 UART Impl
 -> HAL_UART_Transmit_DMA()
 -> DMA2_Stream7 Normal
 -> USART1 TX
```

Communication Task 是唯一产品 TX requester。

第一版：

```text
one active TX transaction
RX + TX concurrently allowed
no UART Service TX RingBuffer
no UART Service TX Queue
no TX worker Task
```

`service_uart_write()` 返回意味着 DMA 已不再访问 caller TX buffer。

禁止 direct HAL blocking TX 与 UART Service DMA TX 同时成为产品 owner。

## 8.3 APP outbound wake model

UART Service wait 使用自身 notify，普通 APP Queue send 无法直接唤醒该 wait。

第一版不引入 Queue Set；Communication loop：

```text
drain outbound Queue nonblocking
 -> app_communication_process(20 ms)
 -> drain outbound Queue nonblocking
```

UART RX notify 仍可立即唤醒；20 ms 是 outbound 最坏附加等待基线。

当前不为降低该周期唤醒做低功耗扩展。

---

# 9. UART Application Protocol

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

必须正确处理 fragmented / coalesced byte chunks。

状态/业务响应：

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

Communication-local：

```text
HELP START STOP ONCE STATUS HELP\r\n
ERR UNKNOWN_COMMAND\r\n
ERR COMMAND_TOO_LONG\r\n
```

ONCE 成功不强制追加 `OK ONCE`；完整 acquisition report TX 成功本身即成功输出。

---

# 10. ONCE Transaction Contract

完整成功条件：

```text
DHT20 success
AND MPU6050 success
AND complete UART report TX success
```

链：

```text
Control
 -> ACQ_SAMPLE_ONCE
 -> Acquisition
 -> Communication ONCE_REPORT
 -> service_uart_write()
 -> ONCE_TX_RESULT OK
 -> Control
 -> Indicator ONCE_SUCCESS
 -> blink 3 times
```

任一 acquisition / TX failure：

```text
clear onceActive
no success blink
RTT diagnostics
```

如果 UART transport 本身失败，不要求再通过同一失败通道发送 `ERR TX_FAILED`。

---

# 11. Indicator Contract

Indicator Task 独立执行：

```text
STOPPED -> OFF
RUNNING -> ON
ONCE_SUCCESS -> blink 3 times, 100 ms on/off -> OFF
```

现有阻塞 blink 允许留在 Indicator Task 中，不得阻塞 Control / Acquisition / Communication。

若闪烁期间 START 到达，RUNNING LED command 最多等待本次约 600 ms 闪烁结束；第一版接受该反馈延迟，业务状态必须立即由 Control FSM 生效。

---

# 12. ISR / Concurrency Contract

ISR / HAL Callback 只允许：

```text
capture
necessary data copy
lightweight state update
ISR-safe notify
quick exit
```

禁止 ISR：

```text
Button gesture FSM
Software I2C transaction
full command parser
Sensor business
LED blocking blink
malloc/free
ordinary mutex
heavy formatted logs
```

---

# 13. Config / Context / Data

```text
Config  = 模块应怎样工作
Context = 模块当前怎样运行
Data    = 模块当前有什么结果
```

例如：

```text
2 s acquisition period        -> Config
STOPPED/RUNNING/onceActive    -> Control Context
periodicEnabled/deadline      -> Acquisition execution Context
DHT20 + MPU6050 sample        -> Data
Queue depth                   -> Config
```

产品配置统一进入 `00_Config/project_config.h`。

---

# 14. app_system Composition Root

`app_system.c` 是静态对象与依赖装配入口。

负责持有：

```text
Platform devices
Services
4 APP Queues
4 APP contexts
4 Platform Threads
fixed communication storage
```

初始化顺序：

```text
Platform/hardware objects
 -> shared bus / sensors / basic devices
 -> Services
 -> Queues
 -> APP contexts
 -> Threads LAST
```

依赖未初始化前不得创建可运行 product thread。

---

# 15. Channel / Log Contract

```text
USART1 -> product control + product data
RTT    -> initialization / state / diagnostics / errors
```

正常运行禁止逐 UART byte、逐 DMA step、逐 I2C bit/ACK、逐 Button poll 刷日志。

---

# 16. Phase / Scope Contract

最终路线：

```text
Phase 1 ~ 8
 -> Phase 9 Final RTOS Application Integration
 -> Final Integrated Board Test
 -> Project Core Complete
```

没有独立 Phase 10。

当前 Phase 9 设计：

```text
00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md
```

当前执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
```

当前不做：

```text
low-power / Tickless policy
Button EXTI wake
SPI / LCD / GUI
W25Q64 / AT24C02
Bluetooth
Roll/Pitch/Yaw / DMP / filters
unneeded framework expansion
```
