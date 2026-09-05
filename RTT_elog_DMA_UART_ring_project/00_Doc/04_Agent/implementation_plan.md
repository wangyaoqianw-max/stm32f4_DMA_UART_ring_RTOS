# SPI Platform + STM32 Impl Phase 1 Implementation Plan

> 文档类型：Completed Implementation Record  
> 状态：COMPLETED / HOST + KEIL VERIFIED  
> 日期：2026-09-05  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

**Goal:** 在已经 Target-Verified 的 SPI1 + ST7789T3 硬件基础上，建立可复用的 SPI Bus / SPI Device Platform 抽象与 STM32 HAL Impl 适配，为正式 ST7789 Driver 提供稳定依赖。

**Primary Design:** `00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md`

**Current Status:** 本计划已经执行完成，本文件不再作为当前 Codex 执行入口。下一阶段应先讨论并冻结正式 ST7789 Driver 设计，再生成新的实施计划。

---

# 0. Completion Summary

```text
Implementation  : COMPLETE
Host focused    : PASS / 2 test groups
Host regression : PASS / 36 test groups
Keil rebuild    : PASS / 0 errors
Warnings        : 13 existing warnings, no new relevant warning
Target test     : NOT REQUIRED BY PLAN
```

本阶段按计划没有重新进行 LCD 板测。

原因：

```text
SPI1 pins / Mode 3 / 12.5 MHz
software CS
HAL_SPI_Transmit()
ST7789 minimal bring-up
```

已经在前置最小 Bring-up 阶段目标板验证通过。本阶段只建立正式 Platform -> Impl 软件抽象。

---

# 1. Implemented Production Files

新增：

```text
03_Platform/platform_mcu/spi/platform_spi_types.h
03_Platform/platform_mcu/spi/platform_spi.h
03_Platform/platform_mcu/spi/platform_spi.c

04_Impl/impl_mcu/impl_platform_spi.h
04_Impl/impl_mcu/impl_platform_spi.c
```

修改：

```text
03_Platform/platform_common/platform_device.h
MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
```

测试：

```text
Tests/platform_spi/test_platform_spi.c
Tests/impl_platform_spi/spi.h
Tests/impl_platform_spi/test_impl_platform_spi.c
```

---

# 2. Frozen Resulting Architecture

```text
future ST7789 Driver
        ↓
platform_spi_device_t
        ↓
platform_spi_transaction_begin()
platform_spi_write()
platform_spi_transaction_end()
        ↓
platform_spi_bus_t / ops
        ↓
STM32 SPI Impl
        ↓
HAL_SPI_Transmit()
        ↓
hspi1
```

稳定分层继续保持：

```text
APP -> Service -> Platform -> Impl -> Vendor / HAL / Hardware
```

禁止：

```text
APP -> Impl
Service -> Impl
Platform public header exposing HAL type
formal ST7789 driver directly accessing hspi1 / HAL SPI
```

---

# 3. SPI Bus / Device Model

已经实现：

```text
platform_spi_bus_t
 = MCU SPI Controller / Bus
 = formal Platform lifecycle device
 = ops + implContext + activeDevice

platform_spi_device_t
 = lightweight SPI slave descriptor
 = bus reference
 = optional CS GPIO
 = configurable CS active level
 = SPI device configuration
 = lightweight initialized state
```

所有关系为 static non-owning references。

没有：

```text
runtime malloc/free
device registry
SPI slave linked list
```

---

# 4. Transaction Contract

公共事务模型：

```text
platform_spi_transaction_begin(device)
platform_spi_write(device, data, length)
platform_spi_transaction_end(device)
```

冻结规则：

```text
begin success -> caller owns transaction
write requires activeDevice == device
write does not auto-end on success or failure
caller must end after successful begin
second begin while bus active -> BUSY
wrong-device write/end -> INVALID_STATE
```

CS：

```text
belongs to SPI Device
may be NULL
supports active LOW or HIGH
```

LCD 专属：

```text
DC
RST
BL
```

没有下沉到 generic SPI。

---

# 5. SPI Device Configuration

已实现配置合同：

```text
mode
bitOrder
dataBits
maxClockHz
```

