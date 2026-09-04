# MPU6050 Phase 7 专项设计

> 文档类型：Phase Design / Architecture Decision  
> 状态：FROZEN FOR IMPLEMENTATION  
> 日期：2026-09-04  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 目标与范围

Phase 7 只实现 MPU6050 六轴运动数据的 Platform 设备能力：

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

首版目标：

```text
WHO_AM_I 身份验证
退出 Sleep
基础寄存器配置
Accel X/Y/Z
Gyro X/Y/Z
14-byte burst read
raw -> g / dps 转换
错误传播
RTT + 逻辑分析仪目标板验证
```

首版明确不做：

```text
service_mpu6050
impl_mpu6050
platform_device_t / registry / manager
MPU6050 private task
MPU6050-owned mutex
malloc/free
FIFO / DMP / DATA_RDY interrupt
Roll / Pitch / Yaw
Complementary / Kalman Filter
hardware I2C replacement
```

后续由统一 Acquisition Service 编排 DHT20 + MPU6050，不为单个传感器建立空转发 Service。

---

# 2. 设计依据

参考优先级：

```text
1. 00_Doc/02_架构设计/MPU6050参考文件/MPU6050寄存器英文版本.pdf
2. 00_Doc/02_架构设计/MPU6050参考文件/MPU6050寄存器英文版本.md
3. 00_Doc/02_架构设计/MPU6050参考文件/MPU6050数据手册_项目适用分析.md
4. 当前 handoff / project_config / 已验证 Software I2C 与 DHT20 设计
```

原始寄存器手册优先于项目蒸馏摘要；当前产品统一采集周期以 `PROJECT_ACQUISITION_PERIOD_MS = 2000U` 为准，旧文档中的 5 s 描述不得继续使用。

---

# 3. 共享 Software I2C 合同

MPU6050 与 DHT20 共用同一 `platform_i2c_t`。MPU6050 只保存非拥有引用：

```text
platform_mpu6050_t -> platform_i2c_t *
```

因此 `platform_mpu6050_deinit()` 不得调用 `platform_i2c_deinit()`。

第一阶段由同一个采集执行上下文串行访问 DHT20 和 MPU6050，不增加 mutex。只有未来出现多个真实并发访问者时才增加互斥，并且互斥范围必须覆盖完整 I2C transaction。

寄存器读取直接复用：

```c
platform_i2c_write_read(i2c, address, &reg, 1U, data, length);
```

其语义为：写寄存器地址 -> Repeated START -> 连续读取。

---

# 4. 对象模型

采用 caller-owned 轻量 Context：

```c
#define PLATFORM_MPU6050_INITIALIZER {0}

typedef struct
{
    platform_i2c_t *i2c;
    uint8_t address;
    platform_bool_t initialized;
} platform_mpu6050_t;
```

`address` 为 7-bit 地址，可由板级装配传入 `0x68` 或 `0x69`。Driver 不静默扫描地址，也不在 NACK 后自动 fallback。

---

# 5. Measurement 数据模型

```c
typedef struct
{
    int16_t accelXRaw;
    int16_t accelYRaw;
    int16_t accelZRaw;

    int16_t gyroXRaw;
    int16_t gyroYRaw;
    int16_t gyroZRaw;

    float accelXG;
    float accelYG;
    float accelZG;

    float gyroXDps;
    float gyroYDps;
    float gyroZDps;
} platform_mpu6050_measurement_t;
```

14-byte burst 会同时读到温度寄存器，但首版不把 MPU6050 内部温度暴露为业务数据；DHT20 继续承担环境温度来源。

---

# 6. 公共 API

首版只公开：

```c
platform_error_t platform_mpu6050_init(
    platform_mpu6050_t *mpu6050,
    platform_i2c_t *i2c,
    uint8_t address);

platform_error_t platform_mpu6050_read(
    platform_mpu6050_t *mpu6050,
    platform_mpu6050_measurement_t *measurement);

platform_error_t platform_mpu6050_deinit(
    platform_mpu6050_t *mpu6050);
```

不公开 `read_register()`、`write_register()`、`configure()`、range setter、sleep/wake/reset 等扩展 API。当前没有第二套真实配置需求，不建立 `platform_mpu6050_config_t`。

---

# 7. init() 生命周期语义

MPU6050 上电默认 Sleep，因此 `init()` 不能只绑定对象。

冻结语义：

```text
initialized == true
=
I2C 已绑定
+ WHO_AM_I 正确
+ 已退出 Sleep
+ 首版配置全部成功
+ read() 可直接使用
```

初始化顺序：

```text
validate arguments / current state
 -> require initialized platform_i2c_t
 -> read WHO_AM_I (0x75)
 -> require 0x68
 -> write PWR_MGMT_1   (0x6B) = 0x01
 -> write CONFIG       (0x1A) = 0x03
 -> write SMPLRT_DIV   (0x19) = 0x04
 -> write GYRO_CONFIG  (0x1B) = 0x00
 -> write ACCEL_CONFIG (0x1C) = 0x00
 -> commit i2c / address / initialized
```

`PWR_MGMT_1 = 0x01`：`SLEEP=0`，`CLKSEL=1`，选择 X-axis gyroscope PLL。

