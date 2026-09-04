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
DHT20 production module                       VERIFIED
MPU6050 hardware connectivity                 VERIFIED
MPU6050 production module                     VERIFIED
```

DHT20 与 MPU6050 已在真实共享 Software I2C 总线上完成 production 级目标板验证；两者当前均为可复用 Platform Sensor 能力。

最终闭环仍缺：

```text
UART application commands / report
Permanent RTOS task / event organization
Unified Acquisition Service / Task
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
Phase 7  MPU6050 Motion Module                   COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 8  UART Application Communication          NEXT / DESIGN PENDING
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

正式专项设计：

```text
00_Doc/02_架构设计/DHT20_Phase1设计.md
```

正式链：

```text
Future APP / Acquisition Service
    -> Platform DHT20
    -> Platform Software I2C
    -> Platform GPIO
    -> STM32 GPIO Impl
```

第一版边界：

```text
Platform-only DHT20 capability
no service_dht20
no impl_dht20
no platform_device_t
no malloc/free
no private task / mutex
shared platform_i2c_t is non-owned reference
```

核心能力：

```text
platform_dht20_init()
platform_dht20_read()
platform_dht20_deinit()
Busy / CRC / Calibration validation
20-bit raw RH/T parsing
float physical conversion
atomic measurement output
```

产品统一采集 / 上报周期：

```text
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

验证状态：

```text
Host regression                    27 / 27 PASS
Keil production                    PASS
RTT target observation             PASS
Logic analyzer PB6/PB7             PASS
continuous ~2 s target read        PASS
Temporary Smoke                    REMOVED
Production startup                 RESTORED
```

Phase 6：`COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED`。

---

# 6. Phase 7 — MPU6050 Motion Module

正式专项设计：

```text
00_Doc/02_架构设计/MPU6050_Phase1设计.md
```

正式能力链：

```text
Future APP / Acquisition Service
    -> Platform MPU6050
    -> Platform Software I2C
    -> Platform GPIO
    -> STM32 GPIO Impl
```

第一版边界：

```text
Platform-only MPU6050 capability
no service_mpu6050
no impl_mpu6050
no platform_device_t
no registry / manager
no private task / mutex
no FIFO / DMP / DATA_RDY interrupt
no attitude algorithm
shared platform_i2c_t is non-owned reference
```

核心能力：

```text
platform_mpu6050_init()
platform_mpu6050_read()
platform_mpu6050_deinit()
WHO_AM_I fixed expectation = 0x68
I2C 7-bit address = 0x68 / 0x69 according to AD0
wake + fixed register configuration
one 14-byte burst read from 0x3B
signed raw parsing
raw -> g / dps conversion
atomic measurement output
```

固定首版配置：

```text
PWR_MGMT_1   = 0x01
CONFIG       = 0x03
SMPLRT_DIV   = 0x04
GYRO_CONFIG  = 0x00   -> ±250 dps
ACCEL_CONFIG = 0x00   -> ±2 g
```

内部约 200 Hz output rate 不等于 APP 采集周期；产品周期继续使用 `PROJECT_ACQUISITION_PERIOD_MS = 2000U`。

验证状态：

```text
Host regression                        28 / 28 PASS
Keil production Full Rebuild           0 errors / historical warnings only
MPU6050 production warning             0 new warnings
RTT / EasyLogger target smoke          PASS
Logic analyzer initialization          PASS
0x3B 14-byte burst + final NACK         PASS
static / translate / flip / rotate     PASS
disconnect / NACK negative smoke       PASS
Temporary Smoke                        REMOVED
Production startup                     RESTORED
Architecture review                    PASS
```

Phase 7：`COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED`。

---

# 7. Phase 8 — UART Application Communication

当前下一阶段。进入 production 编码前必须先专项设计。

复用现有：

```text
UART DMA RX -> UART Service -> RingBuffer -> Communication Task
```

计划应用命令：

```text
START
STOP
ONCE
STATUS
HELP
```

计划业务输出：

```text
DHT20 environment data
MPU6050 six-axis data
system status / command response
```

不得建立第二套 UART RX 路径，不得把应用命令语义塞入 UART Service。

---

# 8. Phase 9 — RTOS Task / Event Design

已确认方向：

```text
Communication Task
Acquisition Task
Indicator Task
```

永久 Button processing context、priority、stack、Button -> APP IPC、Indicator event delivery 等在本 Phase 冻结。

统一 Acquisition Service / Acquisition Task 串行调用 DHT20 + MPU6050；不按设备数量机械创建独立 Task。

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
Phase 7 MPU6050     CLOSED / HOST + KEIL + TARGET VERIFIED
Phase 8 UART App    NEXT / DESIGN PENDING
```

当前流程：

```text
Phase 7 complete
 -> Host regression PASS
 -> Keil production build PASS
 -> RTT / Logic Analyzer target verification PASS
 -> physical / negative smoke PASS
 -> temporary smoke removed
 -> production startup restored
 -> Phase 8 design discussion next
```

Phase 7 已关闭；当前不得直接进入 Phase 8 production implementation，先完成 UART Application Communication 专项设计。
