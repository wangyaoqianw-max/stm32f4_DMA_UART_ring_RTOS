# Final RTOS Application Integration Phase 9 设计

> 文档类型：Final Integration Design  
> 状态：DESIGN FROZEN  
> 日期：2026-09-04  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 设计目标

Phase 9 是本项目最后一个软件实现阶段，合并原“RTOS Task / Event Design”和“Final APP Integration”。

本阶段完成：

```text
Button + UART unified control
APP Control FSM
4 permanent product Tasks
APP Queue / IPC
Unified Acquisition Service
2 s DHT20 + MPU6050 acquisition/report
ONCE full transaction
Indicator feedback
USART1 TX DMA target verification
final integrated target-board verification
```

完成 Phase 9 和最终综合板测后，当前项目核心目标视为完成。

不再保留独立 Phase 10。

---

# 2. 最终分层与主链

固定依赖：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

最终控制链：

```text
PA0 Button
 -> Platform Button
 -> Button Service
 -> Control Task
 -> APP Control FSM

USART1 RX
 -> DMA Circular
 -> UART Service / RingBuffer
 -> Communication Task
 -> APP_CTRL event
 -> Control Queue
 -> Control Task
 -> APP Control FSM
```

最终采集/上报链：

```text
APP Control FSM
 -> Acquisition Command Queue
 -> Acquisition Task
 -> Unified Acquisition Service
 -> Platform DHT20
 -> Platform MPU6050
 -> Communication Outbound Queue
 -> Communication Task
 -> UART Service
 -> Platform UART async TX
 -> DMA2_Stream7
 -> USART1 TX
```

LED 链：

```text
APP Control FSM
 -> Indicator Queue
 -> Indicator Task
 -> Indicator Service
 -> Platform LED
```

---

# 3. 最终产品任务模型

第一版固定 4 个产品业务 Task：

| Task | 主要职责 | 初始 Stack | Priority |
| --- | --- | ---: | --- |
| Communication Task | UART RX parser、产品 TX、report/response format | 2048 B | ABOVE_NORMAL |
| Control Task | Button 10 ms polling、唯一 APP Control FSM | 1024 B | ABOVE_NORMAL |
| Acquisition Task | 2 s scheduling、ONCE、唯一 sensor/I2C runtime accessor | 1536 B | NORMAL |
| Indicator Task | LED semantic execution、ONCE 3 次闪烁 | 768 B | BELOW_NORMAL |

这些数值是 bring-up baseline，不是最终资源极限。

第一版原则：

```text
make complete system run correctly
 -> target stress / scenario verification
 -> inspect Task stack high-water mark and Queue peak occupancy
 -> shrink only after evidence
```

当前阶段不做低功耗优化，不引入 Tickless Idle、Button EXTI wake 或 sleep policy。

---

# 4. CubeMX defaultTask

CubeMX / CMSIS-RTOS2 自动生成 `defaultTask`，当前配置无法直接删除。

不把它改造成产品任务。

冻结处理：

```text
scheduler starts
 -> defaultTask first runs
 -> osThreadExit()
 -> task terminates
```

只修改 `Core/Src/freertos.c` 的 USER CODE 区：

```c
(void)argument;
osThreadExit();
```

删除原业务运行语义：

```c
for (;;) {
    osDelay(1);
}
```

FreeRTOS Idle Task / Timer Service Task 属于内核资源，不计入 4 个产品任务。

---

# 5. APP Control FSM

唯一业务状态：

```text
STOPPED
RUNNING
```

只有 Control Task 可以修改该状态。

Button、Communication、Acquisition、Indicator 不保存第二份 `systemRunning` 真值。

Control Task 允许维护与业务状态正交的 operation context：

```text
onceActive
onceSource
```

`onceActive` 表示 STOPPED 状态下正在执行一次 ONCE transaction，不是第三个业务状态。

控制来源：

```text
APP_CTRL_SOURCE_BUTTON
APP_CTRL_SOURCE_UART
```

事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

Button 映射：

```text
SINGLE -> START
DOUBLE -> SAMPLE_ONCE
LONG   -> STOP
```

UART 映射：

```text
START  -> START
STOP   -> STOP
ONCE   -> SAMPLE_ONCE
STATUS -> GET_STATUS
HELP   -> Communication local
```

---

