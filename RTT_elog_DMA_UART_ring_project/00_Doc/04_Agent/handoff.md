# 工程长期记忆与交接说明

更新时间：2026-09-04

> 本文件是 AI Agent 与人工开发者恢复工程上下文时的长期入口。  
> 详细业务需求以 `00_Doc/00_项目需求/最终功能需求.md` 为准。  
> 架构合同以 `00_Doc/04_Agent/architecture.md` 为准。  
> 阶段路线以 `00_Doc/04_Agent/development_roadmap.md` 为准。  
> 当前施工计划以 `00_Doc/04_Agent/implementation_plan.md` 为准。

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
I2C        : Software I2C over GPIO
Input      : PA0 User Key
Indicator  : PC13 Status LED
```

最终目标：形成一个按键 + UART 可控制、DHT20 + MPU6050 统一采集、UART 上报、RTT 诊断的完整 FreeRTOS 数据采集系统，并沉淀可复用的 Platform / Service 能力。

---

# 2. 稳定架构合同

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

CubeMX 生成文件只承担初始化、IRQ / HAL Callback、Scheduler 和薄胶水，不承载长期业务逻辑。

---

# 3. 已关闭阶段

```text
Phase 1  GPIO STM32 Impl                       COMPLETED
Phase 2  Board Resource + CubeMX              COMPLETED
Phase 3  Software I2C                         COMPLETED
Phase 4  LED Module                           COMPLETED
Phase 5  Button Module                        COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 6  DHT20 Environment Module             COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 7  MPU6050 Motion Module                COMPLETED / HOST + KEIL + TARGET VERIFIED
```

DHT20 / MPU6050 已在 PB6/PB7 共享 Software I2C 上完成真实板级验证；两者均为可复用 Platform sensor capability，不拥有共享 I2C 生命周期。

产品统一采集 / 上报周期：

```text
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

---

# 4. 当前阶段

```text
Phase 8 — UART Application Communication
STATUS: IMPLEMENTATION COMPLETED / HOST + KEIL VERIFIED
TARGET VERIFICATION DEFERRED TO PHASE 9

Phase 9 — RTOS Task / Event / IPC Design
STATUS: DESIGN BASELINE FROZEN / READY FOR DOCUMENTATION AND IMPLEMENTATION PLAN
```

Phase 8 正式专项设计：

```text
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
```

当前已有执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
UART Application Communication Phase 8 Implementation Plan
```

Phase 8 已完成 STM32 UART TX DMA、UART Service 同步 TX transaction、Communication CRLF 命令解析与 APP Control event outlet。

Phase 9 已完成高层 RTOS 资源模型讨论并冻结第一版基线；下一步应先生成 Phase 9 正式设计文档与实施计划，再开始编码。当前没有执行 Phase 9 代码修改。

---

# 5. UART 当前真实基线

已稳定：

```text
USART1 115200 8N1
PA9  TX
PA10 RX
RX DMA2_Stream2 / Channel 4 / Circular
RX IDLE / HT / TC
Platform UART RX path
UART Service RX path
SPSC RingBuffer
Communication Task sole RX consumer
```

TX 当前状态必须准确区分：

```text
blocking Platform TX                    EXISTS
Platform public write_async()           EXISTS
Platform TX_COMPLETE event type         EXISTS
CubeMX USART1 TX DMA                    READY
STM32 Impl async TX                     IMPLEMENTED / HOST + KEIL VERIFIED
UART Service TX                         IMPLEMENTED / HOST + KEIL VERIFIED
APP Communication CRLF protocol         IMPLEMENTED / HOST + KEIL VERIFIED
TX DMA target verification              DEFERRED TO PHASE 9
```

2026-09-04 人工已在 CubeMX 开启 USART1 TX DMA：

```text
DMA2_Stream7
Channel 4
Memory -> Peripheral
Memory increment ENABLE
Peripheral increment DISABLE
BYTE alignment
DMA_NORMAL
Priority LOW
IRQ priority 5
```

因此旧文档中“UART DMA RX / TX VERIFIED”的笼统写法已修正。

---

# 6. Phase 8 冻结 UART 架构

## RX

```text
USART1 RX
 -> DMA2_Stream2 Circular
 -> STM32 UART Impl
 -> PLATFORM_UART_EVENT_RX_DATA
 -> UART Service
 -> SPSC RingBuffer
 -> Communication Task