Phase 1 实际能力：

```text
blocking synchronous TX only
8-bit data only
fixed CubeMX runtime configuration validation
```

`maxClockHz` 表示设备允许的最大 SCK。

STM32 Impl 从 HAL Handle 与 RCC/PCLK 实际配置推导当前时钟，不长期硬编码 12.5 MHz。

---

# 6. STM32 Impl Result

STM32 SPI1 绑定：

```text
impl_platform_spi1_construct()
 -> private stm32_spi_impl_context_t
 -> SPI_HandleTypeDef *halSpi = &hspi1
```

HAL 类型只存在于 Impl 内。

`applyConfig()` 当前严格检查：

```text
SPI mode
bit order
8-bit data size
actual SCK <= maxClockHz
```

当前不进行 runtime reconfiguration。

Blocking write：

```text
HAL_SPI_Transmit()
finite timeout = 1000 ms
```

错误映射：

```text
HAL_OK      -> PLATFORM_ERR_OK
HAL_BUSY    -> PLATFORM_ERR_BUSY
HAL_TIMEOUT -> PLATFORM_ERR_TIMEOUT
HAL_ERROR   -> PLATFORM_ERR_IO
```

---

# 7. Lifecycle Result

SPI Bus 复用现有 Platform lifecycle：

```text
CREATED
 -> INITIALIZED
 -> STARTED
 -> STOPPED
 -> CREATED after deinit
```

当前硬件配置 owner 仍然是：

```text
CubeMX MX_SPI1_Init()
```

SPI Impl lifecycle 不建立第二套 HAL_SPI_Init 配置源。

Bus 有 active transaction 时：

```text
stop/deinit -> BUSY
```

---

# 8. Actual CubeMX Configuration Note

设计讨论阶段曾按：

```text
TX Only Simplex
```

描述 SPI1。

实施时检查实际仓库发现：

```text
.ioc / Core/Src/spi.c = SPI_DIRECTION_2LINES
```

人工确认后按实际生成配置继续：

```text
不修改 CubeMX SPI1 配置
Platform Phase 1 仍只公开 TX API
不新增 read / full-duplex transfer
```

该差异已经记录到 `handoff.md`。

---

# 9. Verification Record

Focused Host tests 覆盖：

```text
Device config validation
Bus lifecycle validation
transaction ownership
CS active/inactive
NULL CS
compatible/incompatible applyConfig
blocking write forwarding
HAL status mapping
```

结果：

```text
Focused SPI Host tests : PASS / 2 groups
Full Host regression   : PASS / 36 groups
Keil Production rebuild: PASS / 0 errors
```

Architecture / coding review：

```text
Platform SPI HAL-free
APP / Service do not depend on Impl SPI
no LCD-specific control semantics in generic SPI
no DMA / IRQ / mutex / runtime SPI reconfiguration
no runtime malloc/free
git diff --check PASS
```

---

# 10. Explicitly Deferred

未实现：

```text
formal ST7789 Driver
LCD init / fill / set-window
font / text rendering
Display Task
Display Queue / snapshot IPC
UART product-output migration
ONCE semantic migration

SPI read
full-duplex transfer
SPI DMA
SPI interrupt transfer
SPI bus mutex
runtime CPOL/CPHA/clock/data-size switching
complex transaction descriptor
backlight PWM
Touch / CTP
```

这些内容不得因为本 SPI 基础层已完成就自动继续施工。

---

# 11. Completion Gate

本计划完成条件已经满足：

```text
Platform SPI Bus / Device established
explicit transaction API established
STM32 SPI1 HAL binding isolated in Impl
fixed-config validation established
blocking TX established
Host focused tests PASS
Host regression PASS
Keil rebuild PASS
no target board test required
```

因此：

```text
SPI Platform + STM32 Impl Phase 1 = COMPLETE
```

---

# 12. Next Entry

当前没有 Active Implementation Plan。

下一阶段流程：

```text
review current SPI implementation
 -> discuss formal ST7789 Driver architecture/API
 -> freeze design
 -> create new implementation plan
 -> Codex implementation
 -> target verification of formal ST7789 path
```

不要直接从本文件继续施工 ST7789。
