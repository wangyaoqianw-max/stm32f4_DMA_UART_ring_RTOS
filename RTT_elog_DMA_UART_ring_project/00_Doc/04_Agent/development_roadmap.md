# Final Acquisition System Development Roadmap

> 文档类型：Development Phase Roadmap  
> 状态：Baseline  
> 日期：2026-09-03  
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
```

最终闭环仍缺：

```text
DHT20 environment data
MPU6050 basic motion data
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
Phase 6  DHT20 Environment Module                NEXT / DESIGN PENDING
Phase 7  MPU6050 Motion Module
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
Host + Keil + DHT20 target smoke verified
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

验证：

```text
Platform Button Host Test          PASS
Platform BSP Button Host Test      PASS
Button Service Host Test           PASS
Keil normal production rebuild     PASS
FreeRTOS Button + Indicator smoke  PASS
Serial Assistant / RTT             PASS
LED visual mapping                 PASS
Existing UART regression           PASS
Temporary smoke cleanup            PASS
Coding Standard Review             PASS
```

Phase 5：`COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED`。

---

# 5. Phase 6 — DHT20 Environment Module

当前下一阶段，只进入专项设计，不直接编码。

目标：

```text
Software I2C
    ↓
DHT20 device capability
    ↓
Environment data / service semantics
```

第一阶段至少解决：

```text
initialization
communication / status check
temperature
relative humidity
data validity / CRC or status handling
error semantics
Host Test
target-board verification
```

设计阶段必须先确认：

```text
DHT20 Platform / Service boundary
是否使用 platform_device_t
I2C bus ownership
address / command / wait timing config
raw data -> physical value conversion
measurement lifecycle
error model
Host Test fake I2C strategy
FreeRTOS target smoke strategy
```

在这些边界冻结前不写生产 DHT20 代码。

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

Phase 5 Smoke Task / Queue 不能作为永久架构依据。

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

---

# 10. 当前下一步

```text
Phase 5 Button      CLOSED
Phase 6 DHT20       NEXT / DESIGN PENDING
```

流程：

```text
Inspect DHT20 + current Software I2C baseline
 -> Discuss design
 -> Freeze DHT20 design document
 -> Replace implementation_plan.md with Phase 6 plan
 -> Codex implementation
```

不得在设计冻结前直接进入 Phase 6 编码，也不得跳到 Phase 7+。
