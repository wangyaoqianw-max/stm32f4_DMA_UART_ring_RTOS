# MPU6050 Phase 7 Implementation Plan

> 当前执行计划 / Current Active Plan  
> 状态：READY FOR CODEX EXECUTION  
> 日期：2026-09-04

**Goal:** 在已验证的 Platform GPIO + Software I2C + DHT20 共享总线基线上，实现 MPU6050 Platform 设备能力，并通过 Host、Keil、RTT 与逻辑分析仪完成目标板验证。

**Frozen Spec:**

```text
00_Doc/02_架构设计/MPU6050_Phase1设计.md
00_Doc/02_架构设计/MPU6050参考文件/MPU6050寄存器英文版本.md
00_Doc/02_架构设计/MPU6050参考文件/MPU6050寄存器英文版本.pdf
00_Doc/02_架构设计/MPU6050参考文件/MPU6050数据手册_项目适用分析.md
00_Doc/04_Agent/handoff.md
00_Config/project_config.h
03_Platform/platform_mcu/i2c/platform_i2c.h
03_Platform/platform_bsp/dht20/platform_dht20.h/.c
```

原始寄存器手册优先于蒸馏摘要。当前产品统一采集周期为 `2000 ms`，不得继续使用旧文档中的 `5 s` 描述。

---

# 1. Phase Scope

本次只实现：

```text
Platform MPU6050
    ↓
Platform Software I2C
    ↓
Platform GPIO
```

不实现：

```text
service_mpu6050
impl_mpu6050
Acquisition Service
Final Acquisition Task
APP START / STOP / ONCE FSM
UART sensor report integration
platform_device_t / registry / manager
MPU6050 private task
MPU6050-owned mutex
FIFO / DMP / DATA_RDY interrupt
attitude algorithm
```

---

# 2. Production Files

新增：

```text
03_Platform/platform_bsp/mpu6050/platform_mpu6050.h
03_Platform/platform_bsp/mpu6050/platform_mpu6050.c
```

按现有工程组织把 production source/header 正常加入 Keil 工程。

除非实际装配需要，不为 MPU6050 新增多余配置框架。若使用产品地址宏，应由上层装配传入，Platform MPU6050 不直接依赖上层业务配置。

---

# 3. Object and API

实现 caller-owned 轻量对象：

```c
#define PLATFORM_MPU6050_INITIALIZER {0}

typedef struct
{
    platform_i2c_t *i2c;
    uint8_t address;
    platform_bool_t initialized;
} platform_mpu6050_t;
```

Measurement：

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

公共 API：

```text
platform_mpu6050_init()
platform_mpu6050_read()
platform_mpu6050_deinit()
```

MPU6050 只引用共享 `platform_i2c_t`，不得拥有或关闭总线。

---

# 4. init() Implementation

`platform_mpu6050_init()`：

```text
validate mpu6050 pointer
validate i2c pointer
validate 7-bit address
reject repeated init
require initialized platform_i2c_t
read WHO_AM_I (0x75)
require WHO_AM_I == 0x68
write PWR_MGMT_1  (0x6B) = 0x01
write CONFIG      (0x1A) = 0x03
write SMPLRT_DIV  (0x19) = 0x04
write GYRO_CONFIG (0x1B) = 0x00
write ACCEL_CONFIG(0x1C) = 0x00
atomically commit i2c/address/initialized
```

关键语义：

```text
initialized == true
=
identity verified
+ awake
+ first-version configuration applied
+ read() is legal
```

普通 Sleep -> Awake 不增加无原始手册依据的固定 100 ms 延时；不要把 reset sequence 的 100 ms 错套到普通唤醒。

任何初始化步骤失败时，对象不得进入半初始化状态。

---

# 5. Address / WHO_AM_I Semantics

```text
I2C address = 0x68 / 0x69 according to AD0
WHO_AM_I expected value = 0x68
```

禁止：

```c
whoAmI == address
```

