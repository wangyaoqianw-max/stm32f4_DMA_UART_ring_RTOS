# 工程长期记忆与交接说明

更新时间：2026-09-04

> 本文件是 AI Agent 与人工开发者恢复工程上下文时的长期入口。
> 详细业务需求以 `00_Doc/00_项目需求/最终功能需求.md` 为准。
> 架构合同以 `00_Doc/04_Agent/architecture.md` 为准。
> 阶段路线以 `00_Doc/04_Agent/development_roadmap.md` 为准。
> 当前施工记录以 `00_Doc/04_Agent/implementation_plan.md` 为准。

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

2026-09-04 临时硬件连通性板测确认：

```text
DHT20  7-bit 0x38 -> ACK / usable
MPU6050 shared Software I2C -> usable
RTT observation -> PASS
```

该临时板测仅用于排除硬件风险，不代表 DHT20/MPU6050 production driver 已完成。

---

# 4. 已关闭阶段

```text
Phase 1  GPIO STM32 Impl                       COMPLETED
Phase 2  Board Resource + CubeMX              COMPLETED
Phase 3  Software I2C                         COMPLETED
Phase 4  LED Module                           COMPLETED
Phase 5  Button Module                        COMPLETED / HOST + KEIL + TARGET VERIFIED
```

Button 正式专项设计：

```text
00_Doc/02_架构设计/Button_Phase1设计.md
```

Button 冻结链：

```text
PA0 HIGH / LOW
    ↓
Platform GPIO
    ↓
Platform Button -> PRESSED / RELEASED
    ↓
Button Service -> SINGLE / DOUBLE / LONG
    ↓
Future APP -> START / SAMPLE_ONCE / STOP
```

临时 Button/Indicator smoke harness 已清理；正常生产启动路径已恢复。

---

# 5. Phase 6 — DHT20 实现状态

状态：

```text
COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

正式设计：

```text
00_Doc/02_架构设计/DHT20_Phase1设计.md
```

当前执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
DHT20 Phase 6 Implementation Plan
Status: COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

正式能力链：

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

第一版 DHT20 位于：

```text
03_Platform/platform_bsp/dht20/
├── platform_dht20.h
└── platform_dht20.c
```

不建立：

```text
service_dht20
impl_dht20
platform_device_t
registry / manager
malloc/free
DHT20 private task
DHT20-owned mutex
Fake I2C / test-only Ops abstraction
```

---

# 6. DHT20 对象 / API 合同

对象：

```text
platform_dht20_t
- platform_i2c_t *i2c
- initialized
```

DHT20 只引用共享 `platform_i2c_t`，不拥有总线生命周期；`platform_dht20_deinit()` 绝不能调用 `platform_i2c_deinit()`，因为 MPU6050 共用该总线。

一次采集数据：

```text
status
rawHumidity
rawTemperature
humidityPercent
 temperatureC
```

公共 API：

```text
platform_dht20_init()
platform_dht20_read()
platform_dht20_deinit()
```

`init()` 只检查并绑定已初始化 I2C，不发送隐藏探测/测量事务。

---

# 7. DHT20 协议合同

Platform I2C 使用 7-bit 地址：

```text
DHT20 address = 0x38
```

不得把规格书 8-bit 地址 `0x70 / 0x71` 直接传给 Platform I2C。

单次读取：

```text
platform_i2c_write(0x38, AC 33 00)
 -> STOP
 -> platform_time_delay_ms(80)
 -> platform_i2c_read(0x38, frame, 7)
 -> Busy
 -> frame CRC8
 -> OTP CRC_flag
 -> CalibrationEnable
 -> raw parse
 -> float conversion
 -> atomic measurement commit
```

不得用 `platform_i2c_write_read()` 代替，因为 DHT20 测量命令要求 STOP + >=80 ms + 新 START。

错误语义：

```text
address NACK             -> PLATFORM_ERR_NOT_FOUND
I2C transaction error    -> preserve underlying error
Busy                     -> PLATFORM_ERR_BUSY
frame CRC mismatch       -> PLATFORM_ERR_CHECKSUM
OTP CRC_flag == 0        -> PLATFORM_ERR_CHECKSUM
CalibrationEnable == 0   -> PLATFORM_ERR_INVALID_STATE
```

失败时不得修改调用者已有 measurement。

---

# 8. 产品采集周期更新

最终第一阶段统一采集 / 上报周期由 5 s 调整为：

```text
2000 ms / 2 s
```

生产配置名称：

```text
PROJECT_ACQUISITION_PERIOD_MS = 2000U
```

该值是产品级静态配置，不是 DHT20 协议常量。

DHT20 规格书也建议约每 2 秒测量一次，以降低频繁激活导致的自热影响。

Phase 6 只加入该配置，不提前实现最终 Acquisition Task。

---

# 9. Phase 6 验证合同

不建立额外 Fake I2C 测试框架，直接验证真实完整链路。

验证组合：

```text
Keil production build
+
RTT / EasyLogger
+
Logic Analyzer on PB6/PB7
```

RTT 观察：

```text
DHT20 init result
status
raw RH / T
converted RH / T
read errors
```

逻辑分析仪确认：

```text
START
0x70 + ACK
AC + ACK
33 + ACK
00 + ACK
STOP

>= 80 ms

START
0x71 + ACK
7-byte read
ACK first 6 bytes
NACK final CRC byte
STOP
```

再验证约 2 s 连续采集下事务稳定。

临时 DHT20 smoke harness 已在目标板验证完成后清理，production 启动路径已恢复。

当前自动验证记录：

```text
Host regression                    27 / 27 PASS
Keil temporary Smoke compile       0 errors / 20 historical warnings
Keil final production rebuild      0 errors / 20 historical warnings
Keil attached target Smoke rebuild 0 errors / 20 historical warnings
DHT20 production / Smoke warning   0 new warnings
Temporary Keil group/include path  REMOVED
Normal startup path                RESTORED
```

当前实现文件：

```text
03_Platform/platform_bsp/dht20/platform_dht20.h
03_Platform/platform_bsp/dht20/platform_dht20.c
Tests/platform_dht20/test_platform_dht20.c
```

目标板验证记录：RTT 初始化与连续读取结果均为 0，状态为 `0x18`，温湿度数据连续合理；逻辑分析仪确认 `AC 33 00`、约 80 ms 等待、7-byte read 最后一字节 NACK，以及约 2 s 周期。Target PASS。

---

# 10. 当前 Active Phase / 下一步

```text
Phase 6 — DHT20 Environment Module
STATUS: COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

下一步：

```text
Phase 6 closed
    ↓
Next session starts from Phase 7 design/plan when explicitly requested
```

本次已关闭 Phase 6，未实现 Phase 7。

---

# 11. 后续路线

```text
Phase 6  DHT20 Environment Module       COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 7  MPU6050 Motion Module          NEXT AFTER PHASE 6
Phase 8  UART Application Communication
Phase 9  RTOS Task / Event Design
Phase 10 Final APP Integration
Final Integrated Board Test
```

后续计划建立统一 Acquisition Service 调用 DHT20 + MPU6050；不为单个 DHT20 建立空转发 Service。

当前暂缓：

```text
SPI / LCD / GUI
W25Q64
AT24C02
Bluetooth
Roll / Pitch / Yaw
DMP
Kalman / Complementary Filter
复杂二进制 UART Protocol
Button EXTI / low-power wake
无需求驱动的框架扩展
```
