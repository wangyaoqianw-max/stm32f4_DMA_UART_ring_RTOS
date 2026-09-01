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

# 3. 当前已完成 / 当前阶段

已完成 / 已验证基线：

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

当前 Active Phase：

```text
GPIO STM32 Impl Phase 1 Design      FROZEN
GPIO STM32 Impl Phase 1 Plan        READY / NOT STARTED
GPIO STM32 Impl Implementation      NOT STARTED
Target Board GPIO Verification      NOT YET VERIFIED
```

当前专项设计：

```text
00_Doc/02_架构设计/GPIO_STM32_Impl_Phase1设计.md
```

当前唯一执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
```

当前尚未完成：

```text
GPIO STM32 Impl implementation
GPIO STM32 Impl Host Verification
GPIO STM32 Impl Keil Verification
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
逐 GPIO read/write 打日志
ISR 中大量格式化日志
每层重复打印同一成功状态
```

初始化过程和采集 / 收发过程需要可观察，但不要求 Platform / Impl 每一层都主动打印；APP / Service 根据返回结果记录关键状态即可。

---

# 7. Platform GPIO 与 STM32 GPIO Impl 合同

Platform GPIO 专项设计：

```text
00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md
```

GPIO STM32 Impl 专项设计：

```text
00_Doc/02_架构设计/GPIO_STM32_Impl_Phase1设计.md
```

Platform 公共 API 已完成并 Host Verified：

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
GPIO_TypeDef
GPIO_PIN_x
LED
KEY
SCL
SDA
DHT20
MPU6050
```

当前 GPIO STM32 Impl 冻结为：

```text
Generic MCU implementation
Caller-owned Context
Context = GPIO_TypeDef *port + uint16_t pin
One Platform GPIO = one physical pin
No Context Pool
No Registry
No dynamic allocation
No reverse Platform pointer
```

Context 只描述 STM32 Port / Pin；具体设备语义和有效电平不进入 Generic Impl。

## 7.1 Config -> HAL

冻结映射：

```text
INPUT                  -> GPIO_MODE_INPUT
OUTPUT + PUSH_PULL     -> GPIO_MODE_OUTPUT_PP
OUTPUT + OPEN_DRAIN    -> GPIO_MODE_OUTPUT_OD
PULL_NONE              -> GPIO_NOPULL
PULL_UP                -> GPIO_PULLUP
PULL_DOWN              -> GPIO_PULLDOWN
GPIO Speed             -> GPIO_SPEED_FREQ_LOW (Impl private policy)
```

Platform 不新增 GPIO Speed API。

## 7.2 Initial Level

OUTPUT 配置顺序冻结：

```text
HAL_GPIO_WritePin(initialLevel)
    -> HAL_GPIO_Init(output mode)
```

用于减少切换输出模式时的错误瞬态。

INPUT 配置不执行 initial-level write。

## 7.3 RCC / CubeMX

GPIO STM32 Impl 不负责 GPIO Port RCC Enable / Disable。

冻结所有权：

```text
CubeMX / Board Bootstrap
    -> GPIO Port RCC

STM32 GPIO Impl
    -> Pin mode / pull / output type / level

Board / BSP
    -> concrete Port + Pin / device polarity
```

调用 `platform_gpio_configure()` 前，对应 GPIO Port Clock 必须已经启用。

## 7.4 Phase 1 Board 边界

当前 Phase 1 不绑定：

```text
PC13 LED
PA0 KEY
Software I2C SCL / SDA
```

上述具体资源进入 Phase 2。

未经新专项设计，不得为了 LED / KEY / Software I2C 修改现有 Platform GPIO 公共 API。

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

# 11. 当前 Active Phase

当前阶段：

```text
Phase 1 — GPIO STM32 Impl
```

状态：

```text
Design          FROZEN
Plan            READY / NOT STARTED
Implementation  NOT STARTED
```

专项设计：

```text
00_Doc/02_架构设计/GPIO_STM32_Impl_Phase1设计.md
```

实现目标：

```text
04_Impl/impl_mcu/impl_platform_gpio.h
04_Impl/impl_mcu/impl_platform_gpio.c
Tests/impl_platform_gpio/
```

