# Final Acquisition System Development Roadmap

> 文档类型：Development Phase Roadmap  
> 状态：Baseline  
> 日期：2026-09-04  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 文档职责

本文件回答：

```text
有哪些开发 Phase？
各 Phase 依赖什么？
完成门槛是什么？
当前下一步是什么？
```

详细业务需求：`00_Doc/00_项目需求/最终功能需求.md`  
长期架构：`00_Doc/04_Agent/architecture.md`  
当前施工计划：`00_Doc/04_Agent/implementation_plan.md`

每个 Phase 必须先专项设计，再生成 / 替换当前 implementation plan。

---

# 2. 已验证基础能力

```text
UART Platform / STM32 Impl                    VERIFIED
UART DMA RX / TX                              VERIFIED
UART Service                                  VERIFIED
SPSC RingBuffer                               VERIFIED
APP Communication Phase 1                     VERIFIED
Platform OS                                   VERIFIED
Service Log + EasyLogger + RTT                 VERIFIED
Platform GPIO + STM32 Impl + Board Binding     VERIFIED
Software I2C                                  VERIFIED
LED / Indicator Module                        VERIFIED
Button / Button Service                       VERIFIED
DHT20 hardware connectivity                   VERIFIED
MPU6050 hardware connectivity                 VERIFIED
```

DHT20 与 MPU6050 已通过真实共享 Software I2C 总线连通性板测，当前硬件连接风险已排除。

最终闭环仍缺：

```text
DHT20 production module
MPU6050 production module
UART application commands / report
Permanent RTOS task / event organization
Final APP Control FSM
Integrated target-board verification
```

---

# 3. 总体阶段

```text
Phase 1  GPIO STM32 Impl                         COMPLETED
Phase 2  Board Resource + CubeMX Configuration   COMPLETED
Phase 3  Software I2C                            COMPLETED
Phase 4  LED Module                              COMPLETED
Phase 5  Button Module                           COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 6  DHT20 Environment Module                COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 7  MPU6050 Motion Module                   NEXT AFTER PHASE 6
Phase 8  UART Application Communication
Phase 9  RTOS Task / Event Design
Phase 10 Final APP Integration
Final Integrated Board Test
```

---

# 4. 已完成阶段摘要

## Phase 1 / 2 — GPIO + Board

完成 STM32 GPIO Impl、Board Binding、CubeMX 配置和目标板 GPIO 验证。

冻结资源：

```text
PC13 Status LED
PA0 User Key
PB6 Soft I2C SCL
PB7 Soft I2C SDA
USART1 existing pins
```

## Phase 3 — Software I2C

```text
Master-only
7-bit
synchronous
Platform GPIO based
Repeated START
microsecond timing
Host + Keil + target smoke verified
```

## Phase 4 — LED

```text
Indicator Service -> Platform LED -> Platform GPIO
STOPPED -> OFF
RUNNING -> ON
ONCE_SUCCESS -> 3 blinks -> OFF
Host + Keil + target verified
```

## Phase 5 — Button

专项设计：

```text
00_Doc/02_架构设计/Button_Phase1设计.md
```

正式链：

```text
Platform GPIO
 -> Platform Button PRESSED / RELEASED
 -> Button Service SINGLE / DOUBLE / LONG
 -> future APP control
```

冻结参数：

```text
active LOW / Pull-Up
sample 10 ms
debounce 30 ms
double 300 ms
long 3000 ms
```

Phase 5：`COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED`。

---

# 5. Phase 6 — DHT20 Environment Module

当前阶段已完成专项设计、production 实现、Host 回归、Keil 编译，以及 RTT 与逻辑分析仪实板验证。

专项设计：

```text
00_Doc/02_架构设计/DHT20_Phase1设计.md
```

当前执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
DHT20 Phase 6 Implementation Plan
```

正式链：

```text
Future APP / Acquisition Service
    -> Platform DHT20
    -> Platform Software I2C
    -> Platform GPIO
    -> STM32 GPIO Impl
