# DHT20 Phase 6 Implementation Plan

> 当前执行计划 / Current Active Plan  
> 状态：READY FOR CODEX EXECUTION  
> 日期：2026-09-04

**Goal:** 在已验证的 Platform GPIO + Software I2C 基线上，实现 DHT20 Platform 设备能力，并通过 Keil、RTT 与逻辑分析仪完成目标板验证。

**Frozen Spec:**

```text
00_Doc/02_架构设计/DHT20_Phase1设计.md
00_Doc/02_架构设计/DHT20参考文件/DHT20_软件I2C接入设计分析.md
00_Doc/02_架构设计/DHT20参考文件/DHT20产品规格书(中文版) A3-202409.md
00_Doc/00_项目需求/最终功能需求.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/handoff.md
```

---

# 1. Phase Scope

本次只实现：

```text
Platform DHT20
    ↓
Platform Software I2C
    ↓
Platform GPIO
```

不实现：

```text
DHT20 Service
Acquisition Service
MPU6050 production driver
Final Sensor Task
APP START / STOP / ONCE FSM
UART sensor report integration
new I2C mutex
Fake I2C / test-only abstraction
```

---

# 2. Production Files

新增：

```text
03_Platform/platform_bsp/dht20/platform_dht20.h
03_Platform/platform_bsp/dht20/platform_dht20.c
```

修改：

```text
00_Config/project_config.h
MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
```

其中 `project_config.h` 只增加产品级：

```c
#define PROJECT_ACQUISITION_PERIOD_MS (2000U)
```

DHT20 的地址、命令、80 ms、CRC 多项式等协议常量保留在 `platform_dht20.c` 私有范围。

---

# 3. Object and API

实现 caller-owned 轻量对象：

```c
typedef struct
{
    platform_i2c_t *i2c;
    platform_bool_t initialized;
} platform_dht20_t;
```

以及：

```c
typedef struct
{
    uint8_t status;
    uint32_t rawHumidity;
    uint32_t rawTemperature;
    float humidityPercent;
    float temperatureC;
} platform_dht20_measurement_t;
```

公共 API：

```text
platform_dht20_init()
platform_dht20_read()
platform_dht20_deinit()
```

DHT20 只引用共享 `platform_i2c_t`，不得拥有或关闭总线。

---

# 4. Init Implementation

`platform_dht20_init()`：

```text
validate dht20
validate i2c
reject repeated init
require initialized platform_i2c_t
bind i2c
initialized = true
```

不发送探测命令，不执行隐藏测量，不增加额外校准序列。

当前板级固定供电，调用前满足 DHT20 VDD 上电至少 5 ms 的规格书前置条件。

---

# 5. Read Implementation

固定 7-bit 地址：

```text
0x38
```

固定测量生命周期：

```text
validate
 -> platform_i2c_write(0x38, AC 33 00)
 -> STOP
 -> platform_time_delay_ms(80)
 -> platform_i2c_read(0x38, frame, 7)
 -> Busy check
 -> frame CRC8 check
 -> OTP CRC_flag check
 -> CalibrationEnable check
 -> parse 20-bit RH / T
 -> convert to float
 -> atomically commit measurement
```

不得使用 `platform_i2c_write_read()`。

第一版不做 Busy polling / retry loop。

---

# 6. Error Semantics

```text
I2C address NACK       -> preserve PLATFORM_ERR_NOT_FOUND
I2C transaction error -> preserve underlying platform_error_t
Busy == 1              -> PLATFORM_ERR_BUSY
frame CRC mismatch     -> PLATFORM_ERR_CHECKSUM
OTP CRC_flag == 0      -> PLATFORM_ERR_CHECKSUM
CalibrationEnable == 0 -> PLATFORM_ERR_INVALID_STATE
```

不得新增 DHT20 私有错误码体系。

失败时不得修改调用者已有 measurement 内容。

---

# 7. CRC / Parsing / Conversion

CRC8：

```text
input frame[0..5]
init 0xFF
poly 0x31
compare frame[6]
```

Raw：

