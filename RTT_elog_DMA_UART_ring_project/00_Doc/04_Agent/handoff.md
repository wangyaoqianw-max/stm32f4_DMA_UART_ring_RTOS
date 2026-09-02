# 工程长期记忆与交接说明

更新时间：2026-09-02

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

# 3. 已完成基线与当前阶段

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
GPIO STM32 Impl Phase 1             COMPLETED / HOST + KEIL VERIFIED
GPIO STM32 Impl Host Verification   PASS
GPIO STM32 Impl Keil Verification   PASS (0 errors, 15 warnings)
```

当前 Active Phase：

```text
Phase 2 — Board Resource + CubeMX Configuration
```

当前 Phase 2 状态：

```text
Board Resource Freeze               PASS
CubeMX GPIO Configuration           PASS
Board / GPIO Context Binding        COMPLETED / HOST VERIFIED
Target Board GPIO Verification      NOT YET VERIFIED
```

当前唯一执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
```

当前计划标题：

```text
Board GPIO Binding + Target Smoke Test Phase 2 Implementation Plan
```

当前尚未完成：

```text
Board / GPIO Context Binding
Target Board GPIO Smoke Test
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

---

# 8. Phase 2 Board Resource / CubeMX 基线

当前冻结资源：

```text
PC13 -> Status LED
PA0  -> User Key
PB6  -> Software I2C SCL
PB7  -> Software I2C SDA
PA9  -> USART1_TX
PA10 -> USART1_RX
```

当前 CubeMX 标签：

```text
PC13 -> LED_OUT
PA0  -> KEY_IN
PB6  -> I2C_SCL
PB7  -> I2C_SDA
```

当前 CubeMX GPIO 配置：

```text
PC13 : GPIO Output Push-Pull / No Pull / initial HIGH
PA0  : GPIO Input / Pull-Up
PB6  : GPIO Output Open-Drain / No Pull / initial HIGH
PB7  : GPIO Output Open-Drain / No Pull / initial HIGH
```

当前 MCU / CubeMX 侧状态：

```text
GPIOB RCC enabled
Hardware I2C peripheral disabled
PA0 EXTI disabled
USART1 + DMA configuration preserved
```

## 8.1 LED 极性

板级原理图 / 资源表已确认：

```text
PC13 LOW  -> LED ON
PC13 HIGH -> LED OFF
```

系统启动要求：

```text
STOPPED
LED OFF
```

因此 CubeMX 初始电平和后续 Platform LED 初始配置均应使用 HIGH。

## 8.2 KEY 极性与输入策略

按键冻结为：

```text
PA0 LOW  -> pressed
PA0 HIGH -> released
```

当前 Phase 2 保持：

```text
GPIO Input
Pull-Up
No EXTI
```

Button Phase 第一版优先使用约 `10~20 ms` 周期扫描 + 软件消抖 / single / double / long-press 状态机。EXTI 不是当前需求的必需能力，不在 Phase 2 提前引入。

## 8.3 Software I2C GPIO 电气策略

PB6 / PB7 冻结为：

```text
GPIO Output Open-Drain
GPIO_NOPULL
initial HIGH
```

逻辑语义：

```text
write LOW  -> MCU actively pulls line low
write HIGH -> open-drain release, external pull-up raises line high
```

DHT20 和 MPU6050 模块原理图已确认 SDA / SCL 总线上存在外部上拉，因此 MCU 内部 Pull-Up 不启用。

Software I2C Phase 应优先采用：

```text
SDA_LOW()      -> write LOW
SDA_RELEASE()  -> write HIGH / release
SDA_READ()     -> read physical pin level

SCL_LOW()
SCL_RELEASE()
SCL_READ()     -> if later needed by clock-stretch policy
```

第一版不需要为了读取 ACK 而强制在 Input / Output 间来回切换 SDA；STM32 开漏输出状态下仍可读取引脚实际输入电平。最终策略在 Phase 3 专项设计中冻结。

---

# 9. Phase 2 Board / BSP Binding 合同

Phase 2C 采用现有 UART BSP 相同的绑定模式：

```text
Platform BSP public contract
        ↓
