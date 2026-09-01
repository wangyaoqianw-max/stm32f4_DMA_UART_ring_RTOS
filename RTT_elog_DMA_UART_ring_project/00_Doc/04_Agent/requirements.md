# Final Acquisition System Requirements

> 文档类型：Agent Requirements Baseline  
> 状态：Baseline  
> 版本：V2.0  
> 更新时间：2026-09-01  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 文档定位

本文件是 AI Agent 执行设计、编码和 Review 时使用的长期需求摘要。

最终业务行为的权威需求文件：

```text
00_Doc/00_项目需求/最终功能需求.md
```

本项目原有 UART DMA + RingBuffer + FreeRTOS 主线继续有效。本阶段是在已验证通信链路上增加 GPIO、软件 I2C、DHT20、MPU6050、按键和 APP 控制状态机，形成最终综合闭环。

若本文件与 `最终功能需求.md` 的业务行为冲突，以 `最终功能需求.md` 为准；若业务需求与硬件正确性或已冻结专项设计发生实质冲突，Agent 必须 STOP 并返回设计阶段确认。

---

# 2. 项目最终目标

最终系统闭环：

```text
KEY -----------------------------+
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

项目继续重点验证：

- UART 不定长接收；
- DMA 数据搬运；
- RingBuffer 字节流缓存；
- ISR / Task 协作；
- FreeRTOS；
- APP / Service / Platform / Impl 分层；
- 静态内存、Buffer 生命周期与并发合同；
- RTT 日志诊断；
- AI 辅助 Requirements -> Design -> Plan -> Implementation -> Test -> Handoff 流程。

---

# 3. 当前目标硬件与软件环境

```text
MCU        : STM32F411CEU6 / Cortex-M4F
Flash      : 512 KB
RAM        : 128 KB
UART       : USART1 / 115200 8N1
Sensor     : DHT20 + MPU6050
Input      : 1 x KEY
Indicator  : 1 x LED
I2C        : Software I2C over GPIO
RTOS       : CMSIS-RTOS2 + FreeRTOS
Log        : EasyLogger + SEGGER RTT
Toolchain  : Keil MDK-ARM + STM32CubeMX
```

DHT20 与 MPU6050 共用一条软件 I2C 总线。

当前最终功能不使用 STM32 硬件 I2C 外设。

SPI / LCD 当前暂停，不纳入本阶段最终验收。

---

# 4. 系统状态与唯一状态源

APP 层必须维护唯一的采集业务状态：

```text
STOPPED
RUNNING
```

启动初始化完成后：

```text
Acquisition = STOPPED
LED         = OFF
UART RX     = ACTIVE
RTT Log     = ACTIVE
```

按键和 UART 命令是两个控制入口，但不得分别维护两套采集状态。

所有控制输入最终必须转换为 APP 控制事件，并由同一状态机决定行为。

推荐控制事件语义：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

---

# 5. 按键需求

按键第一阶段必须支持：

- debounce；
- single click；
- double click；
- long press >= 3 s。

行为：

| 当前状态 | 按键事件 | 结果 |
| --- | --- | --- |
| STOPPED | 单击 | START，进入 RUNNING，LED 常亮 |
| STOPPED | 双击 | ONCE，采集并发送一次，保持 STOPPED |
| STOPPED | 长按 >= 3 s | 无额外业务动作 |
| RUNNING | 单击 | 保持 RUNNING |
| RUNNING | 双击 | 不执行额外单次采样 |
| RUNNING | 长按 >= 3 s | STOP，进入 STOPPED，LED 熄灭 |

双击包含第一次短按，因此 single-click 不能在第一次释放后立即提交，必须经过双击判定窗口。

消抖时间、双击窗口、长按阈值等静态参数必须进入 Config，不允许散布魔法数字。

---

# 6. UART 命令需求

PC 串口助手使用文本命令：

```text
START\r\n
STOP\r\n
ONCE\r\n
STATUS\r\n
HELP\r\n
```

行为：

| 命令 | STOPPED | RUNNING |
| --- | --- | --- |
| START | 进入 RUNNING | 保持 RUNNING，返回 already running |
| STOP | 保持 STOPPED，返回 already stopped | 进入 STOPPED |
| ONCE | 单次采集并发送，保持 STOPPED | 不额外采样，返回 already running |
| STATUS | 返回 STOPPED | 返回 RUNNING |
| HELP | 返回命令列表 | 返回命令列表 |

UART 命令必须复用已验证 RX 链：

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

禁止为命令控制建立第二套绕过 UART Service / RingBuffer 的接收路径。

UART Service 只负责通信能力，不得直接控制 LED、DHT20、MPU6050 或 APP 状态。

---

# 7. 周期采集与发送

第一阶段不做姿态计算，因此基础版本采用统一业务周期：

```text
Acquisition / Report Period = 5000 ms
```

RUNNING 状态每 5 s：

1. 读取 DHT20；
2. 读取 MPU6050；
3. 组织一组业务数据；
4. 通过既有 UART TX 链路发送到 PC；
5. 通过 RTT DEBUG 记录本次采集 / 发送摘要。

STOPPED 状态不得执行周期采集和周期发送。

5 s 周期必须进入 `00_Config` 产品静态配置。

后续若增加姿态算法，可重新把 MPU6050 高频采样周期和 5 s UART 上报周期拆开；第一阶段不得提前为未确认需求引入该复杂度。

---

# 8. DHT20 第一阶段需求

必须实现：

- 初始化；
- 通信状态检查；
- 温度读取；
- 相对湿度读取；
- 数据有效性判断；
- 明确的通信 / 数据错误返回。

DHT20 不得直接依赖 STM32 HAL GPIO API。

---

# 9. MPU6050 第一阶段需求

必须实现：

- 初始化；
- WHO_AM_I 验证；
- Accel X / Y / Z；
- Gyro X / Y / Z；
- 基础原始值或物理量转换；
- 明确的通信错误返回。

当前明确不实现：

```text
Roll
Pitch
Yaw
Complementary Filter
Kalman Filter
DMP
高频姿态融合
```

第一阶段 MPU6050 只是每 5 s 获取一次运动状态快照，不是实时姿态系统。

---

# 10. 软件 I2C 需求

软件 I2C 必须建立在 Platform GPIO 能力之上，协议逻辑不得直接调用 `HAL_GPIO_xxx`。

至少支持：

- START；
- STOP；
- ACK / NACK；
- byte write；
- byte read；
- multi-byte read / write；
- 满足 DHT20 与 MPU6050 的组合读写需求。

软件 I2C 需要微秒级时序能力。

禁止直接使用 `osDelay()` 或 FreeRTOS tick delay 实现 bit-level I2C 时序。

软件 I2C 的具体目录、公共对象模型和延时注入接口在专项设计阶段冻结；在专项设计完成前不得凭实现方便擅自扩大 Platform 公共接口。

第一阶段优先使用单一采集执行上下文串行访问两个设备，避免无必要的总线并发。

若未来拆为多个并发采集 Task，必须增加总线同步，并以完整 I2C transaction 为互斥范围。

---

# 11. LED 产品语义

LED 行为固定为：

```text
STOPPED               -> OFF
RUNNING               -> ON
RUNNING 周期发送       -> 保持 ON，不闪烁
ONCE TX SUCCESS        -> 闪烁 3 次，然后 OFF
ONCE sample/TX failure -> 保持 OFF
```

单次采集时，只有成功完成业务发送后才执行三次成功反馈闪烁。

若 UART 异步 TX 已有明确 TX Complete 事件，应优先以真正 TX Complete 作为“发送成功”语义；若当前阶段只能确认 DMA 发送请求已成功提交，专项设计必须明确这一限制。

LED 的有效电平属于 Board / BSP 语义，上层不得知道实际 GPIO 高低电平极性。

LED 闪烁不得在 ISR / HAL Callback 中执行阻塞延时。

---

# 12. UART 数据输出

传感器业务数据必须复用既有 TX 链路：

```text
APP / Communication
 -> UART Service
 -> Platform UART
 -> STM32 UART Impl
 -> HAL / DMA
 -> USART1
 -> PC Serial Assistant