```

第一版冻结边界：

```text
Platform-only DHT20 device capability
no service_dht20
no impl_dht20
no platform_device_t
no malloc/free
no private DHT20 task
no DHT20-owned mutex
shared platform_i2c_t is non-owned reference
```

必须实现：

```text
platform_dht20_init()
platform_dht20_read()
platform_dht20_deinit()
status / Busy handling
frame CRC8
OTP CRC_flag validation
CalibrationEnable validation
20-bit raw RH/T parsing
float physical conversion
atomic measurement output
```

协议关键点：

```text
7-bit address = 0x38
measure command = AC 33 00
STOP after command
wait >= 80 ms
new START read 7 bytes
final CRC byte followed by master NACK
```

产品统一采集 / 上报周期调整为：

```text
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

该周期属于产品配置，不属于 DHT20 协议常量。DHT20 规格书也建议约每 2 秒测量一次以限制自热影响。

验证策略：

```text
Keil production build
RTT / EasyLogger observation
Logic analyzer on PB6/PB7
continuous ~2 s repeated target read
```

逻辑分析仪至少确认：

```text
START -> 0x70 ACK -> AC ACK -> 33 ACK -> 00 ACK -> STOP
>= 80 ms
START -> 0x71 ACK -> 7-byte read -> final NACK -> STOP
```

不要求 Fake I2C / Host protocol harness；已有底层能力直接走完整真实链路验证。

当前验证状态：

```text
Host regression                    27 / 27 PASS
Keil temporary Smoke compile       PASS / 0 errors
Keil final production rebuild      PASS / 0 errors
Keil attached target Smoke rebuild PASS / 0 errors
DHT20 new warning                  0
RTT target observation             PASS
Logic analyzer PB6/PB7             PASS
continuous ~2 s target read        PASS
```

目标板验证结果：RTT 初始化与连续读取均返回 0，温湿度数据连续合理；逻辑分析仪确认 `AC 33 00` 写事务、约 80 ms 等待、7-byte 读事务末字节 NACK，以及约 2 s 连续采集。临时 Smoke 已清理，production 启动路径已恢复。

---

# 6. Phase 7 — MPU6050 Motion Module

第一阶段只做：

```text
WHO_AM_I
initialization
Accel X / Y / Z
Gyro X / Y / Z
raw / physical-unit conversion as designed
```

不做 Roll / Pitch / Yaw、DMP、Kalman、Complementary Filter 和高频姿态融合。

硬件连通性已提前确认，但 production module 必须等待 Phase 6 关闭后再进入正式设计/实现。

---

# 7. Phase 8 — UART Application Communication

复用现有：

```text
UART DMA RX -> UART Service -> RingBuffer -> Communication Task
```

命令：

```text
START
STOP
ONCE
STATUS
HELP
```

不得建立第二套 UART RX 路径。

---

# 8. Phase 9 — RTOS Task / Event Design

已确认方向：

```text
Communication Task
Acquisition Task
Indicator Task
```

永久 Button processing context、priority、stack、Button -> APP IPC、Indicator event delivery 等在本 Phase 冻结。

后续统一 Acquisition Service / Acquisition Task 串行调用 DHT20 + MPU6050；不按设备数量机械创建独立 Task。

---

# 9. Phase 10 — Final APP Integration

APP 唯一状态：

```text
STOPPED
RUNNING
```

统一事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

最终闭环：Button / UART -> APP FSM -> Acquisition / Indicator / UART Report。

RUNNING 第一阶段统一采集 / 上报周期为 2000 ms。

---

# 10. 当前下一步

```text
Phase 5 Button      CLOSED
Phase 6 DHT20       CLOSED / HOST + KEIL + TARGET VERIFIED
```

流程：

```text
Phase 6 implementation complete
 -> Host regression PASS
 -> Keil production build PASS
 -> RTT target smoke PASS
 -> Logic analyzer verification PASS
 -> temporary smoke removed
 -> production startup restored
```

Phase 6 已关闭；本次提交停止于此，不实现 Phase 7。