Impl BSP concrete board mapping
        ↓
Generic STM32 GPIO Impl
```

计划新增：

```text
03_Platform/platform_bsp/platform_bsp_gpio.h
04_Impl/impl_bsp/impl_platform_bsp_gpio.c
Tests/platform_bsp_gpio/
```

Platform BSP 计划暴露：

```text
platform_bsp_gpio_construct_status_led()
platform_bsp_gpio_construct_user_key()
platform_bsp_gpio_construct_soft_i2c_scl()
platform_bsp_gpio_construct_soft_i2c_sda()
```

具体绑定只存在于 Impl BSP：

```text
status_led     -> PC13
user_key       -> PA0
soft_i2c_scl   -> PB6
soft_i2c_sda   -> PB7
```

Impl BSP 必须使用 CubeMX `main.h` 生成的 `*_GPIO_Port` / `*_Pin` 宏构造 caller-owned static Context，避免在自研代码中维护第二套重复 Pin 常量。

冻结边界：

```text
BSP constructor
    -> construct / bind only

BSP constructor
    != platform_gpio_configure
    != platform_gpio_write
    != HAL GPIO lifecycle
```

设备极性和产品行为不进入 Generic Platform GPIO / STM32 GPIO Impl。

---

# 10. Phase 2 Target Board Smoke Test 基线

Phase 2 完成前必须使用真实开发板验证完整 GPIO vertical slice：

```text
Board BSP
    ↓
Platform GPIO
    ↓
STM32 GPIO Impl
    ↓
HAL
    ↓
PCB / external modules
```

至少验证：

```text
PC13:
    write LOW  -> LED ON
    write HIGH -> LED OFF

PA0:
    released -> HIGH
    pressed  -> LOW

PB6:
    write LOW  -> line LOW
    write HIGH -> release -> external pull-up HIGH

PB7:
    write LOW  -> read LOW
    write HIGH -> release -> read HIGH
```

PB6 / PB7 板测时必须存在有效外部上拉并与 MCU 共地。

本 Smoke Test 明确不实现：

```text
I2C START / STOP
Address
ACK / NACK
DHT20 command
MPU6050 register access
```

这些属于 Phase 3。

只有真实人工观察完成后，才允许把 `Target Board GPIO Smoke Test` 标记为 PASS。

---

# 11. 最终功能基线

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

# 12. 采集 / LED / Software I2C 基线

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

# 13. 当前开发阶段路线

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

# 14. 当前 Active Phase

当前阶段：

```text
Phase 2 — Board Resource + CubeMX Configuration
```

状态：

```text
Resource Table        FROZEN
CubeMX Configuration  COMPLETED / INSPECTED
Board Binding         IMPLEMENTED / HOST + KEIL VERIFIED
Target Board Smoke    PENDING
```

当前唯一执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
```

执行计划只覆盖 Phase 2 剩余范围：

```text
Phase 2C Board / GPIO Context Binding
Phase 2D Target Board GPIO Smoke Test
Phase 2 Closure / Handoff
```

不得重新设计或重复执行：

```text
Platform GPIO Phase 1
GPIO STM32 Impl Phase 1
Board Resource Freeze
CubeMX Pin Configuration
```

除非 Preflight 发现仓库现实与冻结合同发生冲突。

---

# 15. Phase 2 完成门槛

Phase 2 关闭前必须满足：

```text
Platform BSP GPIO contract          PASS
Board / GPIO Context Binding        PASS
GPIO BSP Host Test                  PASS
Platform GPIO Regression            PASS
STM32 GPIO Impl Regression          PASS
Coding Standard Review              PASS
Keil Full Rebuild                   PASS
PC13 LED board smoke                PASS
PA0 KEY board smoke                 PASS
PB6 OD pull-low / release smoke     PASS
PB7 OD pull-low / release / read    PASS
No Hardware I2C introduced          PASS
No EXTI introduced                  PASS
No Software I2C protocol leakage    PASS
```

