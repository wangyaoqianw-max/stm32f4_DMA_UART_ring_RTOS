# 工程长期记忆与交接说明

更新时间：2026-09-01

> 本文件是 AI Agent 与人工开发者恢复工程上下文时的长期入口。
> 只保存长期目标、稳定架构合同、已验证能力、当前边界、技术债和下一步候选方向。
> 详细业务需求以 `00_Doc/00_项目需求/最终功能需求.md` 为准；专项设计和执行步骤以对应设计文档和后续确认的 `implementation_plan.md` 为准。

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

> 在已验证 UART 通信链路和分层架构基础上，加入 GPIO、软件 I2C、DHT20、MPU6050、按键控制、LED 状态反馈和 APP 控制状态机，形成一个可通过按键与 PC 串口命令控制的数据采集系统。

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

# 2. 稳定总体依赖

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

固定规则：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

CubeMX 生成文件只作为初始化、IRQ / HAL Callback、Scheduler 和薄胶水入口，长期业务逻辑不得堆积其中。

---

# 3. 当前已完成 / 已验证能力

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

尚未完成：

```text
STM32 GPIO Impl                     NOT STARTED
Target Board GPIO Verification      NOT STARTED
LED / KEY BSP                       NOT STARTED
Button Event Service                NOT STARTED
Software I2C                        NOT STARTED
DHT20                               NOT STARTED
MPU6050                             NOT STARTED
Final Acquisition FSM               NOT STARTED
UART Command Integration            NOT STARTED
Final Integrated Board Test         NOT STARTED
```

---

# 4. 已验证 UART RX 垂直链路

当前真实 RX 链：

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

新增 `START / STOP / ONCE / STATUS / HELP` 命令必须建立在该链路上。

禁止另建一套绕过 UART Service / RingBuffer 的命令 RX。

---

# 5. RingBuffer / ISR / Buffer 冻结合同

RingBuffer：

```text
SPSC byte stream
caller-owned storage
usable capacity = storageSize - 1
no malloc/free
no UART / DMA / HAL knowledge
no silent overwrite
```

当前生产者 / 消费者：

```text
Producer = UART Service RX callback
Consumer = APP Communication Task
```

不得给当前 SPSC 路径加入普通 Mutex。

ISR / HAL Callback 只做：

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

异步 UART TX Buffer 在 TX Complete / Error / Canceled 前不得被修改或失效。

---

# 6. 日志基线

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

新功能日志原则：

```text
INFO  -> 初始化、START / STOP / ONCE、状态切换
DEBUG -> 每 5 s 采集摘要、完整 UART 命令、业务 TX 状态
WARN  -> 可恢复 I2C / Sensor / UART 异常
ERROR -> 初始化失败、关键通信失败
```

禁止正常运行时逐 UART byte、逐 I2C bit/byte/ACK 或在 ISR 中高频打印。

初始化过程需要可观察，但不要求 Platform / Impl 每层都直接调用日志；APP / Service 可根据返回值统一记录关键状态。

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

低 / 高有效极性属于 Board / BSP。

当前缺口是 STM32 GPIO Impl 和目标板验证，而不是重新设计 Platform GPIO。

未经新专项设计不得为了 Soft I2C / LED / KEY 擅自扩大现有 GPIO 公共 API。

---

# 8. 最终功能基线

权威需求：

```text
00_Doc/00_项目需求/最终功能需求.md
```

Agent 需求摘要：

```text
00_Doc/04_Agent/requirements.md
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

## 按键

```text
STOPPED + single click     -> START
STOPPED + double click     -> ONCE
RUNNING + long >= 3 s      -> STOP
```

双击识别要求 single click 延迟到双击判定窗口结束后确认。

## UART 命令

```text
START
STOP
ONCE
STATUS
HELP
```

按键和 UART 最终必须汇聚到同一个 APP Control FSM，不允许两套状态。

---

# 9. 采集与发送基线

第一阶段不做姿态算法。

RUNNING 状态：

```text
每 5 s
  -> DHT20 温湿度
  -> MPU6050 Accel XYZ + Gyro XYZ
  -> 组织文本数据
  -> 通过现有 UART TX 发送到 PC
  -> RTT DEBUG 记录摘要
```

STOPPED 状态不周期采集、不周期上报。

当前 MPU6050 不做：

```text
Roll
Pitch
Yaw
DMP
Kalman
Complementary Filter
高频姿态融合
```

如果未来重新加入姿态算法，再拆分 MPU6050 高频采样与 5 s 上报周期。

---

# 10. LED 产品语义

```text
STOPPED               -> LED OFF
RUNNING               -> LED ON
RUNNING 5 s report     -> LED 保持 ON，不闪
ONCE sample/TX success -> LED 闪 3 次，然后 OFF
ONCE sample/TX failure -> LED 保持 OFF
```

若 UART 异步发送存在明确 TX Complete，应优先用真正 TX Complete 定义 ONCE “发送成功”。

LED 有效电平由 Board / BSP 封装，APP 不知道 GPIO 电平极性。

LED 三闪不得放在 ISR 中阻塞执行。

---

# 11. Software I2C 当前架构约束

当前确定：

```text
DHT20 + MPU6050
       ↓
Software I2C
       ↓
Platform GPIO
       ↓