本 Phase 重点验证：

```text
Generic caller-owned Context
single physical pin validation
Platform Config -> HAL mapping
GPIO_SPEED_FREQ_LOW private policy
initial-level-before-init ordering
read / write / deinit mapping
no RCC ownership in Impl
Host Fake-HAL tests
Platform GPIO regression
Dependency boundary
Coding Standard Review
Keil Build
```

Phase 1 中不得提前混入：

```text
LED / KEY board binding
PC13 / PA0 concrete mapping
CubeMX final Pin config
Target Board GPIO Smoke Test
Debounce
Software I2C
DHT20
MPU6050
Final APP FSM
```

如果实现必须突破上述边界，STOP / BLOCKED 并返回设计阶段。

---

# 12. implementation_plan 当前状态

文件：

```text
00_Doc/04_Agent/implementation_plan.md
```

当前内容已经更新为：

```text
GPIO STM32 Impl Phase 1 Implementation Plan
Status: READY / NOT STARTED
```

当前计划是唯一可执行施工计划。

执行时必须按 Task 顺序和 TDD Gate 施工，不得继续执行已完成的 GPIO Platform 历史计划。

Phase 1 完成门槛：

```text
GPIO STM32 Impl Host Tests       PASS
Platform GPIO Regression         PASS
Dependency Boundary              PASS
RCC Ownership Scan               PASS
Coding Standard Review           PASS
Keil Build                       PASS
```

目标板 GPIO 行为不是 Phase 1 完成门槛，而是 Phase 2 的板级 Gate。

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

GPIO STM32 Impl Phase 1 不新增产品 Config；GPIO Speed 当前为 Impl private policy。

具体产品宏命名由对应专项设计冻结，不提前修改。

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

## 15.3 GPIO 验证边界

当前：

```text
Platform GPIO                  HOST VERIFIED
GPIO STM32 Impl Design         FROZEN
GPIO STM32 Impl Implementation NOT STARTED
Target Board GPIO              NOT YET VERIFIED
```

不得在 Phase 1 实施前或仅凭 Host Fake-HAL 测试描述为 Target Board Verified。

## 15.4 README

根目录和部分分层 README 尚未系统整理，项目最终收尾时统一处理。

---

# 16. Agent 恢复上下文时必须读取

开始当前 Phase 前至少读取：

```text
00_Doc/00_项目需求/最终功能需求.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/implementation_plan.md
00_Doc/02_架构设计/GPIO_STM32_Impl_Phase1设计.md
00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
```

并检查现有：

```text
03_Platform/platform_mcu/gpio/
04_Impl/impl_mcu/impl_platform_uart.*
Tests/platform_gpio/
Tests/impl_platform_uart/
Core/Src/gpio.c
STM32 HAL GPIO headers
```

---

# 17. 当前核心结论

```text
UART / DMA / RingBuffer / Log 基线已稳定。
Platform GPIO 公共层已 Host Verified。
GPIO STM32 Impl Phase 1 设计已冻结。
GPIO STM32 Impl Phase 1 施工计划已生成并可执行。
GPIO Impl 使用 Generic + caller-owned Context 方案。
Context = GPIO_TypeDef *port + one physical pin。
不使用 Context Pool / Registry / dynamic allocation。
GPIO Port RCC 由 CubeMX / Board Bootstrap 负责。
GPIO Speed Phase 1 固定为 Impl-private LOW。
OUTPUT 必须先准备 initialLevel 再 HAL_GPIO_Init。
PC13 LED / PA0 KEY 等具体 Board Binding 进入 Phase 2。
Phase 1 做 Host Fake-HAL + Platform regression + Keil Build，不做目标板 GPIO Smoke Test。
最终功能仍按 development_roadmap 的 Phase 1 -> Phase 10 顺序推进。
SPI / LCD 暂停。
```

下一步：由 Codex 严格执行当前 `00_Doc/04_Agent/implementation_plan.md`，完成 GPIO STM32 Impl Phase 1；阶段收口后再单独设计 Phase 2 — Board Resource + CubeMX Configuration。