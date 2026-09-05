# SPI Platform + STM32 Impl Phase 1 设计

> 状态：DESIGN FROZEN  
> 日期：2026-09-05  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`  
> 当前目标：为正式 ST7789T3 驱动建立可复用的 SPI Platform / Impl 基础设施。

---

# 1. 背景

Display Extension 已完成以下目标板验证：

```text
P169H002-CTP / ST7789T3
240 x 280
SPI1 Mode 3
SPI clock 12.5 MHz
blocking HAL_SPI_Transmit() viable
X_OFFSET = 0
Y_OFFSET = 20
RGB565 high-byte first
LCD backlight High = ON
BLACK / WHITE / RED / GREEN / BLUE PASS
```

临时 Bring-up 代码已经回退，只保留 CubeMX SPI1 / LCD GPIO 配置。

因此本阶段不再验证“LCD 能否点亮”，而是正式建立：

```text
Device Driver
   ↓
Platform SPI
   ↓
STM32 SPI Impl
   ↓
HAL / SPI1
```

后续 ST7789 驱动不得直接依赖 `HAL_SPI_Transmit()`、`SPI_HandleTypeDef` 或 `hspi1`。

---

# 2. 设计目标

本阶段目标：

```text
1. 建立可复用 SPI Bus / SPI Device 两层模型。
2. Platform 不暴露 HAL 类型。
3. STM32 Impl 负责绑定 SPI1 与 HAL。
4. 支持同步阻塞 TX。
5. 建立轻量 SPI transaction 边界。
6. 支持软件 CS，且允许设备无 CS。
7. 设备配置描述 mode / bit order / data bits / max clock。
8. Phase 1 对当前 CubeMX 固定 SPI 配置做严格校验，不做运行时动态切换。
9. 保持未来扩展 read / transfer / DMA / mutex / dynamic reconfiguration 的接口演进空间。
```

不做：

```text
ST7789 正式驱动
LCD 字体 / 绘图
Display Service / Task / Queue
UART 产品输出迁移
ONCE 语义调整
SPI DMA
SPI interrupt transfer
SPI read
full-duplex transfer
bus mutex
runtime mode / baudrate / data-size switching
复杂 multi-segment transaction descriptor
Touch / CTP
```

---

# 3. 分层边界

工程稳定合同：

```text
APP -> Service -> Platform -> Impl -> Vendor / HAL / Hardware
```

SPI 本阶段：

```text
future ST7789 driver
    ↓
platform_spi_device_t
    ↓
platform_spi_transaction_begin()
platform_spi_write()
platform_spi_transaction_end()
    ↓
platform_spi_bus_t::ops
    ↓
impl_platform_spi
    ↓
HAL_SPI_Transmit()
    ↓
hspi1
```

Platform SPI 禁止出现：

```text
SPI_HandleTypeDef
HAL_SPI_*
stm32f4xx_hal_spi.h
hspi1
SPI1 register details
APB clock implementation details
```

Impl 可以依赖：

```text
Core/Inc/spi.h
STM32 HAL SPI / RCC
hspi1
```

---

# 4. Bus / Device 模型

## 4.1 SPI Bus

`platform_spi_bus_t` 表示 MCU SPI Controller / Bus 资源。

它负责：

```text
Platform device lifecycle
implementation binding
bus operations
active transaction state
future bus-level synchronization point
```

建议结构语义：

```c
typedef struct platform_spi_bus platform_spi_bus_t;
typedef struct platform_spi_device platform_spi_device_t;