# 6. Control FSM 业务语义

## 6.1 START

STOPPED 且 `onceActive == false`：

```text
submit ACQ_START_PERIODIC
 -> state = RUNNING
 -> submit INDICATOR_RUNNING
 -> UART source: response OK START
```

RUNNING：

```text
no duplicate start
UART source: ERR ALREADY_RUNNING
```

STOPPED 且 ONCE 正在执行：

```text
no state change
UART source: ERR BUSY
Button source: ignore + optional RTT DEBUG/WARN summary
```

## 6.2 STOP

RUNNING：

```text
submit ACQ_STOP_PERIODIC
 -> state = STOPPED
 -> submit INDICATOR_STOPPED
 -> UART source: response OK STOP
```

STOPPED 且无 ONCE：

```text
UART source: ERR ALREADY_STOPPED
Button source: no business action
```

ONCE 正在执行：

```text
no forced cancellation of active sensor/UART transaction
UART source: ERR BUSY
Button source: ignore
```

## 6.3 SAMPLE_ONCE

RUNNING：

```text
no extra sample
UART source: ERR ALREADY_RUNNING
```

STOPPED 且无 ONCE：

```text
onceActive = true
remember onceSource
submit ACQ_SAMPLE_ONCE
remain STOPPED
```

STOPPED 且已有 ONCE：

```text
UART source: ERR BUSY
Button source: ignore
```

ONCE 成功定义：

```text
DHT20 OK
AND MPU6050 OK
AND complete product report TX OK
```

成功后：

```text
onceActive = false
 -> submit INDICATOR_ONCE_SUCCESS
 -> remain STOPPED
```

ONCE 的 UART 成功反馈以完整 sensor report 本身为成功结果；第一版不额外强制发送 `OK ONCE`，避免把“report TX success”之后又引入第二个成功 TX transaction。

ONCE acquisition 失败：

```text
onceActive = false
no success blink
RTT WARN/ERROR
UART source: Communication may send ERR ACQUISITION_FAILED
```

ONCE report TX 失败：

```text
onceActive = false
no success blink
RTT WARN/ERROR
```

如果 UART transport 本身失败，不要求再尝试通过同一失败通道发送 `ERR TX_FAILED`。

## 6.4 STATUS

始终返回 APP FSM 业务状态：

```text
STATUS STOPPED
STATUS RUNNING
```

ONCE 执行期间业务状态仍为 STOPPED，因此 `STATUS` 返回 STOPPED。

---

# 7. Unified Acquisition Service

新增：

```text
02_Service/service_acquisition/service_acquisition.h
02_Service/service_acquisition/service_acquisition.c
```

职责仅为：

```text
bind DHT20 + MPU6050 objects
execute one complete ordered sample
DHT20 first
MPU6050 second
return one atomic complete result
retain diagnostic statistics/status
```

不负责：

```text
2 s scheduling
START / STOP / ONCE business
Task / Queue
UART format / TX
LED
shared I2C lifecycle
```

建议公共数据：

```c
typedef struct
{
    platform_dht20_measurement_t environment;
    platform_mpu6050_measurement_t motion;
} service_acquisition_data_t;
```

配置：

```c
typedef struct
{
    platform_dht20_t *dht20;
    platform_mpu6050_t *mpu6050;
} service_acquisition_config_t;
```

主要 API：

```c
platform_error_t service_acquisition_init(
    service_acquisition_t *service,
    const service_acquisition_config_t *config);

platform_error_t service_acquisition_sample(
    service_acquisition_t *service,
    service_acquisition_data_t *data);

platform_error_t service_acquisition_deinit(
    service_acquisition_t *service);
```

成功语义冻结为 all-or-nothing：

```text
DHT20 OK && MPU6050 OK -> complete acquisition OK
otherwise             -> acquisition FAILED
```

即使第一个传感器失败，也继续尝试第二个，以提高共享 Software I2C / device-specific 故障诊断能力。

输出采用 atomic commit：

```text
sample into local temporary data
 -> both sensor reads OK
 -> copy complete data to caller output
```

任一失败时不得修改 caller 原有 `data`。

Service 可维护：

```text
requestCount
successCount
failureCount
dht20FailureCount
mpu6050FailureCount
lastDht20Result
lastMpu6050Result
```

但不在每个正常 2 s sample 内制造大量日志。