原始 Register Map 没有规定普通 Sleep -> Awake 必须固定等待 100 ms，因此首版不加入无依据的 wake delay。手册中的 100 ms 属于 reset sequence，不等价于普通唤醒。

初始化采用提交式语义：任一步失败时对象不得进入半初始化状态。

---

# 8. WHO_AM_I 与地址语义

```text
I2C address : 0x68 / 0x69，由 AD0 决定
WHO_AM_I    : 固定期望 0x68
```

WHO_AM_I 不反映 AD0 地址最低位，因此禁止使用 `whoAmI == address` 判断。

若 I2C 事务成功但 WHO_AM_I != 0x68，返回：

```text
PLATFORM_ERR_NOT_FOUND
```

底层 NACK / timeout / IO 错误原样向上传递。

---

# 9. 首版固定配置

```text
WHO_AM_I      0x75 -> expect 0x68
PWR_MGMT_1    0x6B <- 0x01
CONFIG        0x1A <- 0x03
SMPLRT_DIV    0x19 <- 0x04
GYRO_CONFIG   0x1B <- 0x00   // ±250 dps
ACCEL_CONFIG  0x1C <- 0x00   // ±2 g
```

DLPF=3、SMPLRT_DIV=4 时内部更新约 200 Hz。该内部采样率与 APP 采集周期不是同一概念。

产品统一采集周期保持：

```text
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

---

# 10. read() 与数据解析

从 `ACCEL_XOUT_H = 0x3B` 一次 burst read 14 bytes：

```text
0..1   ACCEL_XOUT_H/L
2..3   ACCEL_YOUT_H/L
4..5   ACCEL_ZOUT_H/L
6..7   TEMP_OUT_H/L      // 首版忽略
8..9   GYRO_XOUT_H/L
10..11 GYRO_YOUT_H/L
12..13 GYRO_ZOUT_H/L
```

事务：

```text
START -> address+W -> ACK -> 0x3B -> ACK
      -> Repeated START -> address+R -> ACK
      -> 14-byte read
      -> ACK first 13 bytes
      -> NACK final byte
      -> STOP
```

每轴为高字节优先 16-bit two's complement：

```c
(int16_t)(((uint16_t)high << 8U) | (uint16_t)low)
```

首版换算：

```c
accel_g  = (float)accel_raw / 16384.0f;
gyro_dps = (float)gyro_raw  / 131.0f;
```

---

# 11. Atomic Output

`platform_mpu6050_read()` 必须先在本地 measurement 中完成全部读取、解析与转换，成功后一次性提交给调用者。

任何失败都不得修改调用者已有 measurement 内容。

---

# 12. Error Semantics

使用现有 `platform_error_t`，不建立 MPU6050 私有错误码体系：

```text
NULL pointer              -> PLATFORM_ERR_NULL_POINTER
invalid address           -> PLATFORM_ERR_INVALID_PARAM
repeated init             -> PLATFORM_ERR_ALREADY_INITIALIZED
I2C not initialized       -> PLATFORM_ERR_NOT_INITIALIZED
read before init          -> PLATFORM_ERR_NOT_INITIALIZED
I2C error                 -> preserve underlying error
WHO_AM_I != 0x68          -> PLATFORM_ERR_NOT_FOUND
```

---

# 13. deinit() 合同

只清理 MPU6050 本对象：

```text
i2c         -> NULL
address     -> 0
initialized -> false
```

不得关闭共享 I2C，也不额外发送 reset / sleep 命令。

---

# 14. 板测与日志合同

目标板验证继续使用：

```text
RTT / EasyLogger
+
Logic Analyzer on PB6/PB7
```

RTT 用于观察：

```text
WHO_AM_I / init result
Accel raw / g
Gyro raw / dps
init / read error
```

禁止在软件 I2C bit/byte 时序路径内打印大量日志。

逻辑分析仪验证初始化事务、Repeated START、ACK/NACK 和 14-byte burst。

物理 sanity check：

```text
静止：gyro 接近 0 dps，允许零偏；某 accel 轴约 ±1 g
翻转/旋转：相关轴符号与幅值合理变化
```

允许最小断连/NACK smoke，确认无死锁且总线可恢复。

---

# 15. 临时 Smoke Harness 边界

Codex 实现阶段允许创建最小临时 MPU6050 smoke 入口用于板测，但不得提前实现：

```text
Acquisition Service
Final Acquisition Task
APP START / STOP / ONCE FSM
UART sensor report integration
```

板测通过后必须清理临时 task/source/Keil test group/include path，并恢复正常 production startup。

---

# 16. Phase 7 完成门槛

只有以下全部满足后才可标记 Phase 7 COMPLETED：

```text
Platform MPU6050 production module complete
shared I2C non-owning lifetime respected
WHO_AM_I semantics correct
fixed init configuration correct
14-byte burst read correct
signed parsing correct
raw -> g/dps conversion correct
failed read preserves caller output
no service_mpu6050 / impl_mpu6050 / private task / mutex
Host regression PASS
Keil production build PASS
no new MPU6050 production warnings
RTT target smoke PASS
logic analyzer init sequence PASS
logic analyzer burst read PASS
physical sanity check PASS
temporary smoke removed
normal production rebuild PASS
architecture review PASS
```
