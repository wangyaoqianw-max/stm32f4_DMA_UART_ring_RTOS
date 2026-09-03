# 工程长期记忆与交接说明

更新时间：2026-09-03

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
LED Module Phase 4                  COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

真实板测已确认：

```text
PC13 Status LED                       PASS
PA0 User Key                          PASS
PB6 Open-Drain Pull-Low / Release     PASS
PB7 Open-Drain Pull-Low / Release     PASS
PB7 Physical Readback                 PASS
LED OFF / ON / 3 blink / final OFF    PASS
Indicator RTT stage sequence           PASS
Existing UART communication regression PASS
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

Software I2C 为 Task Context / synchronous / caller serialized。

LED 三闪最终在独立 Indicator Task Context 执行，不在 ISR / HAL Callback 执行。

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
逐 LED on/off 边沿打日志
逐 GPIO read/write 打日志
ISR 中大量格式化日志
```

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

Board BSP 已提供：

```text
platform_bsp_gpio_construct_status_led()
platform_bsp_gpio_construct_user_key()
platform_bsp_gpio_construct_soft_i2c_scl()
platform_bsp_gpio_construct_soft_i2c_sda()
```

BSP GPIO constructor 只 construct / bind，不执行 GPIO hardware configure。

---

# 8. Software I2C 已冻结 / 已完成摘要

定位：

```text
Platform MCU basic communication capability
Master only
7-bit address
Synchronous
No internal mutex
No dynamic allocation
```

公共 API：

```text
platform_i2c_init
platform_i2c_write
platform_i2c_read
platform_i2c_write_read
platform_i2c_deinit
```

SCL / SDA：

```text
PB6 / PB7
Open-Drain Output
No Pull
external pull-up
HIGH = release
LOW = actively pull low
physical readback
```

时序：

```text
platform_delay_us()
Cortex-M4 DWT CYCCNT
nominal ~100 kHz
Repeated START supported
last RX byte NACK
```

Phase 3 目标板证据（2026-09-02）：

```text
Keil Full Rebuild (with smoke)         0 errors
Serial Assistant                       I2C_SMOKE,TXRX,PASS,status=0x18
RTT / EasyLogger                       i2c smoke txrx pass, status=0x18
Logic Analyzer                         START -> 0x38(W) -> 0xAC 0x33 0x00 -> STOP
Decoder ACK                            address and all three command bytes ACK
Temporary verification path           removed
Keil Full Rebuild (normal path)        0 errors
```

---

# 9. LED Phase 4 完成记录

专项设计：

```text
00_Doc/02_架构设计/LED_Phase1设计.md
```

最终状态：

```text
Phase 4 — COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

实施与验证证据（2026-09-03）：

```text
Platform LED / BSP LED / Indicator Service       IMPLEMENTED
Final Host regression (5 suites, -Werror)         PASS
Normal-path Keil Full Rebuild                      PASS: 0 errors, 20 pre-existing warnings
Phase 4 source warning scan                        PASS: service_indicator.c / platform_led.c /
                                                   platform_bsp_led.c have no warnings
Target LED visual verification                     PASS — OFF / ON / 3 blinks / final OFF
Target RTT stage observation                       PASS — start / STOPPED / RUNNING /
                                                   STOPPED / ONCE_SUCCESS / pass
Target + PC Serial Assistant communication check   PASS — user confirmed Phase 4 plan completed
Temporary FreeRTOS indicator smoke path            REMOVED
Coding Standard Review                             PASS
```

串口回归 PASS 的证据来源为开发者对“本次 Phase 4 计划全部完成”的明确确认，不额外虚构串口截图或新协议证据。

---

# 10. LED Phase 4 冻结设计

最终职责链：

```text
APP / Control           Phase 10
    ↓ semantic event
Indicator Task          Phase 9
    ↓
Indicator Service       Phase 4
    ↓
Platform LED            Phase 4
    ↓
Platform GPIO           existing
    ↓