```

保持现有 SPSC，不加普通 Mutex，不建第二套 RX。

## TX

正式目标链：

```text
Communication Task
 -> service_uart_write()
 -> platform_uart_write_async()
 -> STM32 UART Impl
 -> HAL_UART_Transmit_DMA()
 -> DMA2_Stream7 Normal
 -> HAL_UART_TxCpltCallback()
 -> PLATFORM_UART_EVENT_TX_COMPLETE
 -> UART Service
 -> owner Task wake
```

第一版：

```text
one active TX transaction only
RX + TX simultaneously allowed
second TX while active -> BUSY
no TX RingBuffer
no TX Queue
no TX worker Task
```

Communication Task 是 USART1 唯一产品 TX requester。

---

# 7. Platform UART TX 冻结语义

UART device lifecycle 与 RX/TX transaction 状态分离：

```text
Device: CREATED / INITIALIZED / STARTED / STOPPED
RX:     rxActive false / true
TX:     txActive false / true
```

允许：

```text
RX active + TX active
```

`platform_uart_write_async()` 成功只代表 DMA transaction 已启动。

TX buffer 由 caller 持有；直到：

```text
TX_COMPLETE
CANCELED / TX
terminal TX error
```

之前必须保持有效且不得修改。

HAL TX complete 必须：

```text
snapshot TX pointer/length
 -> clear TX context
 -> emit TX_COMPLETE
```

先清状态，再通知上层。

STM32 Impl 的 `cancel()` 在 Phase 8 补齐 TX / BOTH，并保证停止后 DMA 不再访问 caller buffer。

---

# 8. UART Service TX 冻结语义

UART Service 第一版不公开 async TX API，而提供 Task-friendly：

```c
service_uart_write(service, data, length, timeoutMs)
```

内部仍是：

```text
Platform async TX
 -> DMA
 -> Task wait
 -> TX terminal condition
 -> return result
```

强保证：

```text
service_uart_write() returns
=> DMA no longer accesses caller TX buffer
```

现有 `consumerThread` 语义在 Phase 8 升级为 `ownerThread`：

```text
ownerThread = UART Service sole Task execution context
```

当前产品：

```text
ownerThread = Communication Task
```

TX Context：

```text
txState = IDLE / ACTIVE
txResult
```

TX single transaction error 不自动把健康 RX Service 设成 ERROR。

---

# 9. TX wait / timeout 关键边界

公共 `service_uart_wait_event()` 会根据 RingBuffer 真值持续重建 `RX_AVAILABLE`，因此不能拿它等待 TX completion，否则 RingBuffer 非空时可能 busy loop。

Phase 8 实现内部专用 TX wait：

```text
Notify = wake hint
TX Context = truth
```

并使用：

```text
platform_time_get_ms()
```

维护总 timeout deadline。

无关 RX notification 不得重新开始完整 timeout。

TX timeout：

```text
deadline reached
 -> cancel TX
 -> confirm transaction stopped
 -> return PLATFORM_ERR_TIMEOUT
