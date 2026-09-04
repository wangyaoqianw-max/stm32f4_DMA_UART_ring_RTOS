# 工程长期记忆与交接说明

更新时间：2026-09-04

> 本文件是 AI Agent 与人工开发者恢复工程上下文时的长期入口。
> 详细业务需求以 `00_Doc/00_项目需求/最终功能需求.md` 为准。
> 架构合同以 `00_Doc/04_Agent/architecture.md` 为准。
> 阶段路线以 `00_Doc/04_Agent/development_roadmap.md` 为准。
> 当前施工计划以 `00_Doc/04_Agent/implementation_plan.md` 为准。

---

# 1. 项目定位

工程根目录：

```text
RTT_elog_DMA_UART_ring_project/
```

目标环境：

```text
MCU        : STM32F411CEU6 / Cortex-M4F
UART       : USART1 / 115200 8N1
RTOS       : CMSIS-RTOS2 + FreeRTOS
Debug Log  : EasyLogger + SEGGER RTT
Sensors    : DHT20 + MPU6050
I2C        : Software I2C over GPIO
Input      : PA0 User Key
Indicator  : PC13 Status LED
```

最终目标：在已验证 UART DMA + RingBuffer + FreeRTOS + 五层架构基础上，完成按键控制、Software I2C、DHT20、MPU6050、LED 状态反馈、UART 文本命令和 APP Control FSM，形成完整数据采集系统。

---

# 2. 稳定架构合同

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

CubeMX 生成文件只承担初始化、IRQ / HAL Callback、Scheduler 和薄胶水，不承载长期业务逻辑。

---

# 3. 已验证基线

```text
Platform UART / STM32 UART Impl          VERIFIED
UART DMA RX / TX                         VERIFIED
UART Service                             VERIFIED
SPSC RingBuffer                          VERIFIED
APP Communication Phase 1                VERIFIED
Platform OS                              VERIFIED
Service Log / EasyLogger / RTT           VERIFIED
Platform GPIO / STM32 GPIO Impl          VERIFIED
Board GPIO Binding                       VERIFIED
Software I2C                             VERIFIED
LED / Indicator Module                   VERIFIED
Button Module Phase 5                    VERIFIED
DHT20 production module                  VERIFIED
DHT20 hardware connectivity              VERIFIED
MPU6050 hardware connectivity            VERIFIED
```

当前真实板级资源：

```text
PC13 -> Status LED, active LOW
PA0  -> User Key, Pull-Up, released HIGH / pressed LOW
PB6  -> Software I2C SCL
PB7  -> Software I2C SDA
PA9  -> USART1_TX
PA10 -> USART1_RX
```

DHT20 与 MPU6050 已通过共享 Software I2C 的临时目标板连通性检查。该事实只证明 MPU6050 硬件连接可用，不代表 Phase 7 production driver 已实现。

---

# 4. 已关闭阶段

```text
Phase 1  GPIO STM32 Impl                       COMPLETED
Phase 2  Board Resource + CubeMX              COMPLETED
Phase 3  Software I2C                         COMPLETED
Phase 4  LED Module                           COMPLETED
Phase 5  Button Module                        COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 6  DHT20 Environment Module             COMPLETED / HOST + KEIL + TARGET VERIFIED
```

DHT20 正式能力链：

```text
Future APP / Acquisition Service
            ↓
      Platform DHT20
            ↓
      Platform I2C
            ↓
      Platform GPIO
            ↓
      STM32 GPIO Impl
```

DHT20 只引用共享 `platform_i2c_t`，不拥有总线生命周期。后续 MPU6050 必须遵守同一共享资源原则。

---

# 5. 当前 Active Phase

