# Software I2C Phase 1 设计

> 文档类型：专项架构 / 模块设计  
> 状态：FROZEN FOR IMPLEMENTATION  
> 日期：2026-09-02  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 设计目的

本设计用于冻结 Phase 3 — Software I2C 的第一阶段实现边界。

本阶段目标不是构建完整 I2C Framework，而是在现有 Platform GPIO + STM32 GPIO Impl 基线上，实现一套轻量、同步、Master-only 的软件 I2C 基础总线能力，为后续 DHT20 和 MPU6050 模块提供统一通信接口。

依赖关系：

```text
DHT20 / MPU6050   (later phases)
        ↓
Platform I2C
        ↓
Platform GPIO + platform_delay_us()
        ↓
STM32 GPIO Impl + Cortex-M4 DWT
        ↓
HAL / CMSIS / Hardware
```

Software I2C 不直接调用 `HAL_GPIO_xxx()`，不直接操作具体 Port / Pin，不包含 DHT20 / MPU6050 设备语义。

---

# 2. Phase 3 范围

第一阶段只实现：

```text
Master only
7-bit address
Standard-mode oriented timing
START
Repeated START
STOP
ACK / NACK
Write Byte
Read Byte
Multi-byte Write
Multi-byte Read
Write + Repeated START + Read
Basic SCL-high wait / timeout
Init-time bus recovery
Synchronous transaction API
```

第一阶段明确不实现：

```text
Slave mode
10-bit address
Multi-master
Arbitration
Interrupt-driven I2C
DMA
Asynchronous transaction
Dynamic bus frequency switching
Driver registry
Backend registry
Dynamic allocation
Per-byte / per-bit RTT logging
Sensor register / command semantics
```

---

# 3. 模块位置与命名

Software I2C 属于 Platform 层 MCU 基础通信能力，文件计划位于：

```text
03_Platform/platform_mcu/i2c/
├── platform_i2c.h
├── platform_i2c.c
└── platform_i2c_types.h      // 仅在类型确有拆分价值时创建
```

对上层公共命名使用 `platform_i2c_*`，不使用 `platform_soft_i2c_*`。

原因：DHT20 / MPU6050 依赖的是 I2C 总线能力，而不是“GPIO 模拟”这一实现细节。当前 `platform_i2c.c` 第一版直接实现 Software I2C，不额外引入 ops/backend/registry 框架。

---

# 4. Software I2C 对象模型

第一版对象保持轻量：

```c
typedef struct
{
    const char *name;
    platform_gpio_t *scl;
    platform_gpio_t *sda;
    platform_bool_t initialized;
} platform_i2c_t;
```

对象只保存运行期必须状态：

```text
name
SCL Platform GPIO pointer
SDA Platform GPIO pointer
initialized
```

以下属于静态配置，不进入对象 Context：

```text
I2C half-period delay
SCL high timeout
```

以下不在第一版对象中保存：

```text
frequency runtime state
mutex
busy flag
slave list
transaction queue
DMA / IRQ context
sensor state
```

SCL / SDA `platform_gpio_t` 存储由调用者拥有；I2C 对象只引用，不拥有其内存。

---

# 5. 公共 API

第一版公共 API 冻结为 transaction 级接口：

```c
platform_error_t platform_i2c_init(
    platform_i2c_t *i2c,
    const char *name,
    platform_gpio_t *scl,
    platform_gpio_t *sda);

platform_error_t platform_i2c_write(
    platform_i2c_t *i2c,
    uint8_t address,
    const uint8_t *data,
    uint16_t length);

platform_error_t platform_i2c_read(
    platform_i2c_t *i2c,
    uint8_t address,
    uint8_t *data,
    uint16_t length);

platform_error_t platform_i2c_write_read(
    platform_i2c_t *i2c,
    uint8_t address,
    const uint8_t *tx_data,
    uint16_t tx_length,
    uint8_t *rx_data,
    uint16_t rx_length);

platform_error_t platform_i2c_deinit(platform_i2c_t *i2c);
```

约束：

```text
address = 7-bit slave address
write/read length > 0
write_read tx_length > 0 && rx_length > 0
NULL / zero-length / address > 0x7F -> invalid parameter
```

不增加 `mem_read / mem_write`。寄存器语义属于具体设备驱动；MPU6050 后续可在设备模块内部基于 `write()` / `write_read()` 封装 register access。

---

# 6. Private Protocol Primitives

以下只作为 `platform_i2c.c` 内部静态能力，不暴露给 Sensor / APP：