```

---

# 10. UART Application Command Protocol

严格命令：

```text
START\r\n
STOP\r\n
ONCE\r\n
STATUS\r\n
HELP\r\n
```

第一版：

```text
strict CRLF
uppercase only
case-sensitive
no trim
no arguments
no malloc/free
```

RingBuffer read chunk 不是 command boundary，必须处理：

```text
"STA" + "RT\r\n"
"START\r\nSTATUS\r\nHELP\r\n"
"START\r" + "\n"
```

建议：

```text
PROJECT_COMM_COMMAND_LINE_BUFFER_SIZE = 32U
```

Line Assembler Context：

```text
commandLine[]
commandLength
pendingCr
discardLine
```

超长 / malformed line 整行丢弃直到下一个 CRLF；不能从同行尾部重新识别合法命令。

Protocol invalid input 不进入 `APP_COMMUNICATION_STATE_ERROR`。

---

# 11. APP Control Event Outlet

公共业务事件独立于 UART：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

UART：

```text
START  -> APP_CTRL_START
STOP   -> APP_CTRL_STOP
ONCE   -> APP_CTRL_SAMPLE_ONCE
STATUS -> APP_CTRL_GET_STATUS
HELP   -> Communication local
```

Button 后续也映射同一套事件：

```text
SINGLE -> APP_CTRL_START
DOUBLE -> APP_CTRL_SAMPLE_ONCE
LONG   -> APP_CTRL_STOP
```

handler 返回值只表示 request submission result，不表示业务执行结果。

APP Communication 不得保存 STOPPED / RUNNING 真值。

---

# 12. Response 边界

Phase 8 可直接实现：

```text
HELP START STOP ONCE STATUS HELP\r\n
ERR UNKNOWN_COMMAND\r\n
ERR COMMAND_TOO_LONG\r\n
```

以下发送条件由 APP Control FSM result 决定：

```text
OK START\r\n
OK STOP\r\n
ERR ALREADY_RUNNING\r\n
ERR ALREADY_STOPPED\r\n
STATUS RUNNING\r\n
STATUS STOPPED\r\n
```

`ONCE` 不得在仅收到请求时提前声明完整业务成功。

---

# 13. USART1 / RTT 通道职责

正式职责：

```text
USART1 -> product control + product data
RTT    -> diagnostics / EasyLogger
```

历史 `uartMutexHandle`、`USART1_mutex_Init()`、`fputc -> HAL_UART_Transmit()` 不得重新进入正式产品 TX 路径。

禁止 direct HAL TX 与 UART Service DMA TX 同时成为 USART1 owner。

---

# 14. Phase 9 冻结任务模型

第一版固定为 4 个产品业务任务：

```text
1. Control Task
2. Acquisition Task
3. Communication Task
4. Indicator Task
```

不要建立“一个硬件模块一个 Task”的模型。

职责冻结：

## Control Task

```text
Button 10 ms polling
 -> Button Service gesture recognition
 -> map to APP_CTRL event

UART control request
 -> Control Queue

APP Control FSM
 -> unique STOPPED / RUNNING business truth
 -> Acquisition command
 -> Indicator command
 -> Communication business response
```

Button polling 与 APP Control FSM 放在同一个 Control Task 中，避免额外建立纯 Button Task。

Control Task 应通过带 timeout 的 Queue receive / monotonic deadline 保持约 10 ms Button sample 节奏；不得用长时间阻塞业务操作占住 Control Task。

## Acquisition Task

```text
sole runtime sensor acquisition context
sole Software I2C transaction execution context
DHT20 -> MPU6050 sequential acquisition
2 s periodic scheduling when RUNNING
one-shot acquisition when requested
```

APP Control FSM 决定业务是否 RUNNING；Acquisition Task 只保存必要的执行上下文，不成为第二份业务状态真值。

同步 DHT20 / MPU6050 读操作不可安全中途取消。STOP 的业务状态应立即由 Control FSM 生效；如 STOP 到达时已有 periodic sensor transaction 正在执行，允许当前底层 transaction 收尾，但不得在确认 STOP 后继续产生新的周期上报。

## Communication Task

```text
sole USART1 product RX command consumer
sole USART1 product TX requester
command parser
outbound response/report formatting
service_uart_write()
```

通信线程继续保持 UART Service `ownerThread` 身份。

## Indicator Task

```text
sole LED semantic execution context
STOPPED -> OFF
RUNNING -> ON
ONCE TX SUCCESS -> blink 3 times -> OFF
```

当前 `service_indicator_handle_event()` 的阻塞闪烁允许放在 Indicator Task 中，不得阻塞 Control / Communication / Acquisition Task。

---

# 15. Phase 9 冻结业务数据流

主控制流：

```text
UART RX DMA
 -> UART Service / RingBuffer
 -> Communication Task
 -> Control Queue
 -> Control Task / APP FSM
