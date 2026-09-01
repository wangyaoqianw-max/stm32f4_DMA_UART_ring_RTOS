# 工程长期记忆与交接说明

更新时间：2026-09-01

> 本文件是 AI Agent 与人工开发者恢复工程上下文时的长期入口。
> 只保存长期目标、稳定架构合同、已验证能力、当前阶段、技术债和下一步。
> 详细业务需求以 `00_Doc/00_项目需求/最终功能需求.md` 为准。
> 整体阶段拆分以 `00_Doc/04_Agent/development_roadmap.md` 为准。
> 当前具体施工步骤只以用户确认后更新的 `00_Doc/04_Agent/implementation_plan.md` 为准。

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

项目同时用于验证：

```text
Requirements
 -> Design
 -> Implementation Plan
 -> AI Implementation
 -> Test / Review
 -> Handoff
```

的工程化 AI 辅助开发流程。

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

# 3. 当前已完成 / 已验证基线

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
```

当前尚未完成：

```text
STM32 GPIO Impl
Target Board GPIO Verification
Board Resource / CubeMX final configuration
Software I2C
LED module
Button module
DHT20 module
MPU6050 Motion module
UART Application Command / Report
Final RTOS Task / Event Design
Final APP Control FSM
Final Integrated Board Test
```

---

# 4. 已验证 UART / RingBuffer 合同

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
  ↓
Application-level byte-stream handling
```

新增 `START / STOP / ONCE / STATUS / HELP` 必须复用这条链路，不允许绕过 UART Service / RingBuffer 另建命令 RX。

RingBuffer 冻结为：

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

不得给当前 SPSC 路径加入普通 Mutex。

异步 UART TX Buffer 在 TX Complete / Error / Canceled 前不得被修改或失效。

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
LED 闪烁延时
大量格式化日志
非 ISR-safe RTOS API
```

后续 Button、LED、Sensor、UART Command 等业务都应在 Task / Service / APP 上下文处理。

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

最终功能日志原则：

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
ISR 中大量格式化日志
每层重复打印同一成功状态
```

初始化过程和采集 / 收发过程需要可观察，但不要求 Platform / Impl 每一层都主动打印；可以由 APP / Service 根据返回结果记录关键状态。

---

# 7. Platform GPIO 当前合同

专项设计：

```text
00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md
```

当前公共 API 已完成并 Host Verified：

```text
platform_gpio_init
platform_gpio_configure
platform_gpio_write
platform_gpio_read
platform_gpio_deinit
```

`platform_gpio_t` 是轻量 MCU Resource，不继承 `platform_device_t`。

Platform GPIO 不知道：

```text
LED
KEY
SCL
SDA
DHT20
MPU6050
```

低 / 高有效极性属于 Board / BSP / Impl 边界。

当前缺口是 STM32 GPIO Impl 和目标板验证，而不是重新设计 Platform GPIO。

未经新专项设计，不得为了 LED / KEY / Software I2C 擅自扩大现有 GPIO 公共 API。

---

# 8. 最终功能基线

权威需求：

```text
00_Doc/00_项目需求/最终功能需求.md
```

系统状态：

```text
STOPPED
RUNNING
```

启动完成：

```text
STOPPED
LED OFF
UART RX ACTIVE
RTT ACTIVE
```

按键：

```text
STOPPED + SINGLE       -> START
STOPPED + DOUBLE       -> ONCE
RUNNING + LONG >= 3 s  -> STOP
```

UART 命令：

```text
START
STOP
ONCE
STATUS
HELP
```

按键和 UART 必须统一转换为 APP 控制事件，最终只维护一个 APP Control FSM。

---

# 9. 采集 / LED / Software I2C 基线

RUNNING：

```text
每 5 s
  -> DHT20 temperature / humidity
  -> MPU6050 Accel XYZ / Gyro XYZ
  -> text report
  -> existing UART TX
  -> PC serial assistant
```

第一阶段 MPU6050 不做：

```text
Roll
Pitch
Yaw
DMP
Kalman
Complementary Filter
高频姿态融合
```

LED 产品语义：

```text
STOPPED               -> OFF
RUNNING               -> ON
RUNNING 5 s report     -> stays ON
ONCE TX success        -> blink 3 times, then OFF
ONCE failure           -> stays OFF
```

Software I2C 架构：

```text
DHT20 + MPU6050
       ↓
Software I2C
       ↓
Platform GPIO
       ↓
STM32 GPIO Impl
```

Software I2C 不直接依赖 HAL GPIO；使用微秒级时序；第一阶段优先串行访问两个传感器，避免无必要 I2C 并发。

---

# 10. 当前开发阶段路线

整体路线：

```text
00_Doc/04_Agent/development_roadmap.md
```