---

# 8. Acquisition Task 调度

Acquisition Task 是 DHT20、MPU6050 和共享 Software I2C 的唯一运行时访问者。

执行上下文只表示调度状态：

```text
periodicEnabled
nextSampleDeadlineMs
```

它不是 APP RUNNING 业务真值。

Command：

```text
ACQ_START_PERIODIC
ACQ_STOP_PERIODIC
ACQ_SAMPLE_ONCE
```

## 8.1 STOPPED scheduling

```text
platform_queue_receive(acquisitionQueue, WAIT_FOREVER)
```

无周期唤醒。

## 8.2 START timing

第一版冻结：START 后立即执行第一次完整采集，然后以该次触发时间为基准每 2000 ms 一次。

```text
START command
 -> periodicEnabled = true
 -> sample immediately
 -> next deadline = sampleTrigger + 2000 ms
```

## 8.3 RUNNING scheduling

使用：

```text
queue_receive(timeout = remaining time to absolute deadline)
```

不是：

```text
delay(2000)
```

周期推进：

```text
nextDeadline += PROJECT_ACQUISITION_PERIOD_MS
```

避免把 sensor execution time 累积到周期中。

若 task 超期，不补采历史样本；将 deadline 推进到未来最近的有效周期。

## 8.4 STOP during active sample

DHT20 / Software I2C 同步 transaction 不强制中途取消。

冻结语义：

```text
current low-level transaction finishes safely
 -> process pending STOP before next scheduling
 -> no new periodic acquisition after STOP
```

为避免 STOP 已请求后再发布陈旧 periodic report，periodic sample 完成后、向 Communication Queue 发布前，Acquisition Task 应非阻塞处理已排队的 control command；如果观察到 STOP，则丢弃该 periodic result。

## 8.5 SAMPLE_ONCE

Control FSM 只会在 STOPPED 时提交 `ACQ_SAMPLE_ONCE`。

成功：

```text
Acquisition Task
 -> Communication Outbound Queue / ONCE report
```

失败：

```text
Acquisition Task
 -> Control Queue / ONCE_ACQUISITION_FAILED
```

不直接控制 LED，不直接发送 UART。

---

# 9. APP IPC 类型与 Queue

新增共享 APP IPC 合同：

```text
01_APP/app_ipc_types.h
```

只放 APP 模块之间的值类型消息，不放 HAL / FreeRTOS concrete handle。

第一版 Queue：

| Queue | Depth | Producer | Consumer |
| --- | ---: | --- | --- |
| Control Queue | 8 | Communication、Acquisition | Control |
| Acquisition Command Queue | 4 | Control | Acquisition |
| Communication Outbound Queue | 8 | Control、Acquisition | Communication |
| Indicator Queue | 4 | Control | Indicator |

全部采用 `platform_queue_t` + copy-by-value。

禁止 Queue item 持有临时 stack buffer 指针。

## 9.1 Control Queue message

至少支持：

```text
CONTROL_REQUEST
ONCE_ACQUISITION_FAILED
ONCE_TX_RESULT
```

`CONTROL_REQUEST` 携带：

```text
app_ctrl_event_t event
app_ctrl_source_t source
```

`ONCE_*` completion 携带 `platform_error_t result`。

Button 事件本身在 Control Task 中产生，直接进入 FSM，不需要自发自收 Queue。

## 9.2 Communication Outbound message

至少支持：

```text
CONTROL_RESPONSE
PERIODIC_REPORT
ONCE_REPORT
```

消息只携带 semantic enum / acquisition data，不携带 caller temporary string pointer。

Communication Task 在自己的 stack/local fixed buffer 中格式化文本，并同步调用 `service_uart_write()`。

## 9.3 Indicator message

只需要：

```text
STOPPED
RUNNING
ONCE_SUCCESS
```

Indicator Task 映射到现有 `service_indicator_handle_event()`。

---

# 10. Communication Task 最终行为

保留现有 UART RX DMA + RingBuffer + strict CRLF parser。

Communication Task 继续是：

```text
UART Service ownerThread
sole USART1 product TX requester
```

新增 Communication Outbound Queue consumer。

当前 `service_uart_wait_event()` 无法被普通 APP Queue send 唤醒，第一版不引入 Queue Set / 新 RTOS abstraction。

冻结简单实现：