struct platform_spi_bus
{
    platform_device_t device;
    const platform_spi_bus_ops_t *ops;
    void *implContext;
    platform_spi_device_t *activeDevice;
};
```

说明：

- `implContext` 是 Impl 私有 type-erasure context，不是由 Platform 保存 `SPI_HandleTypeDef *`。
- Platform 不解释 `implContext` 的具体类型。
- `activeDevice` 仅是事务状态检测，不是 RTOS mutex。
- Phase 1 不维护设备链表，不动态注册设备。

若现有 `platform_device_t / lifecycle / initializer` 的准确命名与此示例不同，实施时必须沿用仓库现有 Platform common 风格，不新造平行对象体系。

## 4.2 SPI Device

`platform_spi_device_t` 表示挂在某条 SPI Bus 上的 SPI Slave 描述符。

它不是完整 Platform lifecycle device，不需要 CREATED / INITIALIZED / STARTED / STOPPED 全套状态。

建议语义：

```c
struct platform_spi_device
{
    const char *name;
    platform_spi_bus_t *bus;
    platform_gpio_t *cs;
    platform_gpio_level_t csActiveLevel;
    platform_spi_device_config_t config;
    platform_bool_t initialized;
};
```

所有引用均为 static non-owning reference：

```text
SPI Bus lifetime >= SPI Device lifetime
CS GPIO lifetime >= SPI Device lifetime
SPI Device lifetime >= upper device-driver usage lifetime
```

禁止 runtime malloc/free。

---

# 5. SPI Device 配置模型

冻结配置：

```c
typedef struct
{
    platform_spi_mode_t mode;
    platform_spi_bit_order_t bitOrder;
    uint8_t dataBits;
    uint32_t maxClockHz;
} platform_spi_device_config_t;
```

## 5.1 Mode

支持描述：

```text
MODE 0
MODE 1
MODE 2
MODE 3
```

当前 ST7789 / SPI1：

```text
MODE 3
CPOL High
CPHA 2nd Edge
```

Phase 1 只校验当前硬件是否满足，不动态切换。

## 5.2 Bit Order

支持描述：

```text
MSB FIRST
LSB FIRST
```

当前 ST7789：

```text
MSB FIRST
```

Phase 1 不动态切换。

## 5.3 Data Bits

模型保留 `dataBits`，但 Phase 1 唯一支持：

```text
8 bit
```

非 8 bit 配置返回 `PLATFORM_ERR_NOT_SUPPORTED`。

Phase 1 `platform_spi_write()` 的 `length` 明确定义为 byte count。

## 5.4 maxClockHz

`maxClockHz` 表示从设备允许的最高 SCK，而不是目标必须恰好运行的频率。

约束：

```text
maxClockHz > 0
actual SPI clock <= maxClockHz
```

Phase 1 不调整 prescaler，只校验 CubeMX 当前 SPI clock 是否满足设备限制。

STM32 Impl 必须从真实 HAL/RCC 配置推导当前 SPI clock，不长期硬编码 `12.5 MHz` 常量。

---

# 6. Bus Operations

第一版 Bus ops 最小集合：

```c
typedef struct
{
    platform_error_t (*applyConfig)(
        platform_spi_bus_t *bus,
        const platform_spi_device_config_t *config);

    platform_error_t (*write)(
        platform_spi_bus_t *bus,
        const uint8_t *data,
        platform_size_t length);
} platform_spi_bus_ops_t;
```

准确参数类型必须复用仓库现有 `platform_size_t / platform_bool_t / platform_error_t` 约定。

本阶段不加入：

```text
read()
transfer()
write_async()
DMA callback
lock/unlock ops
```

---

# 7. applyConfig() 合同

`applyConfig()` 的长期语义：

> Ensure the active SPI bus configuration satisfies the requested device configuration.

Phase 1 实际行为是严格 validation：

```text
Mode       current == requested
BitOrder   current == requested
DataBits   requested == 8 and current == 8
Clock      actualClockHz <= maxClockHz
```

全部满足：

```text
PLATFORM_ERR_OK
```

合法配置但当前 Impl 无法满足：

```text
PLATFORM_ERR_NOT_SUPPORTED
```

底层状态异常按现有错误体系映射为：

```text
NOT_INITIALIZED / INVALID_STATE / IO
```

重要：不得“配置字段写了但实现静默忽略”。

未来如果真正出现同总线不同 SPI Mode / clock 设备，再升级 STM32 Impl 的 `applyConfig()` 为必要时重新配置 SPI，Platform API 不改变。

`applyConfig()` 在 transaction begin 时执行，而不是每次 `write()` 都执行。

---

# 8. Transaction 模型

Phase 1 公共事务 API：

```c
platform_spi_transaction_begin(device);
platform_spi_write(device, data, length);
platform_spi_transaction_end(device);
```

不采用“每次 write 自动 CS Low/High”的设计。

原因：一个 SPI operation 可能包含多个连续阶段：

```text
CS active
 -> command
 -> address
 -> data / read
 -> CS inactive
```

ST7789 也需要在 SPI transaction 内独立切换 DC：

```text
transaction begin
 -> DC command
 -> SPI write command
 -> DC data
 -> SPI write data
 -> transaction end