当前冻结的阶段顺序：

```text
Phase 1  GPIO STM32 Impl
Phase 2  Board Resource + CubeMX Configuration
Phase 3  Software I2C
Phase 4  LED Module
Phase 5  Button Module
Phase 6  DHT20 Environment Module
Phase 7  MPU6050 Motion Module
Phase 8  UART Application Communication
Phase 9  RTOS Task / Event Design
Phase 10 Final APP Integration
```

RTT / EasyLogger 与 Config 属于横切要求，随各 Phase 一起完成，不单独拆成 Phase。

阶段原则：

```text
Roadmap
    = 整个最终功能的阶段顺序和边界

implementation_plan.md
    = 当前唯一已确认 Phase 的具体施工计划
```

禁止一次生成 Phase 1 ~ Phase 10 的超长施工计划并连续实现。

---

# 11. 当前下一阶段

当前已确定下一步先讨论：

```text
Phase 1 — GPIO STM32 Impl
```

当前尚未冻结 GPIO Impl 的专项设计，也尚未生成新的 GPIO Impl 执行计划。

下一步讨论应重点确认：

```text
STM32 GPIO Impl object/context binding
Platform config -> HAL GPIO mapping
GPIO port / pin representation
output initial-level ordering
read / write behavior
deinit semantics
Platform Error mapping
CubeMX / Board boundary
Keil verification
Phase 2 board smoke-test handoff boundary
```

Phase 1 中不得提前混入：

```text
LED semantics
KEY semantics
Debounce
Software I2C
DHT20
MPU6050
Final APP FSM
```

---

# 12. implementation_plan 当前状态

文件：

```text
00_Doc/04_Agent/implementation_plan.md
```

当前内容仍是已经完成的 GPIO Platform Phase 1 历史计划。

因此：

> 当前 `implementation_plan.md` 不可直接执行。

用户已明确先保留该文件，等 GPIO STM32 Impl 的设计和任务边界讨论完成后，再更新为 Phase 1 的具体执行计划。

---

# 13. Config 方向

`00_Config` 已存在：

```text
project_config.h
project_log_config.h
```

最终至少应逐阶段纳入：

```text
Acquisition period = 5000 ms
Button debounce time
Button double-click window
Button long-press threshold = 3000 ms
LED ONCE blink count = 3
LED blink interval
UART command / report limits if needed
```

具体宏命名由对应专项设计冻结，不提前修改。

---

# 14. 当前暂停范围

```text
SPI / LCD / GUI
Roll / Pitch / Yaw
DMP / Kalman / complementary filter
W25Q64 / AT24C02
Bluetooth
复杂 UART binary protocol
与最终验收无关的框架扩张
```

---

# 15. 已知技术债 / 限制

## 15.1 Platform Types 依赖 Impl 类型

当前存在：

```text
platform_types.h
    -> board_types.h
```

未来重新确认基础类型归属，不作为当前最终闭环阻塞项。

## 15.2 USART1 Callback 单实例

当前 STM32 UART Impl 使用 USART1 单实例 Context。

状态：

```text
KNOWN LIMITATION
DEFERRED
NOT CURRENT DEFECT
```

出现第二个真实 UART 角色后再设计 registry / dispatcher。

## 15.3 GPIO 仅 Host Verified

Platform GPIO 已 Host Verified，但 STM32 Impl 和目标板行为尚未验证。

不得描述为 Target Board Verified。

## 15.4 README

根目录和部分分层 README 尚未系统整理，项目最终收尾时统一处理。

---

# 16. Agent 恢复上下文时必须读取

开始新 Phase 前至少读取：

```text
00_Doc/00_项目需求/最终功能需求.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/execution_rules.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
```

涉及已有专项模块时，再读取对应专项设计、代码和 Tests。

`implementation_plan.md` 只有在当前 Phase 重新确认并更新后，才作为实施依据。

---

# 17. 当前核心结论

```text
UART / DMA / RingBuffer / Log 基线已稳定。
Platform GPIO 公共层已 Host Verified。
最终功能需求已经收束。
最终功能已拆分为 10 个 Phase。
下一阶段确定为 GPIO STM32 Impl 设计讨论。
当前旧 implementation_plan 不执行。
SPI / LCD 暂停。
MPU6050 第一阶段只做六轴基础数据。
采集与 UART 上报周期为 5 s。
KEY 与 UART 共用一个 APP Control FSM。
STOPPED LED 灭，RUNNING LED 亮。
ONCE 成功发送后 LED 闪 3 次。
RTT 用于初始化、控制、采集、收发和异常诊断。
```

下一步：讨论 GPIO STM32 Impl 的专项设计和验收边界，确认后再更新 `implementation_plan.md`。