# Final Acquisition System Development Roadmap

> 文档类型：Development Phase Roadmap  
> 状态：Baseline  
> 日期：2026-09-03  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 文档目的

本文档用于把最终功能需求拆分为可独立设计、实现、验证和交接的开发阶段。

本文档只回答：

```text
后续分成哪些 Phase？
各 Phase 依赖什么？
各 Phase 做到哪里停止？
什么条件下可以进入下一 Phase？
```

本文档不是具体执行计划，不冻结具体函数签名、文件名和实现步骤。

当前执行计划仍使用：

```text
00_Doc/04_Agent/implementation_plan.md
```

但该文件一次只代表“当前已确认要执行的 Phase”。切换 Phase 前，应先完成专项设计，再重写 / 更新 `implementation_plan.md`。

最终功能需求以：

```text
00_Doc/00_项目需求/最终功能需求.md
```

为准。

架构边界以：

```text
00_Doc/04_Agent/architecture.md
```

为准。

---

# 2. 当前基线

已经完成并作为后续开发基础复用：

```text
UART Platform / STM32 Impl
UART DMA RX / TX
UART Service
SPSC RingBuffer
APP Communication Phase 1
Platform OS
Service Log + EasyLogger + RTT
Platform GPIO Phase 1 / Host Verified
GPIO STM32 Impl / Board Resource / CubeMX Configuration
Software I2C / Host + Keil + DHT20 Target Smoke Verified
```

当前最终闭环尚缺：

```text
LED
Button
DHT20
MPU6050 basic motion data
UART application command / report
Final RTOS scheduling
Final APP Control FSM
Integrated board verification
```

---

# 3. 总体阶段顺序

```text
Phase 1  GPIO STM32 Impl
    ↓
Phase 2  Board Resource + CubeMX Configuration
    ↓
Phase 3  Software I2C
    ↓
Phase 4  LED Module
    ↓
Phase 5  Button Module
    ↓
Phase 6  DHT20 Environment Module
    ↓
Phase 7  MPU6050 Motion Module
    ↓
Phase 8  UART Application Communication
    ↓
Phase 9  RTOS Task / Event Design
    ↓
Phase 10 Final APP Integration
    ↓
Final Integrated Board Test
```

该顺序是当前默认开发路线。

如果后续发现真实硬件依赖要求局部调整顺序，可以专项评审，但不得为了并行开发破坏已冻结的层级依赖和接口合同。

---

# 4. Phase 1 — GPIO STM32 Impl

## 4.1 目标

为已经 Host Verified 的 `Platform GPIO` 提供 STM32F411 + HAL 的具体实现。

依赖：

```text
Platform GPIO Phase 1
STM32 HAL GPIO
现有 Impl / Board 绑定方式
```

## 4.2 范围

本阶段只解决 Platform GPIO 当前公开能力在 STM32 上的落地：

```text
configure
write
read
deinit
```

以及：

- Platform GPIO 配置到 HAL GPIO 配置的转换；
- GPIO Port / Pin 等具体硬件上下文的绑定方式；
- HAL / 参数错误向 Platform Error 的转换；
- 输出初始值与配置顺序；
- Impl 与 CubeMX / Board 资源之间的边界。

## 4.3 不做

```text
LED 产品语义
KEY 产品语义
Debounce
Soft I2C
EXTI
Button click state machine
```

## 4.4 完成门槛

至少满足：

```text
Platform GPIO 公共 API 不被随意修改
STM32 Impl 编译通过
无 APP / Service 反向依赖
Coding Standard Review PASS
```

最终目标板 GPIO 行为可在 Phase 2 的具体资源配置完成后联合验收。

---

# 5. Phase 2 — Board Resource + CubeMX Configuration

## 5.1 目标

冻结最终功能需要的板级 GPIO 资源，并完成 CubeMX / Board Binding。

## 5.2 至少分配

```text
LED GPIO
KEY GPIO
Software I2C SCL GPIO
Software I2C SDA GPIO
USART1 已有资源确认
```

软件 I2C 的 SCL / SDA 使用普通 GPIO，不启用 STM32 硬件 I2C 外设。

## 5.3 输出

- 资源分配表更新；
- CubeMX GPIO 配置；
- Board / Impl Context 绑定；
- GPIO Impl 目标板 Smoke Test。

## 5.4 完成门槛

```text
Keil Build PASS
目标板 GPIO 输入可读
目标板 GPIO 输出可控
LED / KEY / SCL / SDA 资源无冲突
CubeMX USER CODE / 生成代码边界符合规范
```

---

# 6. Phase 3 — Software I2C

## 6.1 目标