WHO_AM_I 不反映 AD0 地址最低位。

不允许 0x68 NACK 后静默 fallback 到 0x69。

---

# 6. Read Implementation

从 `ACCEL_XOUT_H = 0x3B` 一次连续读 14 bytes：

```text
0..1   ACCEL_XOUT_H/L
2..3   ACCEL_YOUT_H/L
4..5   ACCEL_ZOUT_H/L
6..7   TEMP_OUT_H/L      // ignore in first public measurement
8..9   GYRO_XOUT_H/L
10..11 GYRO_YOUT_H/L
12..13 GYRO_ZOUT_H/L
```

使用：

```c
platform_i2c_write_read(i2c, address, &reg, 1U, raw, 14U);
```

必须形成：

```text
START -> address+W -> 0x3B
 -> Repeated START -> address+R
 -> 14-byte read
 -> ACK first 13
 -> NACK final byte
 -> STOP
```

不要拆成逐轴独立读取。

---

# 7. Parsing / Conversion

每轴为 high-byte-first signed 16-bit two's complement：

```c
(int16_t)(((uint16_t)high << 8U) | (uint16_t)low)
```

固定首版量程：

```text
Accel ±2 g
Gyro  ±250 dps
```

换算：

```c
accel_g  = (float)raw / 16384.0f;
gyro_dps = (float)raw / 131.0f;
```

MPU6050 内部温度首版不作为业务输出。

---

# 8. Atomic Output

`platform_mpu6050_read()` 必须使用本地 measurement 完成：

```text
I2C read
 -> raw parse
 -> physical conversion
 -> all success
 -> commit output
```

失败时调用者已有 measurement 保持不变。

---

# 9. Error Semantics

不得新增 MPU6050 私有错误码体系。

```text
NULL pointer            -> PLATFORM_ERR_NULL_POINTER
invalid address         -> PLATFORM_ERR_INVALID_PARAM
repeated init           -> PLATFORM_ERR_ALREADY_INITIALIZED
I2C not initialized     -> PLATFORM_ERR_NOT_INITIALIZED
read before init        -> PLATFORM_ERR_NOT_INITIALIZED
I2C transaction error   -> preserve underlying platform_error_t
WHO_AM_I != 0x68        -> PLATFORM_ERR_NOT_FOUND
```

---

# 10. deinit() Implementation

只清理 MPU6050 自身状态：

```text
i2c         = NULL
address     = 0
initialized = false
```

不得调用 `platform_i2c_deinit()`，不得额外 reset/sleep 芯片。

---

# 11. Host / Static Verification

优先沿用现有项目测试风格，新增最小 MPU6050 contract test；不要为了测试引入 production fake-I2C abstraction。

至少验证：

```text
argument validation
repeated init rejection
read-before-init rejection
WHO_AM_I mismatch semantics
successful init state commit
failed init leaves object uninitialized
signed raw parsing
raw -> g/dps conversion
failed read leaves output unchanged
deinit does not own I2C lifetime
```

若现有 Host 测试基础无法无侵入覆盖真实 I2C transaction，则不要为了“全 mock 覆盖率”重构 production API；协议事务以目标板 + 逻辑分析仪为主验收。

运行现有 Host regression，禁止引入回归。

---

# 12. Keil Integration

正常加入 MPU6050 production 文件并执行 production rebuild。

验收：

```text
0 compile errors
no new MPU6050 production warnings
no architecture dependency violation
```

历史 warning 可记录，但本 Phase 不扩大无关修改范围。

---

# 13. Temporary Target Smoke

允许创建最小临时 MPU6050 smoke harness，仅用于实板验证。

要求：

```text
reuse existing shared Software I2C
initialize Platform MPU6050
read repeated six-axis snapshots
observe via RTT / EasyLogger
```

可以按约 `2000 ms` 周期重复读取以便观察，但该 smoke 不等价于最终 Acquisition Task。

