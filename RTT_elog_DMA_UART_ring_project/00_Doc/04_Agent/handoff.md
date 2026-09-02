# 工程长期记忆与交接说明

更新时间：2026-09-02

> 本文件是 AI Agent 与人工开发者恢复工程上下文时的长期入口。
> 只保存长期目标、稳定架构合同、已验证能力、当前阶段、技术债和下一步。
> 详细业务需求以 `00_Doc/00_项目需求/最终功能需求.md` 为准。
> 整体阶段拆分以 `00_Doc/04_Agent/development_roadmap.md` 为准。
> 当前具体施工步骤只以 `00_Doc/04_Agent/implementation_plan.md` 为准。

---

# 1. 项目定位

工程根目录：

```text
RTT_elog_DMA_UART_ring_project/
```

目标环境：

```text
MCU        : STM32F411CEU6 / Cortex-M4F
Flash      : 512 KB
RAM        : 128 KB
UART       : USART1 / 115200 8N1
RTOS       : CMSIS-RTOS2 + FreeRTOS
Debug Log  : EasyLogger + SEGGER RTT
Sensors    : DHT20 + MPU6050
I2C        : Software I2C over GPIO
Input      : 1 x KEY
Indicator  : 1 x LED
```

项目最初目标：

```text
UART 不定长接收 + DMA + RingBuffer + FreeRTOS
```

当前最终目标：

> 在已验证 UART 通信链路和五层架构基础上，加入 GPIO、Software I2C、DHT20、MPU6050、按键控制、LED 状态反馈、UART 文本命令和 APP Control FSM，形成完整数据采集系统。

---

# 2. 稳定总体架构

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

CubeMX 生成文件只作为初始化、IRQ / HAL Callback、Scheduler 和薄适配入口，长期业务逻辑不得堆积其中。

---

# 3. 已验证基线

已完成 / 已验证：

```text
Platform BSP UART Binding Phase 1   COMPLETED
Communication UART Binding          VERIFIED
UART Service Phase 1                COMPLETED
Base RX Vertical Slice              VERIFIED
APP Phase 1                         COMPLETED / VERIFIED
Platform OS                         COMPLETED / VERIFIED
Service Log Phase 1                 COMPLETED / HOST + RTT VERIFIED
Platform Log Naming Refactor        COMPLETED / KEIL + RTT VERIFIED
RingBuffer SPSC Review              REVIEWED
Platform GPIO Phase 1               COMPLETED / HOST VERIFIED
GPIO Header Isolation               PASS
GPIO Platform Dependency Boundary   PASS
GPIO Coding Standard Review         PASS
GPIO STM32 Impl Phase 1             COMPLETED / HOST + KEIL VERIFIED
Board Resource Freeze               PASS
CubeMX GPIO Configuration           PASS
Board / GPIO Context Binding        COMPLETED / HOST VERIFIED
Target Board GPIO Verification      PASS
Software I2C Phase 3                COMPLETED / HOST + KEIL + DHT20 TARGET SMOKE VERIFIED
```

Phase 2 最终状态：

```text
Phase 2 — COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

真实板测已确认：

```text
PC13 Status LED                       PASS
PA0 User Key                          PASS
PB6 Open-Drain Pull-Low / Release     PASS
PB7 Open-Drain Pull-Low / Release     PASS
PB7 Physical Readback                 PASS
```

---

# 4. UART / RingBuffer 稳定合同

当前 RX 链：

```text
USART1
  ↓
DMA Circular + IDLE / HT / TC
  ↓
STM32 UART Impl
  ↓
Platform UART RX_DATA / ERROR / CANCELED
  ↓
UART Service
  ↓
SPSC RingBuffer
  ↓
Platform Notify From ISR
  ↓