基于 Platform GPIO 实现与 STM32 HAL 解耦的软件 I2C 总线能力，为 DHT20 和 MPU6050 提供统一底层通信。

依赖链：

```text
Software I2C
    ↓
Platform GPIO
    ↓
STM32 GPIO Impl
```

## 6.2 基础能力

至少覆盖：

```text
START
STOP
ACK / NACK
Write Byte
Read Byte
Multi-byte Read / Write
Register-oriented transaction needed by sensors
Microsecond timing
```

具体对象模型、文件位置、微秒延时来源、超时策略在本 Phase 的专项设计中冻结。

## 6.3 约束

- Software I2C 不直接调用 `HAL_GPIO_xxx`；
- 不使用 RTOS tick delay 直接模拟 I2C bit timing；
- 不在正常路径逐 bit / byte / ACK 打 RTT 日志；
- 第一阶段只要求满足 DHT20 / MPU6050 实际需求，不构建过度通用 I2C Framework。

## 6.4 完成门槛

优先使用逻辑分析仪 / 目标器件验证：

```text
START / STOP 正确
Clock / Data 时序正确
Address + ACK 可观察
读写 transaction 正常
错误 / NACK 可返回
```

---

# 7. Phase 4 — LED Module

## 7.1 目标

在现有 Platform GPIO 与 Board GPIO Binding 基线上，建立轻量 LED 设备能力和提示灯 Service 语义。

冻结分层：

```text
Indicator Service
        ↓
Platform LED
        ↓
Platform GPIO
        ↓
STM32 GPIO Impl
```

Phase 4 不新增独立 `impl_led` 透传层。

专项设计：

```text
00_Doc/02_架构设计/LED_Phase1设计.md
```

## 7.2 Platform LED

第一阶段使用轻量 `platform_led_t`，不接入统一 `platform_device_t` / runtime device type。

基础能力：

```text
init
on
off
toggle
deinit
```

Status LED GPIO 继续复用 Phase 2 已验证的 PC13 Board Binding。

LED active level 作为产品编译期静态配置进入 `00_Config/project_config.h`，不得把 LOW/HIGH 极性泄漏到 Service / APP。

## 7.3 Indicator Service

提示事件：

```text
STOPPED
RUNNING
ONCE_SUCCESS
```

产品语义：

```text
STOPPED               -> LED OFF
RUNNING               -> LED ON
RUNNING 5 s report     -> LED stays ON
ONCE TX success        -> blink 3 times, then OFF
ONCE failure           -> stay OFF
```

Indicator Service 不维护 APP 真实 RUNNING / STOPPED 状态，不判断 UART 是否成功，只消费未来上层已经确认的语义事件。

## 7.4 闪烁时序

静态配置：

```text
blink count = 3
blink ON = 100 ms
blink OFF = 100 ms
```

统一进入 `00_Config/project_config.h`。

最终系统采用独立 Indicator Task，因此闪烁允许在该 Task Context 中使用 `platform_time_delay_ms()` 顺序延时。

Phase 4 不正式创建最终 Indicator Task；永久 Task 创建、优先级、栈大小和事件投递方式留给 Phase 9。

禁止：

```text
HAL_Delay in Service / smoke Task
osDelay / vTaskDelay bypassing Platform OS
ISR / HAL Callback blink delay
```

## 7.5 验证

目标板 Smoke Test 在 FreeRTOS Scheduler 启动后的临时 Task Context 中执行。

验证工具：

```text
LED visual observation
RTT / EasyLogger
Serial Assistant as communication regression observation
```

本 Phase 不要求逻辑分析仪。

完成 smoke 后必须删除临时验证入口并恢复正常固件路径。

## 7.6 完成门槛

```text
Platform LED Host Test PASS
Indicator Service Host Test PASS
Platform GPIO regression PASS
Keil Full Rebuild PASS
OFF / ON board behavior PASS
ONCE three-blink board behavior PASS
final OFF PASS
RTT smoke PASS
existing communication regression PASS
temporary smoke removed PASS
normal-path Keil rebuild PASS
Coding Standard Review PASS
```

---

# 8. Phase 5 — Button Module

## 8.1 目标

建立从 GPIO 输入状态到按键业务事件的完整链路。

预期职责：

```text
KEY Platform / BSP
    -> pressed / released

KEY Impl / Board binding
    -> concrete GPIO + active polarity

Button Service
    -> debounce
    -> single click
    -> double click
    -> long press
```

## 8.2 第一阶段事件

```text
SINGLE
DOUBLE
LONG >= 3000 ms
```

单击与双击存在判定冲突，因此 SINGLE 必须等待双击窗口结束后确认。

