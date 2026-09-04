# Final Acquisition System Requirements

> 文档类型：Agent Requirements Baseline  
> 状态：Baseline  
> 版本：V2.3  
> 更新时间：2026-09-04  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 文档定位

本文件是 AI Agent 执行设计、编码和 Review 时使用的长期需求摘要。

最终业务行为权威文件：

```text
00_Doc/00_项目需求/最终功能需求.md
```

当前工程基于已验证的 UART DMA RX + RingBuffer + FreeRTOS 主线，增加 GPIO、Software I2C、DHT20、MPU6050、按键、UART 应用通信和 APP Control FSM，形成最终综合闭环。

---

# 2. 项目最终闭环

```text
KEY -> Platform Button -> Button Service ----+
                                              |
                                              v
PC -> UART RX -> DMA -> RingBuffer -> Command Parser
                                              |
                                              v
                                       APP Control FSM
                                              |
                                +-------------+-------------+
                                |                           |
                                v                           v
                              LED                   Sensor Acquisition
                                                            |
                                              +-------------+-------------+
                                              |                           |
                                              v                           v
                                            DHT20                      MPU6050
                                              \                           /
                                               +------- Soft I2C --------+
                                                            |
                                                     Platform GPIO

Acquisition Result
    -> Communication Task
    -> UART Service
    -> Platform UART async TX
    -> UART DMA TX
    -> PC Serial Assistant

APP / Service Runtime State
    -> Service Log
    -> Platform Log
    -> EasyLogger
    -> SEGGER RTT
```

---

# 3. 硬件与软件环境

```text
MCU        : STM32F411CEU6 / Cortex-M4F
Flash      : 512 KB
RAM        : 128 KB
UART       : USART1 / 115200 8N1
Sensor     : DHT20 + MPU6050
Input      : PA0 User Key
Indicator  : PC13 Status LED
I2C        : Software I2C over PB6/PB7
RTOS       : CMSIS-RTOS2 + FreeRTOS
Log        : EasyLogger + SEGGER RTT
Toolchain  : Keil MDK-ARM + STM32CubeMX
```

DHT20 与 MPU6050 共用同一条 Software I2C，总线硬件连通性已完成实板确认。

USART1 DMA 当前：

```text
RX = DMA2_Stream2 / Channel 4 / Circular / VERIFIED production path
TX = DMA2_Stream7 / Channel 4 / Normal / CubeMX configuration READY
```

TX DMA Platform / Service production path 尚待 Phase 8 实现和验证。

---

# 4. 系统状态与控制事件

APP 层维护唯一业务状态：

```text
STOPPED
RUNNING
```

启动完成后：

```text
Acquisition = STOPPED
LED         = OFF
UART RX     = ACTIVE
RTT Log     = ACTIVE
```

统一控制事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

按键和 UART 只产生控制输入，不得分别维护采集状态。

---

# 5. Button 基线

```text
active LOW / Pull-Up
sample 10 ms
debounce 30 ms
double window 300 ms
long press 3000 ms
```

Platform Button：

```text
HIGH / LOW -> PRESSED / RELEASED
```

Button Service：

```text
PRESSED / RELEASED + nowMs -> NONE / SINGLE / DOUBLE / LONG
```

业务映射由 APP 决定：

| 当前状态 | 按键事件 | 结果 |
| --- | --- | --- |
| STOPPED | SINGLE | START -> RUNNING |
| STOPPED | DOUBLE | SAMPLE_ONCE，保持 STOPPED |
| STOPPED | LONG | 无额外动作 |
| RUNNING | SINGLE | 保持 RUNNING |
| RUNNING | DOUBLE | 不额外采样 |
| RUNNING | LONG | STOP -> STOPPED |

---

# 6. UART Application Communication Phase 8

正式专项设计：

```text
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
```

当前执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
```

状态：

```text
DESIGN FROZEN / PLAN READY / NOT STARTED
```

## 6.1 UART RX

必须复用：

```text
USART1 RX
 -> DMA Circular + IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform UART Event
 -> UART Service
 -> SPSC RingBuffer
 -> Communication Task
 -> Command Parser
 -> APP Control Event
```

不得建立第二套 UART RX，不给当前 SPSC 增加普通 Mutex。

## 6.2 UART TX

目标正式链：

```text
Communication Task
 -> UART Service
 -> Platform UART write_async
 -> STM32 UART Impl
 -> HAL_UART_Transmit_DMA
 -> DMA2_Stream7 Normal
 -> USART1 TX
 -> PC