```

第一阶段使用文本格式，便于观察，例如：

```text
ENV,T=25.34,H=62.18\r\n
IMU,AX=0.013,AY=-0.021,AZ=0.998,GX=0.12,GY=-0.42,GZ=0.08\r\n
```

不要求自定义二进制应用协议。

异步 TX Buffer 在 TX complete / error / cancel 前不得修改或失效，继续遵守现有 Platform UART Buffer 生命周期合同。

---

# 13. RTT / EasyLogger 需求

正式日志链保持：

```text
APP / Service
    -> service_log
    -> Platform Log
    -> EasyLogger Adapter
    -> EasyLogger / SEGGER RTT
```

RTT 的定位是运行状态与故障诊断，不是逐字节 Trace。

## INFO

至少记录：

- 系统初始化开始 / 完成；
- 关键模块初始化结果；
- DHT20 / MPU6050 初始化结果；
- START / STOP / ONCE；
- APP 采集状态变化；
- 有价值时记录控制来源 button / UART。

## DEBUG

可记录：

- 每 5 s 采集完成摘要；
- 完整 UART 命令；
- 一次业务 Report 发送开始 / 完成；
- ONCE 成功反馈。

## WARN / ERROR

至少覆盖：

- Software I2C NACK / timeout / bus error；
- DHT20 读取失败；
- MPU6050 WHO_AM_I / 读取失败；
- UART TX 失败；
- UART RX / RingBuffer overflow；
- 初始化失败。

正常运行时禁止持续打印：

- 每个 UART byte；
- 每个 I2C bit / byte / ACK；
- 每个 DMA 内部步骤；
- 高频无诊断价值日志。

ISR / HAL Callback 中禁止大量格式化日志。

初始化过程的可观测性由 APP / Service 根据各模块返回结果统一记录即可，不要求 Platform / Impl 为每个成功路径直接调用日志 API。

---

# 14. 推荐 RTOS 执行模型

第一阶段优先降低并发复杂度，而不是增加 Task 数量。

推荐职责：

```text
Communication Task
- 消费 UART RingBuffer
- 识别完整文本命令
- 转换为 APP 控制事件

