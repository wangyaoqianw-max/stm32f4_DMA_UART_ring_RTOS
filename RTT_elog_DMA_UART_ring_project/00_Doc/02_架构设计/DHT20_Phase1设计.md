# DHT20 Phase 6 设计

> 状态：DESIGN FROZEN / READY FOR IMPLEMENTATION  
> 日期：2026-09-04  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 目标

Phase 6 只实现 DHT20 的 Platform 设备能力，复用已经完成并实板验证的软件 I2C，不建立 DHT20 Service，不提前实现最终 Acquisition Service / APP Control FSM。

正式链：

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
            ↓
        PB6 / PB7
```

DHT20 与 MPU6050 共用同一 `platform_i2c_t`。

---

# 2. 已验证硬件基线

2026-09-04 已完成目标板连通性冒烟测试：

```text
DHT20   7-bit address 0x38   PASS
MPU6050 shared Soft I2C      PASS
PB6 SCL / PB7 SDA            PASS
Software I2C                 PASS
RTT observation              PASS
```

因此 Phase 6 不重新设计 Software I2C。

---

# 3. 模块位置与边界

新增：

```text
03_Platform/platform_bsp/dht20/
├── platform_dht20.h
└── platform_dht20.c
```

不新增：

```text
service_dht20
impl_dht20
platform_device_t
registry / manager
malloc / free
DHT20 private task
I2C mutex
Fake I2C / test-only Ops abstraction
```

DHT20 负责：

```text
I2C device command
measurement lifecycle
status validation
CRC8
raw parsing
raw -> physical conversion
```

DHT20 不负责：

```text
START / STOP / ONCE
2 s scheduling
UART report
LED behavior
APP state
RTOS task ownership
shared-I2C synchronization policy
```

---

# 4. 对象与数据模型

## 4.1 Device Context

```c
typedef struct
{
    platform_i2c_t *i2c;
    platform_bool_t initialized;
} platform_dht20_t;

#define PLATFORM_DHT20_INITIALIZER {0}
```

`platform_dht20_t` 只引用共享 I2C，不拥有其生命周期。

`platform_dht20_deinit()` 不得调用 `platform_i2c_deinit()`。

## 4.2 Measurement Data

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

对象上下文与一次采集数据分离。

---

# 5. 公共 API

第一版只提供：

```c
platform_error_t platform_dht20_init(
    platform_dht20_t *dht20,
    platform_i2c_t *i2c);

platform_error_t platform_dht20_read(
    platform_dht20_t *dht20,
    platform_dht20_measurement_t *measurement);

platform_error_t platform_dht20_deinit(
    platform_dht20_t *dht20);
```

---

# 6. 初始化合同

DHT20 为出厂标定器件。当前规格书读取流程只要求 VDD 上电后至少等待 5 ms 再进行通信，不要求 MCU 执行额外校准初始化序列。

因此 `platform_dht20_init()` 只负责：

```text
parameter validation
already-initialized validation
I2C initialized validation
bind shared I2C
initialized = true
```

不主动触发测量，不隐藏额外 I2C 流量。

当前板级 DHT20 固定供电；到达 Platform DHT20 初始化时已经满足上电 5 ms 前置条件。

---

# 7. 地址与协议常量

Platform I2C 公共 API 使用 7-bit 地址，因此 DHT20 固定传：

```text
0x38
```

不得传规格书中的 8-bit 地址字节 `0x70 / 0x71`。

协议常量属于 DHT20 私有实现常量，不放入 `project_config.h`：

```text
address          = 0x38
measure command  = AC 33 00
frame length     = 7 bytes
measurement wait = 80 ms
CRC init         = 0xFF
CRC polynomial   = 0x31
```

---

# 8. 单次同步测量生命周期

`platform_dht20_read()` 为 blocking synchronous API：

```text
validate
  ↓
platform_i2c_write(0x38, AC 33 00)
  ↓
STOP
  ↓
platform_time_delay_ms(80)
  ↓
platform_i2c_read(0x38, frame, 7)
  ↓
Busy check
  ↓
frame CRC8 check
  ↓
OTP CRC_flag check
  ↓
CalibrationEnable check
  ↓
20-bit raw parse
  ↓
float conversion
  ↓