```text
Phase 7 — MPU6050 Motion Module
STATUS: COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

MPU6050 production、Host contract test、Keil production 接入与目标板验证均已完成。目标板 RTT、逻辑分析仪、物理 sanity check 和断连/NACK negative smoke 由用户于 2026-09-04 确认通过。

正式专项设计：

```text
00_Doc/02_架构设计/MPU6050_Phase1设计.md
```

当前执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
MPU6050 Phase 7 Implementation Plan
Status: COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

下一次 Codex 应先读取：

```text
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/implementation_plan.md
00_Doc/02_架构设计/MPU6050_Phase1设计.md
00_Doc/02_架构设计/MPU6050参考文件/MPU6050寄存器英文版本.md
00_Doc/02_架构设计/MPU6050参考文件/MPU6050寄存器英文版本.pdf
00_Doc/02_架构设计/MPU6050参考文件/MPU6050数据手册_项目适用分析.md
03_Platform/platform_mcu/i2c/platform_i2c.h
03_Platform/platform_bsp/dht20/platform_dht20.h
03_Platform/platform_bsp/dht20/platform_dht20.c
00_Config/project_config.h
```

原始 MPU6050 Register Map 优先于项目蒸馏摘要。

---

# 6. Phase 7 冻结架构边界

正式能力链：

```text
Future APP / Acquisition Service
            ↓
      Platform MPU6050
            ↓
      Platform I2C
            ↓
      Platform GPIO
            ↓
      STM32 GPIO Impl
```

首版只实现 Platform MPU6050，不建立：

```text
service_mpu6050
impl_mpu6050
platform_device_t
registry / manager
MPU6050 private task
MPU6050-owned mutex
malloc/free
FIFO / DMP / DATA_RDY interrupt
attitude solution / filter
```

后续建立统一 Acquisition Service 同时调用 DHT20 + MPU6050；不要按传感器数量机械创建 Service 或 Task。

---

# 7. MPU6050 对象与 API 合同

轻量 caller-owned Context：

```c
typedef struct
{
    platform_i2c_t *i2c;
    uint8_t address;
    platform_bool_t initialized;
} platform_mpu6050_t;
```

首版 Measurement 公开：

```text
accelXRaw / accelYRaw / accelZRaw
gyroXRaw / gyroYRaw / gyroZRaw
accelXG / accelYG / accelZG
gyroXDps / gyroYDps / gyroZDps
```

MPU6050 内部温度虽然包含在 14-byte burst 中，但首版不作为业务输出；环境温度继续由 DHT20 提供。

公共 API 只保留：

```text
platform_mpu6050_init()
platform_mpu6050_read()
platform_mpu6050_deinit()
```

当前不建立通用 config struct，也不公开 range / filter / register helper API。

---

# 8. init() 冻结语义

MPU6050 上电默认处于 Sleep。

因此：

```text
initialized == true
=
I2C bound
+ WHO_AM_I verified
+ device awake
+ first-version configuration applied
+ read() legal
```

初始化顺序：

```text
WHO_AM_I     0x75 -> expect 0x68
PWR_MGMT_1   0x6B <- 0x01
CONFIG       0x1A <- 0x03
SMPLRT_DIV   0x19 <- 0x04
GYRO_CONFIG  0x1B <- 0x00
ACCEL_CONFIG 0x1C <- 0x00
```

配置语义：

```text
wake from Sleep
X-axis gyro PLL clock
DLPF_CFG = 3
internal output rate approximately 200 Hz
Gyro  ±250 dps
Accel ±2 g
```

内部约 200 Hz 更新率不等于 APP 采集周期。

当前产品统一采集 / 上报周期为：

```text
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

旧 MPU6050 摘要和旧 architecture 中若仍出现 5 s，视为过期描述，不得用于 Phase 7 实现。

原始 Register Map 没有要求普通 Sleep -> Awake 后固定等待 100 ms；手册出现的 100 ms 属于 reset sequence。首版不要无依据添加 wake delay。

初始化应采用提交式语义：任一步失败，不得留下 `initialized=true` 的半初始化对象。

---

# 9. Address / WHO_AM_I 关键语义

```text
AD0 low  -> I2C 7-bit address 0x68
AD0 high -> I2C 7-bit address 0x69
WHO_AM_I expected value is always 0x68
```

原始寄存器手册明确 WHO_AM_I 不反映 AD0 对地址最低位的影响。

禁止：

```text
WHO_AM_I == configured I2C address
```

正确规则：

```text
I2C transaction error -> preserve underlying platform_error_t
WHO_AM_I != 0x68       -> PLATFORM_ERR_NOT_FOUND
```

不得在 0x68 NACK 后静默 fallback 到 0x69。

---

# 10. 六轴读取合同

从 `ACCEL_XOUT_H = 0x3B` 一次读取 14 bytes：

```text
0..1   ACCEL_XOUT_H/L
2..3   ACCEL_YOUT_H/L
4..5   ACCEL_ZOUT_H/L
6..7   TEMP_OUT_H/L      // first version ignored
8..9   GYRO_XOUT_H/L
10..11 GYRO_YOUT_H/L
12..13 GYRO_ZOUT_H/L
```