```text
i2c_start()
i2c_stop()
i2c_write_bit()
i2c_read_bit()
i2c_write_byte()
i2c_read_byte()
i2c_wait_ack()
i2c_send_ack()
i2c_send_nack()
i2c_wait_scl_high()
i2c_bus_recover()
```

上层不得自行拼接 START / byte / ACK / STOP。

一次 public transaction 调用必须形成完整 I2C transaction；成功返回时总线应回到 Idle。

---

# 7. GPIO 电气模型

Phase 2 已冻结：

```text
SCL -> PB6
SDA -> PB7
GPIO Output Open-Drain
GPIO_NOPULL
initial HIGH
external pull-up present
```

Software I2C 初始化后 SCL / SDA 始终保持 Open-Drain Output，不在 transaction 中动态切换 Input / Output。

逻辑语义：

```text
write LOW  -> MCU actively pulls line low
write HIGH -> MCU releases open-drain output
read       -> read actual physical line level
```

内部语义优先使用：

```text
SDA_LOW
SDA_RELEASE
SDA_READ
SCL_LOW
SCL_RELEASE
SCL_READ
```

不使用“主动驱动 HIGH”的语义。

读取 ACK / 数据前：

```text
release SDA
then read physical SDA level
```

---

# 8. 微秒延时方案

现有 `03_Platform/platform_common/platform_def.h` 已预留：

```c
void platform_delay_ms(uint32_t ms);
void platform_delay_us(uint32_t us);
```

本 Phase 不新增 `platform_time_delay_us()` 或新的 Delay Platform 模块。

Software I2C 直接复用：

```c
platform_delay_us(...);
```

`platform_delay_us()` 具体实现新增在 Impl MCU，建议文件：

```text
04_Impl/impl_mcu/impl_platform_delay.c
```

实现策略冻结为：

```text
Cortex-M4 DWT CYCCNT
busy-wait
lazy initialization
```

DWT / CoreDebug / CMSIS 细节不得泄漏到 Platform I2C。

`platform_delay_us()` 用于短微秒级精确等待，不作为长时间任务延时接口。

现有：

```text
platform_time_delay_ms()
```

继续表示 RTOS Task Context 的毫秒级调度延时，两者底层语义不同。

旧预留 `platform_delay_ms()` 不属于本 Phase 必须实现范围，不得为了 Software I2C 顺手引入第二套毫秒延时实现。

---

# 9. 静态时序配置

Software I2C 第一阶段只有一条总线，DHT20 与 MPU6050 共用，因此时序参数不进入 `platform_i2c_t`。

在：

```text
00_Config/project_config.h
```

新增项目级静态配置：

```c
#define PROJECT_SOFT_I2C_HALF_PERIOD_US    (5U)
#define PROJECT_SOFT_I2C_SCL_TIMEOUT_US    (100U)
```

语义：

```text
HALF_PERIOD_US
    -> 每个主要时序阶段主动等待的最小微秒数

SCL_TIMEOUT_US
    -> release SCL 后等待实际 SCL 变 HIGH 的最大时间
```

不定义“精确 100 kHz”宏，因为 Software I2C 实际频率还包含 Platform GPIO、函数调用和 HAL 执行开销。

第一阶段目标：

```text
Standard-mode oriented
nominal ~100 kHz
protocol correctness > exact clock frequency
```

协议固定规则不放入 `project_config.h`：

```text
7-bit addressing
MSB first
9 recovery clocks maximum
last read byte uses NACK
```

---

# 10. Bit / Byte 时序

正常数据位：SDA 只在 SCL LOW 时改变；START / STOP 是例外。

## 10.1 START

```text
SDA RELEASE
SCL RELEASE
wait SCL actual HIGH
short delay
SDA LOW          <- while SCL HIGH
short delay
SCL LOW
```

定义：SCL HIGH 时 SDA HIGH -> LOW。

## 10.2 STOP

```text
SDA LOW
SCL RELEASE
wait SCL actual HIGH
short delay
SDA RELEASE      <- while SCL HIGH
short delay
```

定义：SCL HIGH 时 SDA LOW -> HIGH。

## 10.3 Repeated START

在不发送 STOP 的情况下重新释放 SDA/SCL，并再次制造 SCL HIGH 时 SDA HIGH -> LOW。

内部实现允许复用 START primitive，但不得假设 START 调用前一定处于完整 Idle transaction 结束状态。

## 10.4 Write Bit / Byte

数据发送顺序：