```

第一版要求：

```text
one active TX transaction only
RX + TX concurrently allowed
second TX while active -> BUSY
no TX RingBuffer
no TX Queue
no TX worker Task
```

Platform async TX buffer 由 caller 持有，在 TX_COMPLETE / CANCELED / terminal error 前必须保持有效且不得修改。

UART Service 提供面向 Task 的同步完成语义；内部使用 DMA + notify wait。

强保证：

```text
service_uart_write() returns
=> DMA no longer accesses caller TX buffer
```

## 6.3 TX timeout / cancel

无关 RX notification 不得延长 TX 总 timeout。

使用：

```text
platform_time_get_ms()
wraparound-safe elapsed / remaining time
```

TX timeout 后必须 cancel TX 并确认 transaction 已结束，再向调用者返回 TIMEOUT。

## 6.4 UART command framing

第一阶段：

```text
START\r\n
STOP\r\n
ONCE\r\n
STATUS\r\n
HELP\r\n
```

协议规则：

```text
strict CRLF
uppercase only
case-sensitive
no trim
no arguments
no dynamic allocation
```

RingBuffer read boundary 不是 command boundary，必须支持 fragmented / coalesced arbitrary chunks。

命令行 buffer 建议：

```text
PROJECT_COMM_COMMAND_LINE_BUFFER_SIZE = 32U
```

超长 / malformed line 整行丢弃到下一个 CRLF，不能从同行尾部重新解析合法命令。

非法协议输入不进入 APP Communication fatal ERROR。

## 6.5 Command responsibility

```text
HELP                    -> Communication local
START                    -> APP_CTRL_START
STOP                     -> APP_CTRL_STOP
ONCE                     -> APP_CTRL_SAMPLE_ONCE
STATUS                   -> APP_CTRL_GET_STATUS
```

Phase 8 只冻结逻辑 control handler，不冻结永久 Queue / Task / direct-call 机制。

handler 的返回值只表示 request submission result，不表示业务执行结果。

Communication 不得维护 STOPPED / RUNNING。

## 6.6 Communication-local responses

Phase 8 可直接实现：

```text
HELP START STOP ONCE STATUS HELP\r\n
ERR UNKNOWN_COMMAND\r\n
ERR COMMAND_TOO_LONG\r\n
```

状态相关 response 的发送条件必须等待 APP FSM result。

`ONCE` 不能在仅收到请求时提前声明完整业务成功。

## 6.7 USART1 owner

冻结：

```text
Communication Task = sole USART1 product TX requester
```

未来 Acquisition Task 不直接发送 UART，只向 Communication Task 提交 acquisition result。

---

# 7. USART1 / RTT 职责隔离

```text
UART -> 产品/业务数据与控制
RTT  -> 初始化、运行状态、采集状态、诊断与异常
```

Phase 8 production implementation 后，不允许旧：

```text
printf / fputc -> HAL_UART_Transmit(&huart1)
```

继续作为正式 USART1 旁路。

---

# 8. 周期采集与上报

统一周期：

```text
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

RUNNING：

```text
DHT20
 -> MPU6050
 -> organize result
 -> Communication Task
 -> UART report
 -> RTT DEBUG summary
```

STOPPED 不执行周期采集 / 上报。

---

# 9. DHT20 / MPU6050 基线

DHT20 与 MPU6050 均已实现为 Platform lightweight sensor capability，并完成目标板验证。

共同规则：

```text
share platform_i2c_t
non-owning bus reference
no private Task
no private mutex
no service_dht20 / service_mpu6050 empty wrapper
```

后续统一 Acquisition Service 串行调用两者。

---

# 10. Software I2C 基线

```text
Master-only
7-bit
synchronous
Platform GPIO based
microsecond bit timing
no internal mutex
```

第一阶段单一 Acquisition 执行上下文串行访问 DHT20 / MPU6050；只有真实多访问者出现时才增加 transaction-level synchronization。

---

# 11. LED 产品语义

```text
STOPPED               -> OFF
RUNNING               -> ON
RUNNING periodic TX   -> keep ON
ONCE TX SUCCESS        -> blink 3 times -> OFF
ONCE sample/TX failure -> keep OFF
```

只有成功完成业务发送后才提交 `ONCE_SUCCESS`。

---

# 12. RTT / EasyLogger

正式链：

```text
APP / Service
 -> service_log
 -> Platform Log
 -> EasyLogger Adapter
 -> EasyLogger / SEGGER RTT
```

正常运行禁止逐 UART byte、逐 DMA step、逐 I2C bit / ACK、逐 Button polling 刷日志。

---

# 13. 推荐 RTOS 执行模型

已确认方向：

```text
Communication Task
Acquisition Task
Indicator Task
```

Phase 9 冻结：

```text
permanent Button context
APP Control consumer execution context
control IPC
Acquisition result -> Communication IPC
Indicator event delivery
priority / stack / buffering
Unified Acquisition Service / Task
```

---

# 14. 分层要求

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

APP / Service 不得直接依赖 HAL、CubeMX Handle 或 Impl 私有接口。

---

# 15. ISR / 并发 / 内存

ISR / HAL Callback 只允许：

```text
capture
necessary copy
lightweight state update
ISR-safe notify
quick exit
```

禁止 blocking、ordinary mutex、malloc/free、完整协议、Sensor 业务、LED delay、大量格式化日志。

核心链路优先 static / caller-owned storage。

---

# 16. 当前范围冻结

必须完成：

```text
GPIO STM32 Impl + board verification
LED
Platform Button / Button Service
Software I2C
DHT20
MPU6050 basic 6-axis
UART reusable TX DMA path
UART START / STOP / ONCE / STATUS / HELP
APP Control FSM
2 s acquisition + UART report
ONCE success LED feedback
RTT diagnostic coverage
final integrated board test
```

当前不做：

```text
Roll / Pitch / Yaw
DMP / filters
SPI / LCD / GUI
W25Q64 / AT24C02
Bluetooth
complex binary protocol
TX queue framework
Button EXTI
无需求驱动框架扩展
```

---

# 17. 当前 Active Phase

```text
Phase 8 — UART Application Communication
DESIGN FROZEN / PLAN READY / NOT STARTED
```

进入 production 编码前，执行 Agent 必须读取：

```text
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/implementation_plan.md
```