APP Communication Task
```

RingBuffer 冻结：

```text
SPSC byte stream
caller-owned storage
usable capacity = storageSize - 1
no malloc/free
no UART / DMA / HAL knowledge
no silent overwrite
```

当前：

```text
Producer = UART Service RX callback
Consumer = APP Communication Task
```

不得给当前 SPSC 链路加入普通 Mutex。

新增 `START / STOP / ONCE / STATUS / HELP` 必须复用现有 UART Service / RingBuffer RX 链。

---

# 5. ISR / Task 合同

ISR / HAL Callback 只允许：

```text
capture
copy necessary bytes
update lightweight state
ISR-safe notify
quick exit
```

禁止：

```text
blocking
ordinary mutex
malloc/free
完整协议解析
传感器业务
Software I2C transaction
LED 闪烁延时
大量格式化日志
非 ISR-safe RTOS API
```

Software I2C Phase 1 明确为 Task Context / synchronous / caller serialized。

---

# 6. RTT / EasyLogger 基线

正式链路：

```text
APP / Service
    ↓
service_log
    ↓
Platform Log
    ↓
EasyLogger Adapter
    ↓
EasyLogger / RTT
```

日志原则：

```text
INFO  -> 初始化、START / STOP / ONCE、关键状态切换
DEBUG -> 5 s 采集摘要、完整 UART 命令、业务 TX 状态
WARN  -> 可恢复 GPIO / I2C / Sensor / UART 异常
ERROR -> 初始化失败、关键操作失败
```

禁止正常运行时：

```text
逐 UART byte 打日志
逐 I2C bit / byte / ACK 打日志
逐 GPIO read/write 打日志
ISR 中大量格式化日志
```

Phase 3 目标板 Smoke Test 允许临时低频阶段日志。

---

# 7. Platform GPIO / Board 基线

Platform GPIO 公共 API：

```text
platform_gpio_init
platform_gpio_configure
platform_gpio_write
platform_gpio_read
platform_gpio_deinit
```

Generic STM32 GPIO Impl：

```text
Caller-owned Context
Context = GPIO_TypeDef *port + uint16_t pin
One Platform GPIO = one physical pin
No Context Pool
No Registry
No dynamic allocation
```

当前板级资源：

```text
PC13 -> Status LED
PA0  -> User Key
PB6  -> Software I2C SCL
PB7  -> Software I2C SDA
PA9  -> USART1_TX
PA10 -> USART1_RX
```

Software I2C GPIO：

```text
PB6 / PB7
GPIO Output Open-Drain
GPIO_NOPULL
initial HIGH
external pull-up present
Hardware I2C disabled
```

Board BSP 已提供：

```text
platform_bsp_gpio_construct_status_led()
platform_bsp_gpio_construct_user_key()
platform_bsp_gpio_construct_soft_i2c_scl()
platform_bsp_gpio_construct_soft_i2c_sda()
```

BSP constructor 只 construct / bind，不执行 `platform_gpio_configure()`。

---

# 8. 当前 Active Phase

当前阶段：

```text
Phase 4 — LED Module (planning)
```

状态：

```text
Phase 3 completed
Host + Keil + DHT20 target smoke verified
Temporary smoke harness removed
Normal-path Keil rebuild: 0 errors
```

专项设计：

```text
00_Doc/02_架构设计/Software_I2C_Phase1设计.md
```

当前唯一执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
```

当前计划标题：

```text
Software I2C Phase 3 Completion Record
```

Phase 3 目标板证据（2026-09-02）：

```text
Keil Full Rebuild (with smoke)        0 errors
Serial Assistant                      I2C_SMOKE,TXRX,PASS,status=0x18
RTT / EasyLogger                      i2c smoke txrx pass, status=0x18
Logic Analyzer                         START -> 0x38(W) -> 0xAC 0x33 0x00 -> STOP
Decoder ACK                            address and all three command bytes ACK
Temporary verification path            removed from source and Keil project
Keil Full Rebuild (normal path)        0 errors
```

烟测使用 DHT20 的安全原始事务：写触发命令 `0xAC, 0x33, 0x00`，通过 `platform_time_delay_ms(80)` 等待后读取 7 字节。`status=0x18` 表示设备未忙且校准位有效。当前尚未采集 `write_read()` 的目标板 Repeated START / final-NACK 专项波形，Host Test 已覆盖该协议分支；清理 smoke 后的最终 Keil Full Rebuild 已通过，0 errors。

---

# 9. Software I2C Phase 1 冻结设计

定位：

```text
Platform MCU basic communication capability
Master only
7-bit address
Synchronous
No internal mutex
No dynamic allocation
```

