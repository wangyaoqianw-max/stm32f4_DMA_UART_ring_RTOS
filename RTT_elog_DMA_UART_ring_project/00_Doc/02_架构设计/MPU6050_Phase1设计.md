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

本 Phase 以以下文件为参考：

```text
00_Doc/02_架构设计/MPU6050参考文件/MPU6050寄存器英文版本.md
00_Doc/02_架构设计/MPU6050参考文件/MPU6050寄存器英文版本.pdf
00_Doc/02_架构设计/MPU6050参考文件/MPU6050数据手册_项目适用分析.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/handoff.md
03_Platform/platform_mcu/i2c/platform_i2c.h
03_Platform/platform_bsp/dht20/platform_dht20.h/.c
```

原始寄存器手册优先级高于项目蒸馏摘要；项目周期等产品决策以当前 Architecture / Config 为准。

---

# 3. 与共享 Software I2C 的关系

MPU6050 与 DHT20 共用同一 `platform_i2c_t`。

MPU6050 对象只持有非拥有引用：

```text
platform_mpu6050_t -> platform_i2c_t *
```

因此：

```text
platform_mpu6050_deinit()
不得调用 platform_i2c_deinit()
```

第一阶段只有一个采集执行上下文顺序访问 DHT20 / MPU6050，不增加 mutex。若以后出现多个真实并发访问者，mutex 必须覆盖完整 I2C transaction。

MPU6050 寄存器读使用现有同步接口：

```c
platform_i2c_write_read(i2c, address, &reg, 1U, data, length);
```

该事务必须保持：寄存器地址写入 -> Repeated START -> 连续读取。

---

# 4. 对象模型

采用 caller-owned 轻量 Context，不引入通用设备基类：

```c
#define PLATFORM_MPU6050_INITIALIZER {0}

typedef struct
{
    platform_i2c_t *i2c;
    uint8_t address;
    platform_bool_t initialized;
} platform_mpu6050_t;
```

`address` 保存 7-bit I2C 地址。

地址属于板级装配输入，Driver 不静默探测 `0x68/0x69`，也不在 `0x68` NACK 后自动 fallback 到 `0x69`。

---

# 5. Measurement 数据模型

首版公开六轴 raw + physical data：

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

14-byte burst 会读到 `TEMP_OUT_H/L`，但 MPU6050 内部温度不是首版业务输出，因此不放入公共 measurement。DHT20 继续承担环境温度数据来源。

---

# 6. 公共 API

Phase 7 只公开：

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

以下能力保持 `.c` 私有或暂不实现：

```text
read_register / write_register
configure
set_accel_range
set_gyro_range
set_dlpf
sleep / wake / reset
```

当前没有第二种实际配置需求，不建立 `platform_mpu6050_config_t`。

---

# 7. init() 生命周期语义

MPU6050 上电默认处于 Sleep，因此 `init()` 不能只绑定对象。

冻结语义：

```text
initialized == true
=
I2C 已绑定
+ WHO_AM_I 正确
+ 已退出 Sleep
+ 首版固定配置全部写入成功
+ read() 可合法调用
```

初始化顺序：

```text
validate pointers / address / current state
 -> require initialized platform_i2c_t
 -> read WHO_AM_I (0x75)
 -> require value == 0x68
 -> write PWR_MGMT_1  (0x6B) = 0x01
 -> write CONFIG      (0x1A) = 0x03
 -> write SMPLRT_DIV  (0x19) = 0x04
 -> write GYRO_CONFIG (0x1B) = 0x00
 -> write ACCEL_CONFIG(0x1C) = 0x00
 -> atomically commit i2c / address / initialized
```

`PWR_MGMT_1 = 0x01` 表示：

```text
SLEEP = 0
CLKSEL = 1 -> PLL with X-axis gyroscope reference
```

原始 Register Map 没有规定普通 Sleep -> Awake 后必须插入固定 100 ms 延时，因此 Driver 不增加无依据的 wake delay。手册中的 100 ms 等待属于 reset sequence，不等价于普通唤醒。

如果未来依据 Product Specification 引入明确 startup / PLL settling 要求，再单独修改合同。

初始化采用提交式语义：任一步失败时对象不得进入 `initialized=true` 的半初始化状态，并允许调用者修复原因后重试。

---

# 8. WHO_AM_I 与地址语义

关键规则：

```text
I2C address : 0x68 or 0x69, determined by AD0
WHO_AM_I    : fixed expected value 0x68
```

原始寄存器手册明确：`WHO_AM_I` 不反映 AD0 的最低地址位。因此禁止：

```c
if (whoAmI != address)
```

正确判断为固定设备 ID：

```c
if (whoAmI != MPU6050_WHO_AM_I_VALUE)
{
    return PLATFORM_ERR_NOT_FOUND;
}
```

其中：

```c
#define MPU6050_WHO_AM_I_VALUE (0x68U)
```

---

# 9. 首版固定配置

首版配置：

| Register | Address | Value | Meaning |
| --- | ---: | ---: | --- |
| WHO_AM_I | `0x75` | read `0x68` | identity |
| PWR_MGMT_1 | `0x6B` | `0x01` | wake + X gyro PLL |
| CONFIG | `0x1A` | `0x03` | DLPF_CFG=3 |
| SMPLRT_DIV | `0x19` | `0x04` | ~200 Hz internal sample rate |
| GYRO_CONFIG | `0x1B` | `0x00` | ±250 dps |
| ACCEL_CONFIG | `0x1C` | `0x00` | ±2 g |

内部约 200 Hz 更新速率与产品读取周期不是同一概念。

当前产品统一采集 / 上报周期保持：