Acquisition / Control execution context
- 维护 STOPPED / RUNNING
- 处理 button / UART 控制事件
- 每 5 s 执行 DHT20 + MPU6050 采集和 UART 上报
- 执行 ONCE

Button processing
- 周期扫描或独立轻量任务
- debounce / single / double / long
```

具体 Task 数量与 Queue / Notification / Timer 的使用在专项设计阶段确认。

不得为了展示 FreeRTOS 而无必要地为 DHT20、MPU6050、LED 分别建立 Task。

---

# 15. 分层与依赖要求

固定依赖规则：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

职责：

```text
APP
- 产品级采集状态机
- 控制事件决策
- 周期业务编排
- UART command 业务语义

Service
- UART Service
- Log Service
- RingBuffer 组合
- Button event recognition
- 可复用的数据处理服务

Platform
- UART / GPIO / OS / Log 等公共能力
- 软件 I2C 与 Board / BSP 的具体边界由专项设计冻结

Impl
- STM32 UART / GPIO / OS / Log 的具体实现
- HAL / CubeMX Handle / IRQ 适配

Vendor
- STM32 HAL / CMSIS / FreeRTOS / EasyLogger / RTT
```

禁止 APP / Service 为快速联调直接依赖 HAL、CubeMX Handle 或 Impl 私有接口。

---

# 16. 既有 UART / RingBuffer 基线继续冻结

新功能不得破坏已验证能力：

```text
DMA Circular + IDLE / HT / TC RX
Platform UART Event
UART Service
SPSC RingBuffer
APP Communication Task
caller-owned / static storage
RX ISR / Task boundary
UART async TX buffer lifetime contract
RingBuffer overflow detection
Service Log -> RTT
```

RingBuffer 继续遵守：

```text
Single Producer = UART Service RX callback
Single Consumer = APP Communication Task
no malloc/free
no ordinary mutex in current SPSC path
no silent overwrite
```

---

# 17. ISR、并发与内存要求

ISR / HAL Callback 允许：

- capture hardware state；
- 搬运必要数据；
- 更新轻量状态；
- 调用明确 ISR-safe 的通知接口；
- 尽快退出。

禁止：

- blocking；
- ordinary mutex；
- malloc/free；
- 完整命令解析；
- 传感器业务状态机；
- LED 闪烁延时；
- 大量格式化日志；
- 非 ISR-safe RTOS API。

项目核心链路继续优先 static / caller-owned storage。

---

# 18. 当前范围冻结

## 必须完成

- GPIO STM32 Impl 与目标板验证；
- LED；
- KEY；
- Button debounce / single / double / long；
- Software I2C；
- DHT20；
- MPU6050 六轴基础数据；
- APP 采集状态机；
- UART `START / STOP / ONCE / STATUS / HELP`；
- 5 s 周期采集与 UART 上报；
- ONCE 成功 LED 三闪反馈；
- RTT 初始化、控制、采集、收发及错误状态日志；
- 最终综合板测。

## 当前不做

- Roll / Pitch / Yaw；
- DMP / Kalman / Complementary Filter；
- SPI Platform / Impl；
- SPI LCD / GUI；
- W25Q64 / AT24C02；
- 蓝牙；
- 复杂自定义 UART 二进制协议；
- 与当前验收无关的通用框架扩展。

---

# 19. 配置要求

产品静态配置应集中到 `00_Config`。

至少后续需要纳入：

```text
Acquisition period = 5000 ms
Button debounce period
Button double-click window
Button long-press threshold = 3000 ms
LED one-shot success blink count = 3
LED blink interval
Sensor / command buffer sizes（若需要）
```

具体宏命名由专项设计与现有代码规范统一确定。

---

# 20. 最终验收核心场景

必须至少验证：

1. 上电后 STOPPED、LED 灭、UART RX / RTT 正常；
2. STOPPED 单击 -> RUNNING、LED 常亮；
3. RUNNING 每 5 s 向 PC 输出一组 DHT20 + MPU6050 数据；
4. RUNNING 长按 >= 3 s -> STOPPED、LED 灭、停止周期上报；
5. STOPPED 双击 -> 单次采集和发送 -> TX 成功后 LED 闪 3 次 -> STOPPED；
6. UART `START` / `STOP` 与按键控制使用同一真实状态；
7. UART `ONCE` 在 STOPPED 正确执行；
8. `STATUS` / `HELP` 返回明确文本；
9. 初始化、关键采集和 UART 收发过程在 RTT 中可观察；
10. I2C / Sensor / UART / RingBuffer 异常具有明确错误与日志，不静默失败。

---

# 21. 核心原则

```text
UART / RingBuffer 是已验证基础通信能力。
GPIO / Software I2C / Sensor 是新增设备能力。
APP Control FSM 是唯一业务状态源。
UART 与 KEY 只是控制入口。
UART 是业务数据输出终端。
RTT 是内部运行状态与诊断终端。
```

第一阶段目标是完成稳定、清晰、可验收的基础闭环，而不是继续扩大功能数量。