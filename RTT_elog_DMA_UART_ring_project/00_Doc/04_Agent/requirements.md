# Final Acquisition System Requirements

> 文档类型：Agent Requirements Baseline  
> 状态：Final Phase 9 Baseline  
> 版本：V3.0  
> 更新时间：2026-09-04  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 文档定位

本文件是 AI Agent / Codex 进行设计、编码和 Review 时使用的长期需求摘要。

业务行为权威文件：

```text
00_Doc/00_项目需求/最终功能需求.md
```

最终软件设计：

```text
00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md
```

当前执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
```

原 RTOS Task/Event 阶段与 Final APP Integration 已合并为最终 Phase 9；不存在独立 Phase 10。

---

# 2. 项目最终闭环

```text
PA0 Button
 -> Platform Button
 -> Button Service
 -> Control Task ----------------------+
                                        |
USART1 RX DMA                           v
 -> UART Service / RingBuffer    APP Control FSM
 -> Communication Task                 |
 -> Control Queue ----------------------+
                                        |
                     +------------------+------------------+
                     |                  |                  |
                     v                  v                  v
              Acquisition Cmd    Communication Out   Indicator Queue
                     |                  |                  |
                     v                  v                  v
              Acquisition Task   Communication Task   Indicator Task
                     |                  |                  |
                     v                  v                  v
           Acquisition Service    UART Service      Indicator Service
              DHT20 -> MPU6050      TX DMA               LED
                     |
             Shared Software I2C
                     |
               Platform GPIO
```

诊断：

```text
APP / Service
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

UART：

```text
RX = DMA2_Stream2 / Channel 4 / Circular / VERIFIED
TX = DMA2_Stream7 / Channel 4 / Normal
Platform/Service TX = IMPLEMENTED / HOST + KEIL VERIFIED
TX target verification = Phase 9 final integration scope
```

---

# 4. 系统状态与控制

APP 层维护唯一业务状态：

```text
STOPPED
RUNNING
```

启动后：

```text
State       = STOPPED
LED         = OFF
UART RX     = ACTIVE
RTT Log     = ACTIVE
Periodic acquisition = DISABLED
```

允许与业务状态正交的 ONCE operation context：

```text
onceActive
onceSource
```

它们不是第三个业务状态。

统一控制事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

统一来源：

```text
BUTTON
UART
```

---

# 5. Button Requirements

硬件/手势参数：

```text
active LOW / Pull-Up
sample 10 ms
debounce 30 ms
double window 300 ms
long press 3000 ms
```

映射：

```text
SINGLE -> START
DOUBLE -> SAMPLE_ONCE
LONG   -> STOP
```

业务矩阵：

| State / Operation | SINGLE | DOUBLE | LONG |
| --- | --- | --- | --- |
| STOPPED, no ONCE | RUNNING | start ONCE, remain STOPPED | no business action |
| RUNNING | remain RUNNING | no extra sample | STOPPED |
| STOPPED, ONCE active | ignore | ignore | ignore |

Button polling 在 Control Task 中执行，不建立独立 Button Task。

---

# 6. UART Application Requirements

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

RingBuffer read boundary 不是 command boundary；必须支持 fragmented / coalesced chunks。

业务响应：

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

ONCE active 时：

```text
START / STOP / ONCE -> ERR BUSY
STATUS               -> STATUS STOPPED
```

成功 ONCE 不要求额外 `OK ONCE`；完整 report TX 成功即成功输出。

Communication 不维护 STOPPED/RUNNING。

---

# 7. UART Transport Requirements

RX 保持：

```text
USART1 RX
 -> DMA Circular + IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform UART Event
 -> UART Service
 -> SPSC RingBuffer
 -> Communication Task
```

不得：

```text
create second UART RX path
add ordinary mutex to current SPSC
```

TX：

```text
Communication Task
 -> service_uart_write()
 -> Platform UART write_async
 -> STM32 UART Impl
 -> HAL_UART_Transmit_DMA
 -> DMA2_Stream7 Normal
 -> USART1
```