```text
drain outbound queue nonblocking
 -> app_communication_process(PROJECT_COMM_WAIT_TIMEOUT_MS)
 -> drain outbound queue nonblocking
```

将 `PROJECT_COMM_WAIT_TIMEOUT_MS` 从 1000 ms 调整为第一版 20 ms，使 outbound response/report 最坏额外等待约 20 ms。

UART RX notify 仍可立即唤醒 Communication Task；20 ms timeout 只是解决 APP outbound Queue 与 UART Service notify 的多源等待问题。

当前阶段不优化该周期唤醒的功耗。

## 10.1 Product response

冻结状态相关响应：

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

保留 Communication-local：

```text
HELP START STOP ONCE STATUS HELP\r\n
ERR UNKNOWN_COMMAND\r\n
ERR COMMAND_TOO_LONG\r\n
```

## 10.2 Sensor report

一次完整报告可由 Communication Task 作为一个 logical outbound message 串行发送两行：

```text
ENV,T=25.34,H=62.18\r\n
IMU,AX=0.013,AY=-0.021,AZ=0.998,GX=0.12,GY=-0.42,GZ=0.08\r\n
```

对 ONCE，只有完整 report transaction 成功后才向 Control Queue 提交 `ONCE_TX_RESULT = OK`。

任一 report TX 失败则提交失败 result，不触发成功 LED。

---

# 11. Control Task Button 调度

Control Task 同时承担：

```text
Button 10 ms polling
Control Queue event handling
APP Control FSM
```

不得简单：

```text
for (;;) {
    poll button;
    delay(10);
}
```

冻结为 deadline-driven queue wait：

```text
remaining = nextButtonSampleDeadline - now
platform_queue_receive(controlQueue, remaining)
```

收到 Queue message 时立即处理；deadline 到达时：

```text
platform_button_read()
 -> service_button_process(nowMs)
 -> map gesture to APP_CTRL
 -> execute FSM directly
```

按键采样 deadline 每次按 10 ms 基准推进，避免处理 Queue 消息导致长期漂移。

---

# 12. Indicator Task

Indicator Task 唯一执行 LED semantic side effect。

```text
queue_receive(WAIT_FOREVER)
 -> service_indicator_handle_event()
```

`ONCE_SUCCESS` 当前阻塞约 600 ms，只阻塞 Indicator Task。

如果 ONCE 闪烁期间发生 START，RUNNING indicator command 可能等待闪烁完成后执行；第一版接受该最多约 600 ms 的 LED feedback 延迟，不增加 indicator cancellation/state machine。

业务状态已经由 Control FSM 立即改变，不受 LED 动画阻塞影响。

---

# 13. Queue overflow / IPC failure policy

初始 Queue 深度故意留有余量，正常业务不应触顶。

原则：

```text
no infinite producer blocking
no silent drop of control/completion message
queue full = observable internal runtime fault / backpressure
```

建议 Task producer 使用 0 或短 bounded timeout；禁止 Control Task 因 Queue 满永久阻塞。

UART control request 无法提交到 Control Queue：

```text
Communication statistics increment
 -> response ERR BUSY
```

Acquisition / Indicator / internal completion Queue 满：

```text
RTT ERROR/WARN
statistics increment where appropriate
preserve task liveness
```

第一版不为理论极端队列满设计复杂 rollback transaction；通过合理 queue depth、单 owner 和低消息速率保证正常运行不触发该路径，并在最终压力测试中验证 peak occupancy。

---

# 14. app_system Composition Root

`app_system.c` 继续是静态对象与依赖装配唯一入口。

负责持有：

```text
Platform UART / LED / Button / Software I2C / DHT20 / MPU6050 objects
Service UART / Button / Indicator / Acquisition objects
4 platform_thread_t objects
4 platform_queue_t objects
APP Control / Acquisition / Communication / Indicator contexts
fixed UART DMA/RingBuffer storage
```

初始化顺序必须满足依赖：

```text
construct/bind hardware resources
 -> initialize shared Software I2C
 -> initialize DHT20 / MPU6050
 -> initialize LED / Button
 -> initialize Services
 -> create Queues
 -> initialize APP contexts
 -> create Threads with all dependencies already valid
```