STM32 GPIO Impl         existing
```

Phase 4 不实现正式 APP Control FSM，不永久创建 Indicator Task。

## 10.1 Platform LED

定位：

```text
lightweight LED actuator abstraction
caller-owned static object
no malloc/free
```

第一版不使用：

```text
platform_device_t
runtime device type
registry
manager
ops table
new impl_led layer
```

`platform_led_t` 与 GPIO 一对一，直接拥有自己的 `platform_gpio_t` 存储。

公共能力：

```text
platform_led_init
platform_led_on
platform_led_off
platform_led_toggle
platform_led_deinit
```

## 10.2 BSP / 有效电平

Status LED GPIO 继续复用：

```text
platform_bsp_gpio_construct_status_led()
```

具体 PC13 / GPIOC / HAL Pin 不重复进入 `00_Config`。

静态有效电平：

```text
PROJECT_STATUS_LED_ACTIVE_LEVEL = LOW
```

由 BSP LED construction 将 GPIO Binding + active level 组合为 Status LED 对象。

Service / APP 不知道 LOW/HIGH 极性。

## 10.3 Indicator Service

事件：

```text
SERVICE_INDICATOR_EVENT_STOPPED
SERVICE_INDICATOR_EVENT_RUNNING
SERVICE_INDICATOR_EVENT_ONCE_SUCCESS
```

公共能力：

```text
service_indicator_init
service_indicator_handle_event
service_indicator_deinit
```

行为：

```text
STOPPED      -> OFF
RUNNING      -> ON
ONCE_SUCCESS -> blink 3 times -> OFF
```

Indicator Service 不维护 APP `RUNNING / STOPPED` 真实状态，不决定 UART TX 是否成功。

## 10.4 闪烁时序

静态配置：

```text
blink count   = 3
blink ON ms   = 100
blink OFF ms  = 100
```

延时统一使用：

```text
platform_time_delay_ms()
```

最终存在独立 Indicator Task，因此三闪可以使用 Task blocking delay；只阻塞 Indicator Task，不阻塞 UART / Acquisition 等其他 Task。

---

# 11. Indicator Task 冻结方向

最终系统明确采用独立：

```text
Indicator Task
```

职责：

```text
consume indicator events
serialize LED behavior
execute blink timing
isolate LED delay from APP / UART / acquisition contexts
```

以下细节仍留到 Phase 9：

```text
permanent Task creation
priority
stack size
queue / notification mechanism
event buffering / overwrite policy
```

Button 是否独立 Task 尚未冻结，留到 Phase 5 / Phase 9 根据真实职责决定。

---

# 12. Device Model 当前决策

不要求所有硬件相关模块机械使用统一 Device 类型。

当前分类：

```text
GPIO              -> lightweight resource, no platform_device_t
LED               -> lightweight actuator, no platform_device_t
Software I2C      -> communication capability, no platform_device_t
UART              -> real device object, platform_device_t already used
DHT20             -> future design should evaluate platform_device_t
MPU6050           -> future design should evaluate platform_device_t
```

DHT20 / MPU6050 具有明确设备身份、配置、生命周期和数据语义，后续专项设计优先参考 UART 的 Device 模型，而不是照搬 LED 轻量对象。

---

# 13. 当前 Active Phase

当前阶段：

```text
Phase 5 — Button Module (planning)
```

Phase 5 目前只进入专项设计，不直接编码。

已知硬件基线：

```text
PA0 -> User Key
Platform BSP GPIO constructor -> platform_bsp_gpio_construct_user_key()
Target Board GPIO input verification -> PASS
```

已冻结最终按键业务映射：

```text
Button single -> START
Button double -> SAMPLE_ONCE
Button long   -> STOP
```

但 Phase 5 尚需专项设计冻结：

```text
KEY Platform / BSP capability boundary
active-level configuration
polling / sampling interface
debounce algorithm and timing
double-click window
long-press threshold = 3000 ms
Button Service Context / event contract
single vs double confirmation timing
Host Test strategy
FreeRTOS target-board smoke strategy
whether Phase 5 itself needs a temporary task context
```

永久 Button Task / priority / stack / final event transport 仍优先留到 Phase 9，除非 Phase 5 设计证明必须提前冻结。

---

# 14. 当前执行计划状态

当前文件：

```text
00_Doc/04_Agent/implementation_plan.md
```

目前保存：

```text
LED Phase 4 Implementation Plan — COMPLETED
```

不要直接把该计划用于 Button 编码。

Phase 5 推荐流程：

```text
Inspect current KEY / GPIO baseline
    ↓
Discuss Button design point-by-point
    ↓
Freeze Button Phase 1 design document
    ↓
Replace / update implementation_plan.md with Phase 5 plan
    ↓
Codex implementation
```

---

# 15. 当前后续路线

```text
Phase 3  Software I2C                   COMPLETED
Phase 4  LED Module                     COMPLETED
Phase 5  Button Module                  CURRENT / PLANNING
Phase 6  DHT20 Environment Module
Phase 7  MPU6050 Motion Module
Phase 8  UART Application Communication
Phase 9  RTOS Task / Event Design
Phase 10 Final APP Integration
Final Integrated Board Test
```

---

# 16. 当前暂缓范围

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