第一版：

```text
one active TX transaction only
RX + TX concurrently allowed
no UART TX RingBuffer
no UART TX Queue/worker inside UART Service
Communication Task = sole product TX requester
```

强保证：

```text
service_uart_write() returns
=> DMA no longer accesses caller buffer
```

USART1 只用于产品控制/数据；RTT 用于诊断。禁止 direct HAL blocking TX 重新成为产品旁路。

---

# 8. Unified Acquisition Service Requirements

Phase 9 新增统一 Acquisition Service，组合已经验证的 Platform DHT20 + MPU6050。

职责：

```text
DHT20 read
 -> MPU6050 read
 -> complete atomic acquisition data
```

成功语义：

```text
DHT20 OK && MPU6050 OK -> OK
otherwise             -> FAILED
```

诊断要求：即使 DHT20 失败，也继续尝试 MPU6050。

输出要求：

```text
use temporary data
commit caller output only if both sensors succeed
failure must leave caller output unchanged
```

Service 不负责 Task、Queue、2 s scheduling、UART、LED 或 shared I2C lifecycle。

---

# 9. Acquisition Scheduling Requirements

统一周期：

```text
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

Acquisition Task 是唯一 DHT20 / MPU6050 / shared Software I2C runtime accessor。

第一版不增加 I2C Mutex。

STOPPED：

```text
wait Acquisition Command Queue indefinitely
```

START：

```text
immediate first complete acquisition/report
then every 2000 ms
```

RUNNING scheduling：

```text
Queue receive with timeout until absolute deadline
nextDeadline += 2000 ms
```

不能用简单 `delay(2000)` 作为主调度模型。

Overrun：不补采历史样本，不 catch-up burst。

STOP during active synchronous sensor transaction：不强制中途取消；允许 transaction 安全收尾，但 STOP 后不得发布 stale periodic report，也不得继续新的周期采集。

---

# 10. ONCE Requirements

STOPPED 且没有 active ONCE：

```text
SAMPLE_ONCE
 -> DHT20
 -> MPU6050
 -> complete UART report
 -> UART TX success
 -> Indicator ONCE_SUCCESS
 -> LED blink 3 times
 -> OFF
 -> remain STOPPED
```

成功定义：

```text
DHT20 success
AND MPU6050 success
AND complete report TX success
```

任一失败：

```text
clear onceActive
no success blink
RTT diagnostic
```

如果 acquisition 失败且请求来源为 UART，可返回：

```text
ERR ACQUISITION_FAILED\r\n
```

如果 UART transport 本身失败，不要求再尝试经同一失败通道发送 TX error response。

---

# 11. Sensor Data / Product Report

DHT20：

```text
temperature
relative humidity
```

MPU6050：

```text
Accel X/Y/Z raw + g
Gyro X/Y/Z raw + deg/s
```

第一版 report：

```text
ENV,T=25.34,H=62.18\r\n
IMU,AX=0.013,AY=-0.021,AZ=0.998,GX=0.12,GY=-0.42,GZ=0.08\r\n
```

RUNNING 每 2 s 一组完整 report；START 后第一组立即产生。

不实现姿态融合 / Roll / Pitch / Yaw / DMP / filters。

---

# 12. LED Requirements

```text
STOPPED                -> OFF
RUNNING                -> ON
RUNNING periodic TX    -> keep ON
ONCE TX SUCCESS        -> blink 3 times -> OFF
ONCE sample/TX failure -> keep OFF
```

闪烁：

```text
100 ms ON / 100 ms OFF / 3 times
```

Indicator Task 承担当前阻塞式 blink，不能阻塞 Control / Acquisition / Communication。

---

# 13. Final RTOS Task Model

固定 4 个产品任务：

| Task | Initial Stack | Priority | Core Responsibility |
| --- | ---: | --- | --- |
| Communication | 2048 B | ABOVE_NORMAL | UART RX parser + sole product TX |
| Control | 1024 B | ABOVE_NORMAL | Button polling + sole APP FSM |
| Acquisition | 1536 B | NORMAL | sensor scheduling/execution |
| Indicator | 768 B | BELOW_NORMAL | LED semantic execution |

第一版资源故意偏宽松。完整系统稳定后根据 high-water mark / Queue peak occupancy 再收缩。

当前不讨论低功耗，不引入 Tickless / Button EXTI wake。

CubeMX `defaultTask` 不作为第五个产品任务；实现时仅在 USER CODE 内首次运行后 `osThreadExit()`。

---

# 14. APP IPC Requirements

```text
Control Queue                 depth 8
Acquisition Command Queue     depth 4
Communication Outbound Queue  depth 8
Indicator Queue               depth 4
```

要求：

```text
Platform Queue abstraction
bounded
value-copy
no temporary stack pointer
no infinite producer blocking
queue full must be observable
```

第一版不引入 Queue Set / Event Group / APP state mutex。

Communication Outbound Queue 无法直接唤醒 UART Service private notify wait；第一版采用：

```text
nonblocking outbound drain
 -> app_communication_process(20 ms)
 -> nonblocking outbound drain