```text
MSB first
bit7 -> bit0
```

每个 bit：

```text
SCL LOW
set SDA: 0 -> LOW / 1 -> RELEASE
short delay
SCL RELEASE
wait SCL actual HIGH
short delay
SCL LOW
short delay
```

## 10.5 ACK / NACK

发送 8 bit 后，第 9 个 clock：

```text
Master RELEASE SDA
SCL RELEASE
read SDA

LOW  -> ACK
HIGH -> NACK

SCL LOW
```

读取多个 byte 时：

```text
intermediate byte -> Master sends ACK
last byte         -> Master sends NACK
```

---

# 11. SCL High Wait / Clock Stretch 基线

第一版不构建复杂 Clock Stretching Framework，但每次需要 SCL HIGH 时不得假设“写 HIGH 即实际 HIGH”。

统一行为：

```text
SCL RELEASE
    ↓
read actual SCL
    ↓
HIGH -> continue
LOW  -> wait in short us steps
    ↓
timeout -> PLATFORM_ERR_TIMEOUT
```

这样同时兼容：

```text
open-drain physical semantics
basic slave clock stretching
stuck-low detection
```

---

# 12. Transaction 语义

## 12.1 Write

```text
START
Address + W
ACK
Data[0]
ACK
...
Data[n-1]
ACK
STOP
```

地址字节由 I2C 层生成：

```c
(address << 1) | 0U
```

## 12.2 Read

```text
START
Address + R
ACK
Read byte 0
Master ACK
...
Read last byte
Master NACK
STOP
```

地址字节：

```c
(address << 1) | 1U
```

## 12.3 Write + Read

```text
START
Address + W
TX bytes
Repeated START
Address + R
RX bytes
STOP
```

用于后续 MPU6050 register-oriented read 等场景，但 I2C 层本身不知道寄存器语义。

---

# 13. 错误处理

复用现有 `platform_error_t`，第一版不新增 I2C 专用错误枚举。

建议映射：

```text
invalid address / length / pointer
    -> PLATFORM_ERR_INVALID_PARAM

not initialized
    -> PLATFORM_ERR_NOT_INITIALIZED

address NACK
    -> PLATFORM_ERR_NOT_FOUND

data-stage NACK / protocol I/O failure
    -> PLATFORM_ERR_IO

SCL stuck LOW / SCL high wait timeout
    -> PLATFORM_ERR_TIMEOUT

bus not idle in normal transaction
    -> PLATFORM_ERR_BUSY

Platform GPIO lower-layer error
    -> propagate original error where practical
```

transaction 已开始后发生错误时：

```text
preserve original error
    ↓
best-effort STOP / bus release
    ↓
return original error
```

不得因为 cleanup 失败覆盖最初导致 transaction 失败的主要错误。

---

# 14. Init / Deinit / Bus Recovery

## 14.1 Init

`platform_i2c_init()`：

```text
validate arguments
store name / SCL / SDA
configure SCL as OD Output / No Pull / initial HIGH
configure SDA as OD Output / No Pull / initial HIGH
release SDA
release SCL
wait/check actual bus state
```

正常 Idle：

```text
SCL HIGH
SDA HIGH
```

若：

```text
SCL HIGH
SDA LOW
```

初始化阶段允许执行一次 bus recovery。

若 SCL 无法释放为 HIGH，返回 timeout / busy，不继续 transaction。

## 14.2 Bus Recovery

初始化阶段恢复策略：

```text
release SDA
up to 9 SCL pulses
    SCL LOW
    delay
    SCL RELEASE
    wait actual HIGH
    delay
    read SDA
    if SDA HIGH -> may stop early
finally generate STOP
verify Idle
```

9 个恢复 pulse 属于协议恢复固定规则，不做 project config。

正常 transaction 阶段若发现总线非 Idle：

```text
return BUSY / TIMEOUT
```

不在每次 transaction 中自动反复执行 recovery，避免掩盖真实硬件问题。

## 14.3 Deinit

```text
release SDA
release SCL
platform_gpio_deinit(SDA)
platform_gpio_deinit(SCL)
initialized = FALSE
```

I2C 不释放 / 销毁调用者拥有的 GPIO 对象存储。

---

# 15. RTOS / 并发合同

第一阶段 Software I2C：

```text
synchronous
Task Context only
not ISR-safe
not internally thread-safe
no internal mutex
```

当前最终系统计划由单一采集路径顺序访问 DHT20 + MPU6050，因此 Phase 3 不提前加入 Mutex。