特别修复当前 Phase 8 的潜在顺序问题：不得让 Communication Task 在 `service_uart_init()` 完成前获得运行机会。由于 scheduler 尚未启动时创建线程不会立即运行，但最终 composition 仍应按“依赖先初始化、线程最后创建”的清晰顺序组织。

不得在 APP / Service runtime 使用 malloc/free 作为业务数据流方案。

---

# 15. Static project config

Phase 9 在 `00_Config/project_config.h` 增加/调整：

```text
PROJECT_COMM_TASK_STACK_SIZE_BYTES       2048U
PROJECT_COMM_TASK_PRIORITY               ABOVE_NORMAL
PROJECT_COMM_WAIT_TIMEOUT_MS             20U

PROJECT_CONTROL_TASK_STACK_SIZE_BYTES    1024U
PROJECT_CONTROL_TASK_PRIORITY            ABOVE_NORMAL
PROJECT_CONTROL_QUEUE_DEPTH              8U

PROJECT_ACQUISITION_TASK_STACK_SIZE_BYTES 1536U
PROJECT_ACQUISITION_TASK_PRIORITY         NORMAL
PROJECT_ACQUISITION_QUEUE_DEPTH           4U

PROJECT_INDICATOR_TASK_STACK_SIZE_BYTES   768U
PROJECT_INDICATOR_TASK_PRIORITY           BELOW_NORMAL
PROJECT_INDICATOR_QUEUE_DEPTH             4U

PROJECT_COMM_OUTBOUND_QUEUE_DEPTH         8U
```

保留：

```text
PROJECT_BUTTON_SAMPLE_PERIOD_MS = 10U
PROJECT_ACQUISITION_PERIOD_MS = 2000U
PROJECT_INDICATOR_BLINK_COUNT = 3U
PROJECT_INDICATOR_BLINK_ON_MS = 100U
PROJECT_INDICATOR_BLINK_OFF_MS = 100U
```

---

# 16. Verification

Host / contract tests 至少覆盖：

```text
Acquisition Service both-success atomic commit
DHT fail / MPU success -> whole acquisition fail
DHT success / MPU fail -> whole acquisition fail
both fail diagnostic counters
caller data unchanged on failure

Control STOPPED/RUNNING matrix
UART/Button same FSM
ONCE active BUSY behavior
STATUS during ONCE = STOPPED
ONCE failure no indicator success
ONCE TX success -> indicator success

Acquisition START immediate first sample
2 s absolute deadline scheduling model
STOP prevents future periodic publish
no catch-up burst after overrun

Communication outbound formatting
ERR BUSY on control Queue submission failure
ONCE TX completion posted exactly once
```

Keil：

```text
0 compile errors
no new Phase 9 warnings
```

Target board final verification：

```text
boot -> STOPPED / LED OFF / UART RX active
Button SINGLE -> RUNNING / LED ON / immediate first report / then every 2 s
Button LONG -> STOPPED / no further periodic report / LED OFF
Button DOUBLE in STOPPED -> one complete report -> TX success -> 3 blinks -> OFF
UART START/STOP/ONCE/STATUS/HELP correct
UART and Button share one state truth
DHT20 + MPU6050 sequential shared Soft I2C stable
USART1 RX remains active while TX DMA operates
RingBuffer fragmented/coalesced commands still correct
sensor/UART failure visible through RTT
ONCE failure never success-blinks
```

使用 debugger / RTOS viewer 记录四个产品 Task 的 stack high-water mark，并检查 Queue peak occupancy；只有最终系统稳定后才收缩 stack / depth。

---

# 17. 最终范围外

本阶段不增加：

```text
Phase 10
Tickless low-power policy
Button EXTI wake
I2C mutex without second accessor
DHT20/MPU6050 separate Tasks
TX worker / TX RingBuffer framework
Queue Set abstraction
Roll/Pitch/Yaw / DMP / filters
SPI/LCD/GUI
W25Q64 / AT24C02
Bluetooth
```

---

# 18. 冻结结论

Phase 9 即最终软件集成阶段。

```text
Phase 1 ~ 8 completed capabilities
 -> Phase 9 Final RTOS Application Integration
 -> Final Integrated Board Test
 -> Project core complete
```

编码执行必须以本文档、`最终功能需求.md`、`architecture.md`、`requirements.md`、`execution_rules.md` 和当前 `implementation_plan.md` 为共同约束。