STM32 GPIO Impl
```

Software I2C 必须：

- 不直接依赖 `HAL_GPIO_xxx`；
- 使用微秒级 delay；
- 不用 RTOS tick delay 直接 bit-bang；
- 支持 START / STOP / ACK / NACK / byte / multi-byte transaction；
- 满足 DHT20 / MPU6050 读写需求。

第一阶段推荐一个采集执行上下文串行访问两个设备，从结构上避免总线并发。

如果未来存在多个并发访问者，Mutex 必须覆盖完整 I2C transaction。

Software I2C 的具体文件位置、对象模型和 delay-us 接口尚未专项冻结，下一阶段不得直接凭 Agent 偏好决定。

---

# 12. 推荐最终职责边界

```text
APP
- 唯一 STOPPED / RUNNING 状态
- START / STOP / ONCE / STATUS 决策
- 5 s 采集业务编排
- LED 产品语义
- UART command 业务语义

Service
- UART Service
- Log Service
- Button debounce / single / double / long
- 可复用传感器数据服务（若专项设计采用）

Platform
- UART
- GPIO
- OS / Time
- Log
- Software I2C / BSP 的具体边界等待专项设计冻结

Impl
- STM32 UART / GPIO
- RTOS / Log adapter
- HAL / CubeMX / IRQ 适配
```

---

# 13. 当前 Config 方向

`00_Config` 已存在：

```text
project_config.h
project_log_config.h
```

后续专项设计至少应把以下静态参数纳入 Config：

```text
Acquisition period = 5000 ms
Button debounce period
Button double-click window
Button long-press threshold = 3000 ms
LED ONCE blink count = 3
LED blink interval
```

具体宏命名等待下一阶段设计，不应现在无计划修改产品代码。

---

# 14. 当前范围冻结

必须最终完成：

```text
GPIO STM32 Impl + board verification
LED / KEY
Button Service
Software I2C
DHT20
MPU6050 basic six-axis data
APP Control FSM
UART command integration
5 s acquisition/report
ONCE LED three-blink success feedback
RTT status/error logs
final integrated board test
```

当前暂停 / 不做：

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

# 15. 当前 implementation_plan 状态

文件：

```text
00_Doc/04_Agent/implementation_plan.md
```

当前仍保留此前 GPIO Platform Phase 1 的计划内容。

该计划对应的 Platform GPIO 实现事实上已经完成并 Host Verified，因此：

> 当前 `implementation_plan.md` 不代表新的最终功能执行顺序，不得被下一位 Agent 直接当作待执行计划重新施工。

用户已明确：

> 先不修改 `implementation_plan.md`，等最终功能拆分任务并确认下一步执行哪个模块后，再生成 / 更新对应计划。

因此下一位 Agent 必须先确认新的“下一阶段任务”，再更新 `implementation_plan.md`。

---

# 16. execution_rules 状态

```text
00_Doc/04_Agent/execution_rules.md
```

仍是有效的通用 Agent Execution Contract，本轮最终功能需求没有改变其中：

- C 代码规范优先级；
- Preflight；
- Coding Standard Review；
- Vendor / CubeMX 修改边界；
- STOP / BLOCKED 规则。

本轮不需要修改该文件。

---

# 17. 下一阶段候选任务

当前还没有选定唯一下一步。

合理候选包括：

```text
A. STM32 GPIO Impl + LED / KEY board binding
B. Software I2C 专项设计
C. DHT20 / MPU6050 驱动专项设计
D. Button Service 专项设计
E. Final APP Control / UART command integration 设计
```

推荐遵循底层可验证能力逐步向上组合，但具体顺序以用户下一次确认的任务为准。

在任务未确认前：

- 不直接开始编码；
- 不重写 `implementation_plan.md`；
- 不提前一次性设计全部子系统；
- 不重新打开 SPI / LCD 范围。

---

# 18. 已知技术债 / 限制

## 18.1 Platform Types 依赖 Impl 类型

当前存在：

```text
platform_types.h
    -> board_types.h
```

需要未来重新确认基础类型归属，但不作为当前最终闭环的阻塞项。

## 18.2 USART1 Callback 单实例

当前 STM32 UART Impl 使用 USART1 单实例 Context。

状态：

```text
KNOWN LIMITATION
DEFERRED
NOT CURRENT DEFECT
```

出现第二个真实 UART 角色后再设计 registry / dispatcher。

## 18.3 README 占位

根目录和部分分层 README 尚未系统整理，项目最终收尾时再统一处理。

## 18.4 GPIO 仅 Host Verified

Platform GPIO 已 Host Verified，但 STM32 Impl 和目标板行为尚未验证。

不得把 GPIO 状态描述为 Target Board Verified。

---

# 19. Agent 恢复上下文时必须读取

在执行下一阶段前至少读取：

```text
00_Doc/00_项目需求/最终功能需求.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/execution_rules.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
```

如果下一阶段涉及已有专项模块，再读取对应专项设计和 Tests。

`implementation_plan.md` 仅在用户确认新的下一阶段并更新后才作为执行依据。

---

# 20. 当前阶段核心结论

```text
UART / DMA / RingBuffer / Log 基线已稳定。
Platform GPIO 公共层已 Host Verified。
最终功能需求已经收束。
SPI / LCD 暂停。
MPU6050 第一阶段不做姿态算法。
采集与 UART 上报周期固定为 5 s。
KEY 与 UART 共用一个 APP Control FSM。
STOPPED LED 灭，RUNNING LED 亮。
ONCE 成功发送后 LED 闪 3 次。
RTT 用于初始化、控制、采集、收发和异常诊断。
下一实施任务尚未确认。
```

下一步先做专项任务选择和设计，再更新 `implementation_plan.md`，不要直接扩大实现范围。