调用者负责保证 transaction 串行化。

若 Phase 9 最终 Task / Event 设计产生真实多任务共享总线需求，再专项评审 bus-level mutex；不得在 Phase 3 为假设需求提前扩展。

---

# 16. 日志策略

Software I2C Platform 内部正常路径禁止：

```text
per-bit log
per-byte log
per-ACK log
每次 GPIO 操作打 RTT
```

最终产品日志仍由 APP / Service 根据 I2C / Sensor 返回值记录关键状态。

Phase 3 目标板 Smoke Test 允许临时测试入口输出少量阶段标记与错误原因，验收完成后不得把高频测试日志留在正式运行路径。

---

# 17. 验证策略

Phase 3 使用四层验证，其中目标板验收重点采用用户确认的三路观察。

## 17.1 Host Test

使用 Fake：

```text
platform_gpio_configure
platform_gpio_write
platform_gpio_read
platform_gpio_deinit
platform_delay_us
```

记录 GPIO interaction sequence，并可预置 SDA / SCL read sequence 模拟 ACK、NACK、RX bit、SCL stuck-low。

至少覆盖：

```text
init parameter validation
OD GPIO configuration
Idle release
init-time bus recovery
bus recovery failure
7-bit address shift
Write R/W bit
Read R/W bit
MSB-first byte output
ACK / NACK handling
multi-byte read: intermediate ACK + final NACK
Repeated START
Address NACK
Data NACK
SCL timeout
best-effort STOP on failure
```

Host Test 验证协议逻辑和 GPIO 操作顺序，不负责证明真实微秒时序。

## 17.2 Keil Build

必须 Full Rebuild：

```text
0 errors
no new warnings introduced by Phase 3 code
```

用于验证：

```text
DWT / CMSIS implementation integration
Platform / Impl dependency boundary
project_config integration
```

## 17.3 Target Board — Serial Assistant

临时 Phase 3 Smoke Harness 可通过现有 USART1 输出低频阶段标记，例如：

```text
I2C_SMOKE,START
I2C_SMOKE,INIT,PASS
I2C_SMOKE,TXRX,PASS
I2C_SMOKE,PASS
```

失败时输出明确阶段和 Platform Error：

```text
I2C_SMOKE,FAIL,<stage>,<error>
```

串口助手用于确认测试流程、transaction 成败和固件没有卡死。

## 17.4 Target Board — RTT / EasyLogger

RTT 观察：

```text
Software I2C init result
bus recovery occurrence / failure
address NACK / timeout / I/O failure
smoke transaction final result
```

不打印逐 bit / byte / ACK 正常日志。

RTT 与串口助手结果必须一致。

## 17.5 Target Board — Logic Analyzer

逻辑分析仪连接 PB6 / PB7，必须人工观察并优先使用 I2C protocol decoder 验证：

```text
Idle SCL/SDA HIGH
START
STOP
Repeated START
7-bit Address
R/W bit
ACK / NACK
TX data
RX data
SCL high / low time
实际总线频率在 Standard-mode 可接受范围
```

目标是验证真实电气时序与协议波形，不要求频率精确等于 100 kHz。

目标板可使用连接中的 DHT20 / MPU6050 做原始 transaction smoke verification；测试代码只发送 / 接收明确的原始 I2C byte sequence，不在 Phase 3 建立正式 Sensor Driver。

---

# 18. Phase 3 完成门槛

必须全部满足：

```text
Software I2C public contract frozen
Host Test PASS
Coding Standard Review PASS
Keil Full Rebuild PASS
DWT us delay target integration PASS
Serial Assistant smoke result PASS
RTT smoke result PASS
Logic Analyzer START / STOP PASS
Logic Analyzer Address / ACK PASS
Logic Analyzer Repeated START PASS
Logic Analyzer Read / Write transaction PASS
No Hardware I2C introduced
No HAL GPIO leakage into Platform I2C
No Sensor business logic introduced
No new internal mutex / async framework
```

只有真实目标板三路观察（串口助手 + RTT + 逻辑分析仪）完成后，才允许记录：

```text
Phase 3 — COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

---

# 19. 后续阶段边界

Phase 3 完成后停止继续扩展 I2C，进入 Roadmap：

```text
Phase 4 — LED Module
Phase 5 — Button Module
Phase 6 — DHT20 Environment Module
Phase 7 — MPU6050 Motion Module
```

DHT20 / MPU6050 设备地址、命令、寄存器、数据解析等内容只在对应 Sensor Phase 实现。