```

因此：

```text
CS = generic SPI device selection
DC / RST / BL = LCD-specific control
```

DC / RST / BL 禁止进入 generic Platform SPI。

---

# 9. transaction_begin() 合同

建议顺序：

```text
validate device
validate device initialized
validate bus lifecycle == STARTED
validate bus activeDevice == NULL
applyConfig(device config)
set CS active if CS exists
publish bus->activeDevice = device
return OK
```

如果 `cs == NULL`：

```text
transaction still owns the bus
no GPIO CS action
activeDevice still tracks ownership
```

支持无 CS 的原因：

```text
single device with externally fixed NSS
external logic controls chip select
specific devices without independent CS
```

Phase 1 不加入 mutex；`activeDevice` 只负责状态检测，不能替代真正的 RTOS mutual exclusion。

---

# 10. platform_spi_write() 合同

第一版为 blocking synchronous TX：

```text
function returns only after transmission is complete or failed
caller may modify/reuse buffer after return
length is byte count
```

必须要求：

```text
device initialized
bus STARTED
bus->activeDevice == device
```

若没有先成功 begin：

```text
PLATFORM_ERR_INVALID_STATE
```

不设计隐式：

```text
write() auto begin/end
```

避免同一 API 出现两套事务语义。

STM32 Impl 对应：

```text
HAL_SPI_Transmit()
```

Timeout 作为 Phase 1 STM32 Impl 内部策略，不向公共 Platform write API 暴露 HAL 风格 timeout 参数。

禁止长期使用 `HAL_MAX_DELAY`；必须使用有限 timeout。

---

# 11. transaction_end() 合同

要求：

```text
device initialized
bus->activeDevice == device
```

执行：

```text
set CS inactive if CS exists
clear bus->activeDevice
```

一旦 begin 成功，无论后续 write 成功或失败，调用者都必须执行 end。

`platform_spi_write()` 失败不得隐式结束 transaction。

原因：transaction ownership 必须保持单一、显式、可预测。

如果 CS inactive GPIO 操作失败：

```text
still clear software activeDevice state
return GPIO error
```

避免软件永久把 Bus 留在 busy 状态；硬件异常由上层决定是否进入恢复流程。

---

# 12. SPI Device init / deinit

`platform_spi_device_init()` 只建立从设备描述符，不访问 SPI payload，也不重新初始化 HAL SPI。

职责：

```text
validate pointers / enum / config
validate maxClockHz > 0
bind bus
bind optional CS
save CS active level
save config
set CS inactive if CS exists
initialized = true
```

不要求在 init 时发送 SPI 数据。

推荐正常初始化顺序仍为：

```text
CubeMX hardware init
 -> Platform GPIO ready
 -> SPI Bus construct
 -> SPI Bus init/start
 -> SPI Device init
 -> future ST7789 init
```

`platform_spi_device_deinit()`：

```text
if bus->activeDevice == device -> PLATFORM_ERR_BUSY
set CS inactive if applicable
clear references/config
initialized = false
```

---

# 13. SPI Bus 生命周期

SPI Bus 复用现有 Platform device lifecycle，不创建第二套状态模型。

推荐：

```text
Impl construct
 -> CREATED
 -> lifecycle.init()
 -> INITIALIZED
 -> lifecycle.start()
 -> STARTED
 -> transaction allowed
```

Phase 1：

```text
CubeMX MX_SPI1_Init() remains hardware configuration owner
```

因此 Impl lifecycle `init()` 不再次调用 `HAL_SPI_Init()` 做第二套硬件配置源。

`init()` 负责：

```text
validate impl context
validate HAL handle / hardware initialized state
clear activeDevice
update Platform lifecycle state
```

`start()` 负责进入可传输状态。

`stop()`：

```text
activeDevice != NULL -> BUSY
otherwise transition according to existing Platform lifecycle convention
```

`deinit()`：

```text
must not have active transaction
follow existing UART/GPIO Platform common lifecycle semantics
```

不要为 SPI 单独发明新的 deinitialized state。

未来若项目需要多 SPI Device 不同 Mode/clock，再单独设计 Runtime Reconfiguration Phase，把硬件动态配置职责进一步收归 Impl。

---

# 14. STM32 Impl 绑定

建议新增：

```text
04_Impl/impl_mcu/impl_platform_spi.h
04_Impl/impl_mcu/impl_platform_spi.c
```

Impl 提供具体 SPI1 construct 入口，其风格应参考现有 UART Impl，而不是在 Platform 内 switch `SPI1/SPI2`。

推荐关系：

```text
impl_platform_spi1_construct()
    ↓