公共命名：

```text
platform_i2c_*
```

当前内部实现：Software I2C。

计划文件：

```text
03_Platform/platform_mcu/i2c/platform_i2c.h
03_Platform/platform_mcu/i2c/platform_i2c.c
```

公共 transaction API：

```text
platform_i2c_init
platform_i2c_write
platform_i2c_read
platform_i2c_write_read
platform_i2c_deinit
```

不暴露：

```text
START
STOP
ACK / NACK
write_byte
read_byte
```

这些属于 `platform_i2c.c` private protocol primitives。

不增加 `mem_read / mem_write`，寄存器语义留给 MPU6050 等具体设备模块。

---

# 10. Software I2C GPIO / 时序合同

SCL / SDA transaction 中始终保持：

```text
Open-Drain Output
```

语义：

```text
LOW     -> actively pull low
HIGH    -> release
READ    -> physical line readback
```

不动态切换 SDA Input / Output。

内部语义优先使用：

```text
SDA_LOW
SDA_RELEASE
SDA_READ
SCL_LOW
SCL_RELEASE
SCL_READ
```

时序：

```text
Standard-mode oriented
nominal ~100 kHz
exact 100 kHz not required
MSB first
Repeated START supported
last read byte -> NACK
```

每次需要 SCL HIGH 时：

```text
release SCL
read actual SCL
wait in us steps if LOW
timeout -> PLATFORM_ERR_TIMEOUT
```

第一版因此具备基础 clock-stretch / stuck-low 检测能力，但不构建复杂 Clock Stretching Framework。

---

# 11. 微秒延时冻结方案

现有：

```text
03_Platform/platform_common/platform_def.h
```

已经预留：

```c
void platform_delay_ms(uint32_t ms);
void platform_delay_us(uint32_t us);
```

Phase 3 只实现：

```text
platform_delay_us()
```

计划实现位置：

```text
04_Impl/impl_mcu/impl_platform_delay.c
```

实现：

```text
Cortex-M4 DWT CYCCNT
busy-wait
lazy initialization
SystemCoreClock based conversion
```

不新增：

```text
platform_time_delay_us()
new Platform Delay module
public delay init API
```

现有：

```text
platform_time_delay_ms()
```

继续作为 FreeRTOS / CMSIS-RTOS2 Task Context 毫秒调度延时。

两个延时底层语义不同：

```text
ms -> scheduler delay
us -> short busy-wait timing
```

旧预留 `platform_delay_ms()` 不属于 Phase 3 必须实现范围。

---

# 12. Software I2C 静态配置

静态配置统一放：

```text
00_Config/project_config.h
```

冻结计划：

```c
#define PROJECT_SOFT_I2C_HALF_PERIOD_US    (5U)
#define PROJECT_SOFT_I2C_SCL_TIMEOUT_US    (100U)
```

不把 timing 放进 `platform_i2c_t` Context。

协议固定行为不进入 Config：

```text
7-bit address
MSB first
9 recovery clocks maximum
last read byte NACK
```

---

# 13. Transaction / 错误合同

Write：

```text
START
Address + W
ACK
TX bytes + ACK
STOP
```

Read：

```text
START
Address + R
ACK
RX bytes
intermediate ACK
last byte NACK
STOP
```

WriteRead：

```text
START
Address + W
TX
Repeated START
Address + R
RX
STOP
```

地址由上层传 7-bit address，I2C 层内部生成 R/W bit。

错误复用 `platform_error_t`：

```text
invalid param       -> PLATFORM_ERR_INVALID_PARAM
not initialized     -> PLATFORM_ERR_NOT_INITIALIZED
address NACK        -> PLATFORM_ERR_NOT_FOUND
data/protocol error -> PLATFORM_ERR_IO
SCL timeout         -> PLATFORM_ERR_TIMEOUT
bus non-idle        -> PLATFORM_ERR_BUSY
```

transaction 已开始后失败：

```text
preserve original error
best-effort STOP / release
return original error
```

---

# 14. Init / Recovery / Deinit

Init：

```text
validate
store SCL/SDA
configure both OD Output / No Pull / initial HIGH
release both
check physical Idle
```