Button Service 应优先设计为可由周期 process / time input 驱动的状态机，以便 Host Test；是否独立 Button Task 留到 Phase 9 最终冻结。

## 8.3 完成门槛

Host / Board 测试至少覆盖：

```text
机械抖动不重复触发
单击只产生 SINGLE
双击只产生 DOUBLE
长按只产生 LONG
边界时间行为明确
```

---

# 9. Phase 6 — DHT20 Environment Module

## 9.1 目标

通过 Software I2C 实现 DHT20 初始化和温湿度采集，并向上提供环境数据语义。

预期链路：

```text
Environment Service / Device Module
       ↓
DHT20 device capability
       ↓
Software I2C
```

DHT20 不新增 STM32 专用 Impl；MCU 相关实现已经由 Software I2C -> Platform GPIO -> STM32 GPIO Impl 承担。

DHT20 具有明确身份、配置、生命周期和数据语义，专项设计阶段应评估复用统一 `platform_device_t` 模型，而不是照搬 LED 的轻量对象策略。

## 9.2 第一阶段数据

```text
Temperature
Relative Humidity
Validity / Error status
```

## 9.3 完成门槛

- 初始化可验证；
- 读取温湿度成功；
- 校验 / 状态 / 超时行为明确；
- 异常可通过返回值和 RTT WARN / ERROR 诊断；
- 不在 DHT20 模块中加入 APP 控制状态。

---

# 10. Phase 7 — MPU6050 Motion Module

## 10.1 命名

第一阶段统一使用：

```text
MPU6050 Motion / IMU Data Module
```

不称为 Attitude（姿态）模块，因为当前不做姿态解算。

## 10.2 目标

通过 Software I2C 完成 MPU6050 基础初始化和六轴数据读取。

MPU6050 具有明确身份、配置、生命周期和数据语义，专项设计阶段应评估复用统一 `platform_device_t` 模型。

第一阶段数据：

```text
Accel X / Y / Z
Gyro X / Y / Z
```

可以在专项设计中决定是否同时提供：

```text
raw values
physical units (g / deg/s)
```

## 10.3 当前明确不做

```text
Roll
Pitch
Yaw
DMP
Kalman Filter
Complementary Filter
高频姿态融合
```

## 10.4 完成门槛

- WHO_AM_I / 初始化验证通过；
- 六轴读取稳定；
- 原始值 / 单位转换规则明确；
- I2C 和设备异常可诊断；
- 不引入姿态算法范围。

---

# 11. Phase 8 — UART Application Communication

## 11.1 目标

在已有 UART DMA + RingBuffer + UART Service 上增加最终应用需要的命令解析和文本数据上报。

RX 链必须复用：

```text
UART DMA RX
    ↓
UART Service
    ↓
RingBuffer
    ↓
Communication Task / APP communication
    ↓
Command Parser
```

不得旁路现有链路。

## 11.2 命令

```text
START
STOP
ONCE
STATUS
HELP
```

需要在专项设计中冻结：

- 行结束符；
- 最大命令长度；
- 非法命令处理；
- 大小写策略；
- 命令到 APP Control Event 的映射；
- STATUS / HELP 响应文本。

## 11.3 数据上报

第一阶段使用文本格式，至少包含：

```text
DHT20 temperature / humidity
MPU6050 Accel XYZ / Gyro XYZ
```

5 s 周期报告和 ONCE 单次报告共用同一套格式化 / TX 能力。

## 11.4 完成门槛

- 完整命令可通过 RingBuffer 正确解析；
- 命令不会直接在 UART Service 内执行传感器 / LED 业务；
- 数据报告格式稳定；
- UART TX Buffer 生命周期符合既有异步合同；
- RTT 记录完整命令和业务 TX 状态，不逐字节刷屏。

---

# 12. Phase 9 — RTOS Task / Event Design

## 12.1 目标

在各模块职责和调用方式已经明确后，再决定最终 Task、周期调度、事件通知和资源所有权。

本阶段不是“为了使用 RTOS 而增加 Task”，但已冻结的真实并发职责应得到独立执行上下文。

当前已确认方向：

```text
Communication Task
Acquisition Task
Indicator Task
```

Button 是否独立 Task 留待 Phase 5 + Phase 9 决定。

本阶段需要明确：

```text
Communication processing context
Button periodic processing context
5 s acquisition scheduling
ONCE execution context
Indicator event delivery
Indicator Task priority / stack / queueing policy
APP control event delivery
UART async TX completion handling
```

## 12.2 Indicator Task

Indicator Task 的存在已经由 Phase 4 专项设计冻结，职责是：