使用：

```text
platform_i2c_write_read(..., &reg, 1, raw, 14)
```

必须形成：

```text
START
 -> address + W
 -> 0x3B
 -> Repeated START
 -> address + R
 -> 14-byte read
 -> ACK first 13 bytes
 -> NACK final byte
 -> STOP
```

每轴是 high-byte-first 16-bit two's complement。

换算：

```text
accel_g  = raw / 16384.0
 gyro_dps = raw / 131.0
```

`platform_mpu6050_read()` 必须 atomic commit；失败时不修改调用者已有 measurement。

---

# 11. Shared I2C / 并发合同

DHT20 与 MPU6050 共用 Software I2C。

第一阶段：

```text
one acquisition execution context
 -> DHT20 complete transaction
 -> MPU6050 complete transaction
```

不加 mutex。

未来若出现多个真实访问任务，mutex 必须覆盖完整 transaction，而不是单个 byte / START / STOP。

`platform_mpu6050_deinit()` 绝不能调用 `platform_i2c_deinit()`。

---

# 12. Phase 7 板测合同

继续使用：

```text
RTT / EasyLogger
+
Logic Analyzer on PB6/PB7
```

RTT 观察：

```text
WHO_AM_I / init result
AX/AY/AZ raw + g
GX/GY/GZ raw + dps
init/read error
```

禁止在软件 I2C bit/byte 级时序中刷日志。

逻辑分析仪初始化检查：

```text
WHO_AM_I read
PWR_MGMT_1 = 0x01
CONFIG = 0x03
SMPLRT_DIV = 0x04
GYRO_CONFIG = 0x00
ACCEL_CONFIG = 0x00
START / STOP / ACK / Repeated START
```

采样检查：

```text
register pointer = 0x3B
Repeated START
14-byte continuous read
ACK first 13 bytes
NACK final byte
STOP
```

物理 sanity check：

```text
静止：gyro 接近 0 dps，允许零偏；一个 accel 轴约 ±1 g
翻转/旋转：对应轴符号和幅值合理变化
```

允许最小 disconnect / NACK smoke，确认错误传播、无死锁和共享总线可恢复。

---

# 13. Codex 执行边界

Codex 可以：

```text
实现 platform_mpu6050.h/.c
增加最小必要 contract tests
更新 Keil production group
创建临时 target smoke harness
执行 Host / Keil 验证
指导/记录 RTT + Logic Analyzer 实板验证
在验证完成后清理临时 smoke
更新 Phase 7 execution record 和 handoff
```

Codex 不得自动继续：

```text
Phase 8 UART Application Communication
Acquisition Service
Final Acquisition Task
APP Control FSM
final UART sensor reporting
```

只有具备真实 Host、Keil、RTT、逻辑分析仪和物理 sanity check 证据后，才能把 Phase 7 标记为：

```text
COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

当前状态：

```text
COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

当前自动执行证据：

```text
Host regression                        28 / 28 PASS
Keil production Full Rebuild           0 errors / 20 historical warnings
Keil attached target smoke rebuild     0 errors / 20 historical warnings
Keil final production Full Rebuild     0 errors / 20 historical warnings
MPU6050 production warning             0 new warnings
Temporary smoke source/group/path      REMOVED
Normal production startup              RESTORED
Architecture dependency review         PASS
```

当前实现文件：

```text
03_Platform/platform_bsp/mpu6050/platform_mpu6050.h
03_Platform/platform_bsp/mpu6050/platform_mpu6050.c
Tests/platform_mpu6050/test_platform_mpu6050.c
```

目标板验证结果（用户于 2026-09-04 确认）：RTT / EasyLogger 初始化与六轴输出通过；PB6/PB7 初始化寄存器事务通过；`0x3B` 起始 14-byte burst 与末字节 NACK 通过；静止/平移/翻转/旋转 sanity check 通过；最小断连/NACK smoke 通过。

---

# 14. 后续路线

```text
Phase 6  DHT20 Environment Module       COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 7  MPU6050 Motion Module          COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
Phase 8  UART Application Communication
Phase 9  RTOS Task / Event Design
Phase 10 Final APP Integration
Final Integrated Board Test
```

当前停止点：Phase 7 已完成并关闭；不得自动进入 Phase 8。