```

不为此扩展新的 RTOS abstraction。

---

# 15. Software I2C Requirements

当前冻结：

```text
Master-only
7-bit
synchronous
Platform GPIO based
microsecond bit timing
no internal mutex
```

DHT20 + MPU6050 共用 PB6/PB7。

唯一 Acquisition Task 串行执行：

```text
DHT20 complete transaction
 -> MPU6050 complete transaction
```

只有未来出现真实第二个并发访问者时才讨论 transaction-level synchronization。

---

# 16. ISR / Memory / Layering Requirements

ISR / HAL Callback 仅允许：

```text
capture
necessary copy
lightweight state update
ISR-safe notify
quick exit
```

禁止：

```text
Button gesture FSM in ISR
Software I2C in ISR
full parser in ISR
Sensor business in ISR
blocking LED blink in ISR
malloc/free in ISR/business path
ordinary mutex in ISR
heavy formatted logs in ISR
```

分层：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

核心运行数据优先 static / caller-owned / value-copy storage。

---

# 17. RTT / EasyLogger Requirements

```text
UART -> product control + product data
RTT  -> init / state / acquisition / diagnostics / errors
```

建议：

```text
INFO  initialization / START / STOP / ONCE / state changes
DEBUG periodic acquisition/report summary
WARN  recoverable sensor/I2C/UART/queue issues
ERROR critical initialization/runtime failures
```

禁止正常运行逐 UART byte、DMA step、I2C bit/ACK、Button poll 刷日志。

---

# 18. Final Acceptance

必须验证：

```text
Boot -> STOPPED / LED OFF / UART RX active / no periodic report
Button SINGLE -> RUNNING / LED ON / immediate first report / every 2 s
Button LONG -> STOPPED / LED OFF / no future periodic report
Button DOUBLE in STOPPED -> one report / TX success / 3 blinks / remain STOPPED
UART START/STOP/ONCE/STATUS/HELP correct
Button + UART use one APP state truth
RX remains active during TX DMA
DHT20 + MPU6050 shared Soft-I2C stable
ONCE acquisition/TX failure never success-blinks
relevant failures visible through RTT
```

完成后记录：

```text
4 Task stack high-water marks
Queue peak occupancy / observed margins
Host regression result
Keil result
Target integrated result
```

---

# 19. Scope / Active Phase

必须完成：

```text
Phase 9 Final RTOS Application Integration
Final Integrated Board Test
```

当前不做：

```text
low-power / Tickless policy
Button EXTI wake
SPI / LCD / GUI
W25Q64 / AT24C02
Bluetooth
Roll / Pitch / Yaw / DMP / filters
complex binary UART protocol
unneeded framework expansion
```

当前：

```text
Phase 8 = IMPLEMENTATION COMPLETED / HOST + KEIL VERIFIED
Phase 9 = DESIGN FROZEN / READY FOR CODEX
```

Codex 必须从 `00_Doc/04_Agent/implementation_plan.md` Task 0 开始执行。
