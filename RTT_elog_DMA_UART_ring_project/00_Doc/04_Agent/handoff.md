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
STATUS: DESIGN FROZEN / PLAN READY / NOT STARTED
```

正式专项设计：

```text
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
```

当前执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
UART Application Communication Phase 8 Implementation Plan
```

本次只同步了文档，没有修改任何 production source / header / CubeMX 配置。

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
STM32 Impl async TX                     NOT IMPLEMENTED
UART Service TX                         NOT IMPLEMENTED
TX DMA production path                  NOT YET VERIFIED
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

现有 `consumerThread` 语义在 Phase 8 计划升级为 `ownerThread`：

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

Button 后续也映射同一套事件。

Phase 8 只冻结逻辑 handler outlet；实际 Queue / Task / direct call 留到 Phase 9。

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

以下仅冻结格式方向，发送条件由未来 APP FSM result 决定：

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

当前 CubeMX USER CODE 仍存在历史：

```text
uartMutexHandle
USART1_mutex_Init()
fputc -> HAL_UART_Transmit(&huart1)
```

Phase 8 production implementation 应移除或彻底退出正式运行路径，禁止 direct HAL TX 与 UART Service DMA TX 同时成为 USART1 owner。

---

# 14. Phase 8 不做

```text
APP STOPPED / RUNNING FSM
permanent APP Control consumer Task
permanent Queue / IPC policy
Acquisition Service / Acquisition Task
Button permanent IPC
Indicator permanent IPC
2 s integrated sensor reporting
ONCE final business transaction
TX queue framework
binary protocol / JSON / CLI shell
```

这些按路线进入 Phase 9 / Phase 10。

---

# 15. 后续路线

```text
Phase 8  UART Application Communication   DESIGN FROZEN / PLAN READY / NOT STARTED
Phase 9  RTOS Task / Event Design
Phase 10 Final APP Integration
Final Integrated Board Test
```

Phase 9 重点：

```text
Communication Task
Acquisition Task
Indicator Task
Button permanent context
APP Control consumer context
control IPC
Acquisition result -> Communication IPC
Indicator event delivery
priority / stack / buffering
Unified Acquisition Service
```

Phase 10 重点：

```text
STOPPED / RUNNING FSM
START / STOP / ONCE / STATUS business result
state-dependent UART response
2 s periodic acquisition/report
ONCE TX success -> Indicator 3 blinks
```

---

# 16. 当前停止点

```text
Phase 8 design documentation synchronized
implementation_plan ready
NO production code changes performed in this documentation-sync turn
```

下一次若开始编码，执行 Agent 必须先读取：

```text
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/implementation_plan.md
00_Doc/04_Agent/handoff.md
```

Coding Standard Review 必须在每个实现 Task 提交前执行。