```text
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

即 APP / Acquisition 层未来约每 2 s 获取一次当前快照。

---

# 10. read() 事务与解析

从 `ACCEL_XOUT_H = 0x3B` 一次连续读取 14 bytes：

```text
0..1   ACCEL_XOUT_H/L
2..3   ACCEL_YOUT_H/L
4..5   ACCEL_ZOUT_H/L
6..7   TEMP_OUT_H/L      // first version ignored
8..9   GYRO_XOUT_H/L
10..11 GYRO_YOUT_H/L
12..13 GYRO_ZOUT_H/L
```

总线事务：

```text
START
 -> address + W
 -> ACK
 -> 0x3B
 -> ACK
 -> Repeated START
 -> address + R
 -> ACK
 -> read 14 bytes
 -> ACK first 13 bytes
 -> NACK final byte
 -> STOP
```

禁止拆成 6 次独立轴读取。

每轴数据为高字节优先、16-bit two's complement：

```c
(int16_t)(((uint16_t)high << 8U) | (uint16_t)low)
```

固定首版换算：

```c
accel_g  = (float)accel_raw / 16384.0f;
gyro_dps = (float)gyro_raw  / 131.0f;
```

---

# 11. Atomic Output

`platform_mpu6050_read()` 必须使用本地 measurement 完成读取、解析和转换：

```text
I2C read
 -> parse raw
 -> convert physical units
 -> all success
 -> *measurement = localMeasurement
```

失败时调用者原有 `measurement` 保持不变，不允许留下部分更新结果。

该规则与 DHT20 当前 Platform Sensor 设计保持一致。

---

# 12. Error Semantics

使用现有 `platform_error_t`，不建立 MPU6050 私有错误码体系。

```text
NULL pointer              -> PLATFORM_ERR_NULL_POINTER
invalid 7-bit address     -> PLATFORM_ERR_INVALID_PARAM
repeated init             -> PLATFORM_ERR_ALREADY_INITIALIZED
I2C not initialized       -> PLATFORM_ERR_NOT_INITIALIZED
read before init          -> PLATFORM_ERR_NOT_INITIALIZED
I2C NACK / timeout / IO   -> preserve underlying platform_error_t
WHO_AM_I != 0x68          -> PLATFORM_ERR_NOT_FOUND
```

WHO_AM_I mismatch 表示目标地址上的设备不是期望的 MPU6050，因此使用现有 `PLATFORM_ERR_NOT_FOUND`。

---

# 13. deinit() 合同

`platform_mpu6050_deinit()` 只释放本对象生命周期状态：

```text
i2c         -> NULL
address     -> 0
initialized -> false
```

不得关闭共享 I2C，不发送额外芯片 reset / sleep 命令。

---

# 14. Logging 边界

正式 Driver 不在 bit/byte 级软件 I2C 时序中打印日志。

RTT / EasyLogger 仅用于目标板 smoke / 上层诊断观察：

```text
MPU6050 init result
WHO_AM_I value when useful
Accel raw / g summary
Gyro raw / dps summary
read / init error
```

日志不得改变软件 I2C 时序行为。

---

# 15. Target Verification

Phase 7 板测继续使用：

```text
RTT / EasyLogger
+
Logic Analyzer on PB6/PB7
```

## 15.1 初始化验证

RTT：

```text
WHO_AM_I = 0x68
init result = OK
```

逻辑分析仪至少确认：

```text
WHO_AM_I register read
PWR_MGMT_1 write
CONFIG write
SMPLRT_DIV write
GYRO_CONFIG write
ACCEL_CONFIG write
correct address / ACK / START / STOP / Repeated START
```

## 15.2 Burst Read 验证

逻辑分析仪必须确认：

```text
register pointer = 0x3B
Repeated START exists
14 bytes continuous read
ACK first 13 bytes
NACK final byte
STOP
```

RTT 输出六轴 raw / physical values。

## 15.3 Physical Sanity Check

静止平放：

```text
gyro axes near 0 dps, bias allowed
one accel axis near ±1 g according to board orientation
```

翻转 / 旋转：

```text
corresponding accel / gyro sign and magnitude change reasonably
```

这里只验证 byte order、signed conversion、range conversion 和基本物理响应，不验证姿态算法。

## 15.4 Negative Smoke

允许最小断连 / NACK 测试：

```text
disconnect MPU6050
 -> init/read returns NOT_FOUND or underlying bus error
 -> RTT reports one diagnostic result
 -> transaction exits normally
 -> Software I2C bus returns idle/recoverable state
```

不为异常注入建立复杂测试框架。

---

# 16. 临时 Smoke Harness 规则

允许创建最小临时 MPU6050 target smoke，仅用于 Phase 7 目标板验证。

不得借 smoke 提前实现：

```text
Acquisition Service
Final Acquisition Task
APP START / STOP / ONCE FSM
UART sensor report integration
```

验证通过后删除临时 task / source / Keil test group / include path，并恢复正常 production startup path。

---

# 17. Phase 7 完成门槛

Phase 7 只有同时满足以下条件才可标记 COMPLETED：

```text
platform_mpu6050.h/.c production implementation complete
shared I2C non-owning lifetime respected
WHO_AM_I semantics correct
fixed init config correct
14-byte burst read correct
signed raw parsing correct
raw -> g / dps conversion correct
failed read preserves caller output
no service_mpu6050 / impl_mpu6050 / private task / mutex
Keil production build PASS
no new MPU6050 production warning
existing Host regression PASS
RTT target smoke PASS
logic analyzer init sequence PASS
logic analyzer 14-byte burst PASS
physical static / rotate sanity PASS
temporary smoke removed
normal production rebuild PASS
architecture review PASS
```

完成后再更新 `handoff.md` 和 Phase 7 execution record；在完成前不得把 Phase 7 标记为 target verified。