```

Button 控制流：

```text
PA0
 -> Control Task polling
 -> Button Service
 -> APP_CTRL event
 -> APP FSM
```

周期采集流：

```text
Control Task START
 -> Acquisition Command Queue
 -> Acquisition Task periodic mode
 -> Unified Acquisition Service
 -> DHT20
 -> MPU6050
 -> Communication Outbound Queue
 -> Communication Task
 -> UART DMA TX
```

ONCE 完整事务：

```text
Control Task
 -> SAMPLE_ONCE
 -> Acquisition Task
 -> Unified Acquisition Service
 -> Communication Task
 -> UART TX success
 -> completion/result back to Control Task
 -> Indicator Task ONCE_SUCCESS
 -> blink 3 times
```

关键边界：

```text
queue submission success != business execution success
acquisition success != UART TX success
ONCE success feedback only after UART TX success
```

因此 ONCE completion 必须存在从 Communication 回到 Control 的完成事件；不得由 Communication Task 直接决定 LED 业务语义。

---

# 16. Phase 9 IPC 基线

第一版使用 Platform Queue + 现有 UART Service Thread Notify，不引入 Mutex / Semaphore / Event Group / Queue Set。

建议主要 APP Queue：

```text
Control Queue                 depth 8
Acquisition Command Queue     depth 4
Communication Outbound Queue  depth 8
Indicator Queue               depth 4
```

这些深度是“先完整跑通”的初始值，不是最终优化值。

Queue contract：

```text
value-copy messages only
no pointer to temporary stack object
bounded queue
no runtime malloc/free business design
single owner for each business state / hardware side effect
```

Control Queue 为多生产者入口，至少接收：

```text
UART control request
Communication completion/result
```

Button event 因本身就在 Control Task 中产生，可直接进入同一个 FSM，不需要再绕一次 Queue。

UART 请求需要保留 source metadata，使 Control FSM 能区分：

```text
UART source   -> may require product response
BUTTON source -> normally no command-response text
```

`app_ctrl_event_t` 继续保持与输入设备无关；可由 APP IPC message 额外携带 source。

Acquisition result 通过值拷贝送入 Communication Outbound Queue；不要使用共享可变 sensor buffer + Mutex。

Communication Task 格式化自己的 TX buffer，并同步调用 `service_uart_write()`；依赖其强保证，函数返回后 DMA 已不再访问 caller TX buffer。

---

# 17. Unified Acquisition Service 基线

Phase 9 新增统一采集 Service，用于把两个 Platform sensor capability 组合成一个完整采集动作：

```text
Service Acquisition
 -> platform_dht20_read()
 -> platform_mpu6050_read()
```

建议目录：

```text
02_Service/service_acquisition/
```

职责：

```text
bind DHT20 + MPU6050 Platform objects
execute DHT20 then MPU6050 sequentially
return one complete acquisition sample
keep APP independent from sensor call ordering details
```

第一版以“完整 sample transaction”为主，不在 Service 内建立 RTOS Task / Queue，也不拥有 Software I2C lifecycle。

Software I2C 并发策略继续冻结为：

```text
one Acquisition Task runtime accessor
 -> DHT20 full transaction
 -> MPU6050 full transaction
```

因此第一版不增加 I2C Mutex。

---

# 18. 第一版任务资源策略

目标优先级：

```text
first make the complete system run correctly
then measure
then shrink resources
```

初始 Task stack 采用偏宽松值：

```text
Communication Task  1536 bytes
Control Task        1024 bytes
Acquisition Task    1536 bytes
Indicator Task       768 bytes
```

初始 Priority 建议：

```text
Communication Task  ABOVE_NORMAL
Control Task        NORMAL
Acquisition Task    NORMAL
Indicator Task      BELOW_NORMAL
```

以上 stack / priority 是 Phase 9 第一版 bring-up baseline，不是最终性能结论。

系统完整运行并通过目标板场景验证后，再根据：

```text
Task stack high-water mark
queue peak occupancy
control latency
UART RX/TX behavior
2 s acquisition timing
button gesture stability
```

逐步收缩 stack / queue，并决定是否调整 priority。

不要为了第一版节省几百字节 SRAM 而提前把系统资源压到边界。

---

# 19. CubeMX defaultTask 冻结处理

CubeMX / CMSIS-RTOS2 自动生成的 `defaultTask` 在当前配置下无法直接删除，不将其改造成第五个产品业务任务。

处理方案冻结为：

```text
CubeMX creates defaultTask
 -> scheduler starts
 -> defaultTask first runs
 -> osThreadExit()
 -> task terminates