若 Host / Keil 已通过但真实板测尚未完成：

```text
Phase 2 — IMPLEMENTED / TARGET BOARD VERIFICATION PENDING
```

只有真实板测全部通过后才能记录：

```text
Phase 2 — COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

---

# 16. Phase 2 完成后的下一步

Phase 2 完成后停止实现，进入 Phase 3 设计讨论：

```text
Phase 3 — Software I2C
```

Phase 3 开码前至少冻结：

```text
Software I2C object/interface model
SCL/SDA release/read strategy
microsecond timing source
clock-stretch policy if supported
ACK/NACK behavior
transaction timeout
bus recovery policy
DHT20 / MPU6050 required transaction subset
Host / logic-analyzer verification strategy
```

然后再用新的 Phase 3 内容覆盖：

```text
00_Doc/04_Agent/implementation_plan.md
```

禁止直接从当前 Phase 2 执行计划继续编写 Software I2C。

---

# 17. 当前暂缓范围

当前明确暂缓：

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

除非最终功能需求重新变更，否则不得主动把这些内容加入当前开发主线。

---

# 18. Coding Standard 状态

当前已完成阶段：

```text
Platform GPIO Phase 1 Coding Standard Review  PASS
GPIO STM32 Impl Phase 1 Coding Standard Review PASS
```

当前 Phase 2 新增 / 修改的所有自研 C 代码在提交前必须再次执行：

```text
Coding Standard Review: PASS / NEEDS_FIX / EXCEPTION
```

如为 EXCEPTION，必须记录文件、规则、原因与后续整改状态。

当前 Phase 2 已完成生产代码、Host Test 和 Keil 编译验证；真实目标板尚未连接观察，因此：

```text
Phase 2 Coding Standard Review: PASS
Target Board GPIO Smoke Test: PENDING
Phase 2 status: IMPLEMENTED / TARGET BOARD VERIFICATION PENDING
```

---

# 19. 2026-09-02 Phase 2 执行续接记录

本次在 `main` 分支完成了当前 implementation plan 的 Board GPIO Binding 和目标板 Smoke Test 准备：

```text
Status LED      -> LED_OUT_GPIO_Port / LED_OUT_Pin -> PC13 / active-low
User Key        -> KEY_IN_GPIO_Port / KEY_IN_Pin   -> PA0  / active-low
Soft I2C SCL    -> I2C_SCL_GPIO_Port / I2C_SCL_Pin -> PB6  / open-drain
Soft I2C SDA    -> I2C_SDA_GPIO_Port / I2C_SDA_Pin -> PB7  / open-drain
```

已完成验证：

```text
Platform BSP GPIO Host Test       PASS
Platform GPIO Regression          PASS
STM32 GPIO Impl Regression        PASS
Platform BSP UART Regression      PASS
Keil Full Rebuild                 PASS (0 errors, 20 existing warnings)
```

本次新增的 `impl_platform_bsp_gpio.c` 和临时 `board_gpio_smoke.c` 均未产生 warning。临时 Smoke 分组已经从 Keil 工程移除，`Core/Src/main.c` 未加入临时入口，正常固件启动路径保持不变。

目标板验证方案必须同时使用 USART1 串口助手和 RTT 日志：串口助手观察 `GPIO_SMOKE,...` 阶段标记，RTT 观察对应 `gpio smoke ...` 日志；两条记录与万用表 / 逻辑分析仪结果一致后，才可将 PC13、PA0、PB6、PB7 各项标为 PASS。

当前真实硬件状态：

```text
PC13 Status LED                       PENDING
PA0 User Key                          PENDING
PB6 Open-Drain Pull-Low / Release     PENDING
PB7 Open-Drain Pull-Low / Release     PENDING
PB7 Physical Readback                 PENDING
```

在人工目标板验证完成前，Phase 2 不关闭；下一步只进行真实 GPIO Smoke Test，不开始 Phase 3 Software I2C 实现。