Idle：

```text
SCL HIGH
SDA HIGH
```

初始化时：

```text
SCL HIGH + SDA LOW
    -> allow one bus recovery
```

Recovery：

```text
release SDA
up to 9 SCL pulses
stop early if SDA releases
finally generate STOP
verify Idle
```

正常 transaction 如果总线异常，不自动反复 recovery；返回错误给调用者。

Deinit：

```text
release SCL/SDA
platform_gpio_deinit()
clear initialized
```

I2C 不销毁 caller-owned GPIO 对象存储。

---

# 15. Phase 3 验证方案

Phase 3 不允许只凭 Host Test / Keil Build 关闭。

验证分层：

```text
Host Test
    -> protocol logic / GPIO interaction sequence

Keil Full Rebuild
    -> STM32 / DWT / integration

Target Board
    -> Serial Assistant
    -> RTT / EasyLogger
    -> Logic Analyzer
```

目标板三路观察冻结为：

## 15.1 串口助手

观察低频测试阶段 / 最终结果，例如：

```text
I2C_SMOKE,START
I2C_SMOKE,INIT,PASS
I2C_SMOKE,TXRX,PASS
I2C_SMOKE,PASS
```

失败：

```text
I2C_SMOKE,FAIL,<stage>,<platform_error>
```

## 15.2 RTT / EasyLogger

观察：

```text
I2C init result
bus recovery occurrence / failure
address NACK
timeout
I/O failure
smoke final result
```

不打印逐 bit / byte / ACK 正常日志。

## 15.3 逻辑分析仪

连接：

```text
PB6 -> SCL
PB7 -> SDA
GND -> common ground
```

必须观察：

```text
Idle HIGH
START
STOP
Repeated START
7-bit Address
R/W
ACK / NACK
TX / RX Data
MSB first
last-read-byte NACK
SCL high / low time
actual clock in acceptable Standard-mode range
```

优先使用逻辑分析仪 I2C decoder。

串口助手、RTT 与逻辑分析仪结果必须一致。

目标板允许使用已连接 DHT20 / MPU6050 做原始 I2C transaction smoke verification，但 Phase 3 不建立正式 Sensor Driver，不猜测未经数据手册确认的破坏性命令。

---

# 16. Phase 3 完成门槛

必须全部满足：

```text
Microsecond DWT implementation             PASS
Platform I2C Host Test                     PASS
Platform GPIO regressions                  PASS
Coding Standard Review                     PASS
Keil Full Rebuild                          PASS
Serial Assistant target observation        PASS
RTT target observation                     PASS
Logic Analyzer START / STOP                PASS
Logic Analyzer Address / ACK               PASS
Logic Analyzer Repeated START              PASS
Logic Analyzer Read / Write transaction    PASS
Normal firmware path restored              PASS
No Hardware I2C introduced                 PASS
No HAL GPIO leakage into Platform I2C      PASS
No Sensor business logic introduced        PASS
No unnecessary mutex / async framework     PASS
```

若 Host / Keil 已通过但真实板测未完成：

```text
Phase 3 — IMPLEMENTED / TARGET BOARD VERIFICATION PENDING
```

只有三路目标板观察全部通过后：

```text
Phase 3 — COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

---

# 17. 当前后续路线

冻结顺序：

```text
Phase 3  Software I2C          <- COMPLETED / TARGET SMOKE VERIFIED
Phase 4  LED Module             <- NEXT (planning)
Phase 5  Button Module
Phase 6  DHT20 Environment Module
Phase 7  MPU6050 Motion Module
Phase 8  UART Application Communication
Phase 9  RTOS Task / Event Design
Phase 10 Final APP Integration
Final Integrated Board Test
```

在清理 smoke 后完成一次 Keil Full Rebuild；随后停止继续扩展 I2C，进入 Phase 4 的 LED 专项设计与计划评审。

---

# 18. 当前暂缓范围

```text
SPI / LCD / GUI
W25Q64
AT24C02
Bluetooth
Roll / Pitch / Yaw
DMP
Kalman / Complementary Filter
复杂二进制 UART Protocol
无需求驱动的框架扩展
```

不得主动加入当前开发主线。