```c
rawHumidity = ((uint32_t)frame[1] << 12U)
            | ((uint32_t)frame[2] << 4U)
            | ((uint32_t)frame[3] >> 4U);

rawTemperature = (((uint32_t)frame[3] & 0x0FU) << 16U)
               | ((uint32_t)frame[4] << 8U)
               | (uint32_t)frame[5];
```

Conversion：

```c
humidityPercent = ((float)rawHumidity * 100.0f) / 1048576.0f;
temperatureC = ((float)rawTemperature * 200.0f) / 1048576.0f - 50.0f;
```

---

# 8. Keil Integration

将两个 DHT20 production source/header 正常加入工程，不创建临时永久 Test Group。

完成 normal production rebuild。

验收：

```text
0 compile errors
no new DHT20 production warnings
no architecture dependency violation
```

现有历史 warning 可记录但不得由本 Phase 无关修改扩大范围。

---

# 9. Temporary Target Smoke

允许创建最小临时 DHT20 smoke harness，仅用于目标板验证。

要求：

```text
reuse existing Soft I2C
initialize Platform DHT20
perform repeated DHT20 read
period approximately 2000 ms
observe via RTT
```

不要实现最终 Acquisition Service / APP FSM。

RTT 至少观察：

```text
DHT20 init result
status
raw RH / T
converted RH / T
read error when present
```

正常测试不逐 bit / 逐 ACK 刷日志。

---

# 10. Logic Analyzer Verification

连接：

```text
PB6 -> SCL
PB7 -> SDA
```

单次测量必须观察到：

```text
START
0x70 + ACK
0xAC + ACK
0x33 + ACK
0x00 + ACK
STOP

>= 80 ms

START
0x71 + ACK
7-byte read
ACK after first 6 bytes
NACK after final CRC byte
STOP
```

再观察连续约 2 s 周期下事务稳定。

确认：

```text
no random address/data NACK
no stuck-low bus
no malformed START/STOP
final byte NACK correct
```

---

# 11. Negative Smoke

可执行最小断连测试：

```text
disconnect DHT20
 -> read/probe returns PLATFORM_ERR_NOT_FOUND or corresponding bus error
 -> RTT reports failure
```

不为了制造 Busy / CRC / OTP 错误而增加测试框架。

---

# 12. Cleanup

目标板验证通过后，移除所有仅为 DHT20 Smoke 添加的：

```text
temporary task / startup hook
temporary test source
temporary Keil test group/include path
```

保留 production DHT20 source、2000 ms 产品配置和正式文档。

重新执行正常生产 Build。

---

# 13. Architecture Review

确认：

```text
Platform DHT20 -> Platform I2C only
no direct HAL dependency
no Service -> Impl
no APP -> Impl
no impl_dht20 passthrough
no service_dht20 empty wrapper
no platform_device_t
no malloc/free
no DHT20-owned mutex/task
no platform_i2c_deinit() from DHT20 deinit
```

---

# 14. Final Acceptance Checklist

```text
[ ] platform_dht20.h/.c created
[ ] lightweight object implemented
[ ] shared I2C non-owning reference respected
[ ] init/read/deinit implemented
[ ] 7-bit 0x38 used
[ ] AC 33 00 transaction correct
[ ] STOP + 80 ms + new read transaction correct
[ ] Busy check correct
[ ] frame CRC8 correct
[ ] OTP CRC_flag correct
[ ] CalibrationEnable correct
[ ] raw RH/T parsing correct
[ ] float conversion correct
[ ] failed read leaves output unchanged
[ ] PROJECT_ACQUISITION_PERIOD_MS = 2000U
[ ] Keil production build passes
[ ] no new DHT20 production warning
[ ] RTT target smoke passes
[ ] logic analyzer single transaction passes
[ ] continuous ~2 s target read passes
[ ] optional disconnect error observed
[ ] temporary smoke removed
[ ] normal-path rebuild passes after cleanup
[ ] architecture/coding review passes
[ ] no Phase 7 / final APP scope creep
```

---

# 15. Stop Point

Phase 6 完成后停止。

不要自动继续实现：

```text
Phase 7 MPU6050 production module
Acquisition Service
Final Sensor Task
APP Control FSM
final UART sensor reporting
```

先更新验证记录与 `handoff.md`，再进入下一阶段设计讨论。