private stm32_spi_context
    ↓
SPI_HandleTypeDef *halSpi = &hspi1
    ↓
platform bus ops / lifecycle binding
```

`void *implContext` 合法，因为它是 Impl 私有 context 的 type erasure；禁止由上层把 `&hspi1` 直接塞入 Platform 对象。

HAL error mapping沿用现有 Platform error 体系：

```text
HAL_OK      -> PLATFORM_ERR_OK
HAL_BUSY    -> PLATFORM_ERR_BUSY
HAL_TIMEOUT -> PLATFORM_ERR_TIMEOUT
HAL_ERROR   -> PLATFORM_ERR_IO
```

若仓库现有 UART Impl 已有统一 helper，优先复用现有风格，不复制平行错误体系。

---

# 15. 文件边界

建议新增：

```text
03_Platform/platform_mcu/spi/
├── platform_spi_types.h
├── platform_spi.h
└── platform_spi.c

04_Impl/impl_mcu/
├── impl_platform_spi.h
└── impl_platform_spi.c
```

如果仓库现有 Platform MCU 模块不单独拆 `*_types.h`，实施时允许按最近邻模块风格合并，但不得改变本设计合同。

可能修改：

```text
03_Platform/platform_common/... device class definitions
Keil project/group files if manual source registration required
host test build list / mocks if current harness requires
```

若 Platform common 的 device class 已经需要为 SPI Bus 增加类型，新增 generic SPI class；不要把 SPI Bus 错归类为 DISPLAY。

未来 ST7789 本体仍应作为显示设备能力，不等于 SPI Bus。

---

# 16. 当前 ST7789 设备配置输入

后续正式 LCD Driver 创建 SPI Device 时应基于已验证硬件事实：

```text
bus          = SPI1 Platform Bus
CS           = PA4 Platform GPIO
CS active    = LOW
mode         = MODE 3
bitOrder     = MSB FIRST
dataBits     = 8
actual SCK   = 12.5 MHz current CubeMX configuration
```

`maxClockHz` 应从当前屏幕规格/项目参考文档中选择一个明确、可验证且不低于当前 12.5 MHz 的安全值；Codex 不得凭空猜测器件上限。

如果正式参考资料无法确认最大频率，本 SPI Phase 1 不应自行修改当前 12.5 MHz CubeMX 配置；应记录为待确认输入，而不是静默填一个随意常量。

---

# 17. 验证策略

本阶段不重复 LCD Bring-up。

最低验证：

```text
1. Host / compile contract tests for Platform SPI where current test harness supports.
2. Platform SPI contains no HAL dependency.
3. STM32 Impl is the only SPI HAL binding location.
4. invalid device/config/state paths return deterministic platform_error_t.
5. begin -> write -> end transaction state behaves correctly.
6. second begin while active returns BUSY.
7. write without begin returns INVALID_STATE.
8. wrong-device end returns INVALID_STATE.
9. optional NULL CS path works without GPIO access.
10. unsupported dataBits / incompatible fixed config are rejected.
11. HAL status mapping is correct.
12. existing Host regression remains PASS.
13. Keil production rebuild: 0 errors, no new relevant warnings.
```

Target smoke test only在确有必要验证新 Platform->Impl 调用链时执行最小受控传输；不得重新恢复 Bring-up-only 色块循环或把临时 HAL 测试代码作为正式实现。

---

# 18. 完成条件

本 Phase 完成后必须成立：

```text
formal ST7789 driver can depend on:
- Platform SPI
- Platform GPIO
- Platform delay/time capability

formal ST7789 driver does NOT depend on:
- HAL SPI
- hspi1
- CubeMX generated SPI internals
```

且：

```text
SPI Bus / Device model reusable for future W25Q64 / SPI sensor
current implementation remains small and synchronous
no premature DMA / mutex / dynamic config complexity
Phase 1~9 stable application baseline is not behaviorally changed
```

下一阶段再单独设计并实施正式 ST7789 Driver / Platform BSP，不在本 Phase 内继续扩展 Display Task、IPC 或 UART product behavior。