```text
consume indicator events
serialize LED behavior
execute blocking blink timing using platform_time_delay_ms()
isolate LED timing from APP / UART / acquisition tasks
```

Phase 9 冻结其具体创建方式和事件机制，而不是重新讨论是否需要该 Task。

## 12.3 Software I2C 并发

第一阶段优先保证 DHT20 和 MPU6050 在同一 Acquisition Task 中串行访问 Software I2C，从结构上避免总线竞争。

只有出现多个真实并发访问者时，才设计 I2C Mutex；Mutex 必须覆盖完整 transaction。

## 12.4 完成门槛

- 每个 Task 有明确职责；
- 没有为 DHT20 / MPU6050 等设备机械创建独立 Task；
- Indicator Task 职责和事件缓存策略明确；
- ISR / Task 边界符合现有合同；
- 共享数据和 Buffer 所有权明确；
- 5 s 周期与 Button 时间基准明确。

---

# 13. Phase 10 — Final APP Integration

## 13.1 目标

由 APP 统一组合所有已经验证的能力，形成最终数据采集闭环。

APP 是采集状态唯一真实来源：

```text
STOPPED
RUNNING
```

控制事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

事件来源可以是 KEY 或 UART，但 APP 不维护两套状态。

## 13.2 最终行为

```text
Startup
 -> STOPPED
 -> submit indicator STOPPED
 -> UART RX active

START
 -> RUNNING
 -> submit indicator RUNNING
 -> every 5 s acquire DHT20 + MPU6050
 -> UART report

STOP
 -> STOPPED
 -> stop periodic acquisition/report
 -> submit indicator STOPPED

ONCE while STOPPED
 -> acquire DHT20 + MPU6050 once
 -> UART TX once
 -> TX success: submit indicator ONCE_SUCCESS
 -> remain STOPPED
```

## 13.3 完成门槛

最终需求文档中的验收场景全部通过，并完成：

```text
Keil Build
Target Board Test
UART Serial Assistant Test
RTT Log Observation
Button Interaction Test
Error-path Smoke Test
Coding Standard Review
Handoff Update
```

---

# 14. RTT / EasyLogger 横切要求

RTT 日志不是独立 Phase，而是所有 Phase 的共同验收内容。

基本规则：

```text
INFO  -> 初始化、功能启停、关键状态变化
DEBUG -> 采集摘要、完整命令、业务 TX 状态、必要内部状态
WARN  -> 可恢复 GPIO / I2C / Sensor / UART 异常
ERROR -> 初始化失败、关键操作失败
```

禁止：

```text
逐 UART byte 正常日志
逐 I2C bit / byte / ACK 正常日志
逐 LED on/off 边沿正常日志
ISR 大量格式化日志
每层重复打印同一成功信息
```

底层通过错误码向上传递状态，上层可统一记录关键初始化 / 业务结果。

---

# 15. Config 横切要求

稳定的产品静态参数应逐阶段纳入 `00_Config`，而不是散落在业务代码中。

最终至少包含：

```text
Acquisition report period = 5000 ms
Button debounce time
Button double-click window
Button long-press threshold = 3000 ms
Status LED active level
LED ONCE blink count = 3
LED blink ON = 100 ms
LED blink OFF = 100 ms
UART command / report limits if needed
```

具体宏名称由对应 Phase 专项设计冻结。

---

# 16. implementation_plan 使用规则

`development_roadmap.md` 与 `implementation_plan.md` 的职责必须区分：

```text
development_roadmap.md
    = 整个最终功能的阶段路线

implementation_plan.md
    = 当前唯一已确认 Phase 的可执行施工计划
```

禁止一次把 Phase 1 ~ Phase 10 全部写成一个超长 implementation plan 后连续实现。

每个 Phase 推荐流程：

```text
Inspect repository
    ↓
Discuss scope / design
    ↓
Freeze design document
    ↓
Update implementation_plan.md
    ↓
Codex / Agent implementation
    ↓
Host / Keil / Board verification
    ↓
Review
    ↓
Update handoff
    ↓
Enter next Phase
```

---

# 17. 当前下一阶段

当前执行阶段：

```text
Phase 4 — LED Module
```

专项设计已经冻结：

```text
00_Doc/02_架构设计/LED_Phase1设计.md
```

当前执行步骤以：

```text
00_Doc/04_Agent/implementation_plan.md
```

为唯一计划。

Phase 4 完成前：

- 不开始 Phase 5 Button；
- 不实现 Final APP Control FSM；
- 不正式创建永久 Indicator Task；
- 不增加 `impl_led` 透传层；
- 不将 LED 接入统一 Device Registry；
- 不使用 HAL_Delay 代替 Platform Time。