RTT 至少观察：

```text
WHO_AM_I when useful
MPU6050 init result
AX/AY/AZ raw + g
GX/GY/GZ raw + dps
read error when present
```

禁止逐 I2C bit / byte / ACK 高频日志。

---

# 14. Logic Analyzer Verification

连接：

```text
PB6 -> SCL
PB7 -> SDA
```

初始化至少确认：

```text
WHO_AM_I read
PWR_MGMT_1 write 0x01
CONFIG write 0x03
SMPLRT_DIV write 0x04
GYRO_CONFIG write 0x00
ACCEL_CONFIG write 0x00
correct START / STOP / ACK / Repeated START
```

单次采样必须确认：

```text
register pointer 0x3B
Repeated START
14-byte continuous read
ACK first 13 bytes
NACK final byte
STOP
```

不得出现随机 NACK、总线 stuck-low 或畸形 START/STOP。

---

# 15. Physical Sanity Check

静止平放：

```text
gyro three axes near 0 dps, bias allowed
one accel axis near ±1 g according to orientation
```

翻转 / 旋转：

```text
related accel / gyro axes change sign and magnitude reasonably
```

该测试用于验证字节序、有符号转换、量程换算和基本物理响应，不验证姿态解算。

---

# 16. Negative Smoke

允许最小断连/NACK测试：

```text
disconnect MPU6050
 -> init/read returns NOT_FOUND or underlying bus error
 -> RTT records one diagnostic failure
 -> no task deadlock
 -> transaction exits
 -> shared Software I2C remains recoverable
```

不为异常注入增加复杂 framework。

---

# 17. Cleanup

目标板验证完成后移除所有仅为 MPU6050 Smoke 添加的：

```text
temporary task / startup hook
temporary test source
temporary Keil test group/include path
```

恢复 normal production startup path，再执行正常生产 Build。

---

# 18. Architecture Review

确认：

```text
Platform MPU6050 -> Platform I2C only
no direct HAL dependency
no Service -> Impl
no APP -> Impl
no impl_mpu6050 passthrough
no service_mpu6050 empty wrapper
no platform_device_t
no malloc/free
no MPU6050-owned mutex/task
no platform_i2c_deinit() from MPU6050 deinit
no final APP / Acquisition scope creep
```

---

# 19. Final Acceptance Checklist

```text
[ ] platform_mpu6050.h/.c created
[ ] lightweight context implemented
[ ] shared I2C non-owning reference respected
[ ] init/read/deinit implemented
[ ] 7-bit address semantics correct
[ ] no silent address fallback
[ ] WHO_AM_I fixed expectation 0x68 correct
[ ] init performs wake + fixed configuration
[ ] partial init does not commit object state
[ ] 14-byte burst read correct
[ ] signed raw parsing correct
[ ] ±2 g / ±250 dps conversion correct
[ ] MPU internal temperature omitted from public measurement
[ ] failed read leaves output unchanged
[ ] no private MPU6050 error type
[ ] existing Host regression passes
[ ] MPU6050 contract tests pass where practical
[ ] Keil production build passes
[ ] no new MPU6050 production warning
[ ] RTT target smoke passes
[ ] logic analyzer init sequence passes
[ ] logic analyzer 14-byte burst passes
[ ] physical static / rotate sanity passes
[ ] negative smoke disposition recorded
[ ] temporary smoke removed
[ ] normal-path rebuild passes after cleanup
[ ] architecture review passes
[ ] handoff updated with real execution evidence
```

---

# 20. Stop Point

Phase 7 完成后停止。

不要自动继续实现：

```text
Phase 8 UART Application Communication
Acquisition Service
Final Acquisition Task
APP Control FSM
final UART sensor reporting
```

只有在 Host、Keil、RTT、逻辑分析仪和物理 sanity check 均有真实证据后，才把 Phase 7 标记为 `COMPLETED / TARGET VERIFIED` 并更新 `handoff.md`。
