# Final Acquisition System Requirements

> 文档类型：Agent Requirements Baseline  
> 状态：Baseline  
> 版本：V2.2  
> 更新时间：2026-09-04  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 文档定位

本文件是 AI Agent 执行设计、编码和 Review 时使用的长期需求摘要。

最终业务行为的权威需求文件：

```text
00_Doc/00_项目需求/最终功能需求.md
```

当前工程继续基于已验证的 UART DMA + RingBuffer + FreeRTOS 主线，增加 GPIO、Software I2C、DHT20、MPU6050、按键和 APP Control FSM，形成最终综合闭环。

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

Sensor Data
    -> APP / Communication
    -> UART Service
    -> Platform UART
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

DHT20 与 MPU6050 共用同一条 Software I2C，总线硬件连通性已在 2026-09-04 实板确认。

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

按键第一阶段：

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

Phase 5 已完成 Host + Keil + Target 验证。

---

# 6. UART 命令

第一阶段：

```text
START\r\n
STOP\r\n
ONCE\r\n
STATUS\r\n
HELP\r\n
```

必须复用：

```text
USART1
 -> DMA Circular + IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform UART Event
 -> UART Service
 -> SPSC RingBuffer
 -> APP Communication Task
 -> Command Parser
 -> APP Control Event
```

UART Service 不直接控制 LED、DHT20、MPU6050 或 APP 状态。

---

# 7. 周期采集与上报

第一阶段统一产品周期已更新为：

```text
Acquisition / Report Period = 2000 ms
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

RUNNING 每 2 s：

```text
DHT20
 -> MPU6050
 -> organize report
 -> UART TX
 -> RTT DEBUG summary
```

STOPPED 不执行周期采集 / 上报。

该 2 s 周期同时符合 DHT20 规格书“约每 2 秒测量 1 次”的推荐，以减少频繁激活导致的自热影响。

---

# 8. DHT20 Phase 6 冻结需求

正式设计：

```text
00_Doc/02_架构设计/DHT20_Phase1设计.md
```

只实现 Platform 设备能力：

```text
Platform DHT20
 -> Platform Software I2C
 -> Platform GPIO
```

不新增：

```text
service_dht20
impl_dht20
platform_device_t
runtime registry / manager
malloc/free
DHT20 private task
DHT20-owned mutex
Fake I2C / test-only Ops abstraction
```

第一版对象：

```text
platform_dht20_t
- platform_i2c_t *i2c
- initialized
```

DHT20 只引用共享 I2C，不拥有总线生命周期，`platform_dht20_deinit()` 不得调用 `platform_i2c_deinit()`。

公共 API：

```text
platform_dht20_init()
platform_dht20_read()
platform_dht20_deinit()
```

单次测量协议：

```text
7-bit address 0x38
write AC 33 00
STOP
wait >= 80 ms
new START
read 7 bytes
final byte NACK
STOP
```

读取必须检查：

```text
Busy
frame CRC8
OTP CRC_flag
CalibrationEnable
```

错误语义：

```text
address NACK             -> PLATFORM_ERR_NOT_FOUND
underlying I2C error     -> preserve underlying error
Busy                     -> PLATFORM_ERR_BUSY
frame CRC mismatch       -> PLATFORM_ERR_CHECKSUM
OTP CRC_flag == 0        -> PLATFORM_ERR_CHECKSUM
CalibrationEnable == 0   -> PLATFORM_ERR_INVALID_STATE
```

失败时不得修改调用者原有 measurement。

measurement 至少包含：

```text
status
rawHumidity
rawTemperature
humidityPercent
temperatureC
```

---

# 9. DHT20 验证要求

不要求额外 Fake I2C Host framework；直接验证完整真实链路。

必须完成：

```text
Keil production build
RTT / EasyLogger target observation
Logic analyzer on PB6/PB7
continuous ~2 s repeated target read
```

RTT 观察：

```text
DHT20 init result
status
raw RH/T
converted RH/T
read error
```

逻辑分析仪至少确认：

```text
START -> 0x70 ACK -> AC ACK -> 33 ACK -> 00 ACK -> STOP
>= 80 ms
START -> 0x71 ACK -> 7-byte read -> final NACK -> STOP
```

临时 smoke harness 验证完成后必须删除，再执行 normal production build。

---

# 10. MPU6050 第一阶段

Phase 7 才正式实现：

```text
WHO_AM_I
initialization
Accel X/Y/Z
Gyro X/Y/Z
raw + basic physical conversion
```

当前不实现：

```text
Roll / Pitch / Yaw
Complementary Filter
Kalman Filter
DMP
高频姿态融合
```

虽然硬件连通性已确认，但不得在 Phase 6 提前实现 MPU6050 production module。

---

# 11. Software I2C 基线

已完成并冻结：

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

# 12. LED 产品语义

```text
STOPPED               -> OFF
RUNNING               -> ON
RUNNING periodic TX   -> keep ON
ONCE TX SUCCESS        -> blink 3 times -> OFF
ONCE sample/TX failure -> keep OFF
```

只有成功完成业务发送后才提交 `ONCE_SUCCESS`。

---

# 13. RTT / EasyLogger

正式链：

```text
APP / Service
 -> service_log
 -> Platform Log
 -> EasyLogger Adapter
 -> EasyLogger / SEGGER RTT
```

```text
INFO  -> initialization / START / STOP / ONCE / state changes
DEBUG -> 2 s acquisition summary / complete UART command / business TX state
WARN  -> recoverable I2C / Sensor / UART / GPIO issue
ERROR -> initialization or critical failure
```

正常运行禁止逐 UART byte、逐 I2C bit / ACK、逐 DMA 步骤、逐 Button polling 刷日志。

---

# 14. 推荐 RTOS 执行模型

已确认方向：

```text
Communication Task
Acquisition Task
Indicator Task
```

后续统一 Acquisition Service / Task 串行调用 DHT20 + MPU6050，不按设备数量机械创建独立 Task。

永久 Button processing context 与最终 IPC 留到 Phase 9。

---

# 15. 分层要求

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

# 16. ISR / 并发 / 内存

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

# 17. 当前范围冻结

必须完成：

```text
GPIO STM32 Impl + board verification
LED
Platform Button / Button Service
Software I2C
DHT20
MPU6050 basic 6-axis
APP Control FSM
UART START / STOP / ONCE / STATUS / HELP
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
复杂二进制协议
Button EXTI
无需求驱动框架扩展
```

---

# 18. 最终验收核心场景

至少验证：

1. 上电后 STOPPED、LED 灭、UART RX / RTT 正常；
2. STOPPED 单击 -> RUNNING、LED 常亮；
3. RUNNING 每 2 s 输出一组 DHT20 + MPU6050 数据；
4. RUNNING 长按 >= 3 s -> STOPPED、LED 灭、停止周期上报；
5. STOPPED 双击 -> 单次采集和发送 -> TX 成功后 LED 闪 3 次 -> STOPPED；
6. UART START / STOP 与按键控制使用同一真实状态；
7. UART ONCE 在 STOPPED 正确执行；
8. STATUS / HELP 返回明确文本；
9. 初始化、关键采集和 UART 收发在 RTT 中可观察；
10. I2C / Sensor / UART / RingBuffer 异常不静默失败。

---

# 19. 当前 Active Phase

```text
Phase 6 DHT20
DESIGN FROZEN / READY FOR CODEX EXECUTION
```

当前执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
```