commit output measurement
```

不得使用 `platform_i2c_write_read()`，因为测量命令结束后要求 STOP、等待至少 80 ms，然后重新 START 读取。

第一版不做 Busy polling / retry loop。80 ms 后读到 Busy=1 时返回 `PLATFORM_ERR_BUSY`。

---

# 9. 状态与错误语义

状态位：

```text
Bit7 Busy
    1 -> measuring
    0 -> idle

Bit4 CRC_flag
    1 -> OTP integrity pass
    0 -> OTP integrity failure

Bit3 CalibrationEnable
    1 -> calibrated output
    0 -> raw ADC output
```

错误映射：

```text
underlying I2C address NACK -> PLATFORM_ERR_NOT_FOUND
underlying I2C transaction -> preserve underlying platform_error_t
Busy == 1                  -> PLATFORM_ERR_BUSY
frame CRC mismatch         -> PLATFORM_ERR_CHECKSUM
OTP CRC_flag == 0          -> PLATFORM_ERR_CHECKSUM
CalibrationEnable == 0     -> PLATFORM_ERR_INVALID_STATE
```

第一版不增加 DHT20 私有错误枚举。

---

# 10. CRC 与解析

CRC8：

```text
input = frame[0..5]
init  = 0xFF
poly  = 0x31
expect == frame[6]
```

CRC helper 保持 `static` 私有。

Raw parsing：

```c
rawHumidity = ((uint32_t)frame[1] << 12U)
            | ((uint32_t)frame[2] << 4U)
            | ((uint32_t)frame[3] >> 4U);

rawTemperature = (((uint32_t)frame[3] & 0x0FU) << 16U)
               | ((uint32_t)frame[4] << 8U)
               | (uint32_t)frame[5];
```

转换：

```c
humidityPercent = ((float)rawHumidity * 100.0f) / 1048576.0f;
temperatureC = ((float)rawTemperature * 200.0f) / 1048576.0f - 50.0f;
```

STM32F411 Cortex-M4F 第一版直接使用 `float`。

---

# 11. 输出原子性

`platform_dht20_read()` 内部使用局部 measurement 临时对象。

只有全部协议、状态、CRC 与解析成功后才：

```c
*measurement = localMeasurement;
```

因此：

> 只有返回 `PLATFORM_ERR_OK` 时才更新调用者 measurement；失败时调用者原数据保持不变。

---

# 12. 共享 I2C 与并发

Phase 6 不在 DHT20 内增加 mutex。

当前方向：后续单一 Acquisition Task 串行访问 DHT20 与 MPU6050。

若未来产生多个真实 I2C 访问上下文，再在共享总线事务边界增加同步，而不是让每个设备驱动各自私有加锁。

---

# 13. 产品采集周期

产品级采集/上报周期由原 5 s 调整为：

```text
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

该值属于 `00_Config/project_config.h` 的产品静态配置，不属于 DHT20 协议常量。

规格书也建议约每 2 秒测量一次，以降低频繁激活造成的自热影响。

Phase 6 只加入配置项，不实现最终周期 Acquisition Task。

---

# 14. 验证策略

不建立 Fake I2C / Host protocol harness；直接验证完整真实链路。

## 14.1 编译 / 静态检查

```text
Keil normal production build
architecture dependency review
coding standard review
no new raw HAL dependency in DHT20
```

## 14.2 RTT

目标板观察：

```text
DHT20 init result
DHT20 read result
status
raw humidity / temperature
converted RH / T
error code on failure
```

正常运行不打印逐 bit / 逐 ACK 日志。

## 14.3 逻辑分析仪

PB6=SCL，PB7=SDA。

确认单次完整测量：

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
read 7 bytes
ACK first 6 bytes
NACK final CRC byte
STOP
```

再确认连续 2 s 周期下重复事务稳定，无异常拉低、随机 NACK 或错误时序。

---

# 15. Phase 6 完成标准

```text
[ ] platform_dht20.h/.c implemented
[ ] shared I2C ownership respected
[ ] init/read/deinit lifecycle correct
[ ] 0x38 7-bit address correct
[ ] AC 33 00 + STOP + 80 ms + read transaction correct
[ ] Busy / frame CRC / OTP CRC / calibration checks correct
[ ] raw parse / float conversion correct
[ ] failed read does not modify output measurement
[ ] PROJECT_ACQUISITION_PERIOD_MS = 2000U added
[ ] Keil build passes
[ ] RTT target observation passes
[ ] logic analyzer protocol observation passes
[ ] continuous repeated target read passes
[ ] no Phase 7 / final APP implementation introduced
```