```

`StartDefaultTask()` 只在 CubeMX USER CODE 区域执行：

```c
(void)argument;
osThreadExit();
```

不要保留历史：

```text
for (;;)
{
    osDelay(1);
}
```

也不要为了“利用 defaultTask”而把 Control Task 等产品任务绑定到 CubeMX generated task，否则 task ownership / stack / priority 会分裂为两套配置来源。

稳定运行阶段“4 个产品任务”不等于 RTOS Viewer 只有 4 个线程；FreeRTOS 自身还可能存在：

```text
Idle Task
Timer Service Task
```

这是内核资源，不计入产品任务模型。

---

# 20. Phase 9 当前不再阻塞的设计问题

以下关键问题已冻结，不再作为编码前置争议：

```text
4 个业务 Task，而不是 5 个硬件 Task
defaultTask first-run self exit
APP Control FSM 是唯一 STOPPED / RUNNING truth
Button polling + Control FSM 同 Task
Acquisition Task sole Software I2C runtime accessor
Communication Task sole USART1 product TX owner
Indicator Task owns blocking LED feedback
ONCE success waits for UART TX completion
APP IPC uses bounded value-copy queues
no I2C mutex in first version
initial task/queue resources intentionally loose
```

仍可在 Phase 9 详细设计 / 实现时微调、但不阻塞开工的细节：

```text
exact Communication outbound wake/poll implementation
exact START first periodic sample timing
rare simultaneous-command / queue-full response wording
final stack and queue sizes after target measurements
```

这些属于实现策略或异常边界，不应推翻当前任务与 IPC 架构。

---

# 21. 后续路线

```text
Phase 8  UART Application Communication   IMPLEMENTATION COMPLETED / HOST + KEIL VERIFIED
                                           TARGET VERIFICATION DEFERRED TO PHASE 9
Phase 9  RTOS Task / Event Design          BASELINE FROZEN
Phase 10 Final APP Integration
Final Integrated Board Test
```

Phase 9 下一步：

```text
write formal Phase 9 design document
update implementation plan
implement Unified Acquisition Service
implement Control / Acquisition / Indicator APP contexts
extend Communication outbound IPC
wire queues / tasks in app_system
make CubeMX defaultTask self-exit
complete USART1 TX DMA target verification
bring up full RTOS skeleton before Phase 10 business closure
```

Phase 10 重点：

```text
STOPPED / RUNNING FSM final behavior
START / STOP / ONCE / STATUS business result
state-dependent UART response
2 s periodic acquisition/report
ONCE TX success -> Indicator 3 blinks
full integrated scenario closure
```

---

# 22. 当前停止点

```text
Phase 8 implementation and documentation synchronized
Host regression PASS
Keil production rebuild PASS: 0 Error(s), 14 existing non-Phase-8 warnings
Coding Standard Review PASS
Architecture Review PASS
Independent Phase-8 target-board smoke NOT RUN

Phase 9 RTOS task / IPC high-level design baseline FROZEN
Phase 9 code NOT STARTED
```

下一步不是直接零散编码，而是先把本次冻结内容落成 Phase 9 正式设计文档和实施计划，然后按计划实现。

后续编码前，执行 Agent 必须先读取：

```text
00_Doc/00_项目需求/最终功能需求.md
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/implementation_plan.md
00_Doc/04_Agent/handoff.md
```

Coding Standard Review 必须在每个实现 Task 提交前执行。
