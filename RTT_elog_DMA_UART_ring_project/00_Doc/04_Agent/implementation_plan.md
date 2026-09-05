# SPI Platform + STM32 Impl Phase 1 Implementation Plan

> 当前执行计划 / Current Active Plan  
> 状态：DESIGN FROZEN / READY FOR CODEX  
> 日期：2026-09-05  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

**Goal:** 在已经 Target-Verified 的 SPI1 + ST7789T3 硬件基础上，正式建立可复用的 SPI Bus / SPI Device Platform 抽象与 STM32 HAL Impl 适配，为下一阶段正式 ST7789 Driver 提供稳定依赖。

**Primary Design:** `00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md`

**Important:** LCD 最小 Bring-up 已完成并回退临时代码。本计划不重新做点亮实验，不实现 ST7789 正式驱动，不引入 SPI DMA / Display Task / UART 输出迁移。

---

# 0. Codex 开工前必读

```text
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/implementation_plan.md

03_Platform/platform_common/
03_Platform/platform_mcu/gpio/
03_Platform/platform_mcu/i2c/
03_Platform/platform_mcu/uart/
04_Impl/impl_mcu/impl_platform_gpio.*
04_Impl/impl_mcu/impl_platform_uart.*
Core/Src/spi.c
Core/Inc/spi.h
```

实施原则：

```text
APP -> Impl FORBIDDEN
Service -> Impl FORBIDDEN
Platform SPI public headers contain no HAL type
reuse existing platform_error_t / platform_device_t / lifecycle conventions
reuse nearest existing module style before creating new abstractions
no runtime malloc/free
no LCD-specific DC/RST/BL logic in generic SPI
no SPI DMA / IRQ / read / full-duplex / mutex in this Phase
no runtime SPI mode/clock switching in this Phase
no Bring-up-only color test restoration
```

每个 Task：

```text
1. inspect nearest implementation and tests
2. add/adjust focused contract tests first where practical
3. confirm expected RED for new behavior
4. implement smallest production change
5. run focused tests
6. run relevant regression
7. perform coding-standard review
8. continue only after green
```

---

# 1. Task 0 — Baseline Verification

改代码前确认并记录：

```text
current Host regression PASS
Keil production build PASS / 0 errors
SPI1 CubeMX configuration remains:
  Master
  TX Only Simplex
  8 bit
  MSB First
  Software NSS
  Mode 3
  Prescaler /8
  12.5 MHz current clock
  DMA disabled
  IRQ disabled

LCD Bring-up temporary code is absent
Phase 1~9 product behavior unchanged
```

本 Task 不修改架构。

---

# 2. Task 1 — Add Generic SPI Types and Device Class Support

**Create:**

```text
03_Platform/platform_mcu/spi/platform_spi_types.h
```

若最近邻 Platform MCU 模块的文件组织证明无需独立 `*_types.h`，可在不改变设计合同的前提下合并到 `platform_spi.h`；不要为文件数量机械拆分。

定义至少包括：

```text
platform_spi_mode_t
  MODE_0
  MODE_1
  MODE_2
  MODE_3

platform_spi_bit_order_t
  MSB_FIRST
  LSB_FIRST

platform_spi_device_config_t
  mode
  bitOrder
  dataBits
  maxClockHz
```

语义冻结：

```text
dataBits Phase 1 only supports 8
maxClockHz must be > 0
maxClockHz = device maximum accepted SCK, not required exact SCK
write length = byte count
```

如现有 `platform_device_class_t` 需要新增 SPI Bus class：

```text
add generic PLATFORM_DEVICE_CLASS_SPI
```

不得把 SPI Bus 归类为 DISPLAY。

先完成 compile-contract tests / static review。

---

# 3. Task 2 — Implement Platform SPI Bus / Device Contracts

**Create:**

```text
03_Platform/platform_mcu/spi/platform_spi.h
03_Platform/platform_mcu/spi/platform_spi.c
```

## 3.1 Bus model

按现有 Platform common object / lifecycle 风格实现 `platform_spi_bus_t`。

必须表达：

```text
platform_device_t base
bus ops
void *implContext
activeDevice
```

`implContext` 仅用于 Impl 私有 type-erasure context；Platform 不得把它解释为 HAL handle。

## 3.2 Device model

实现轻量 `platform_spi_device_t`，至少保存：

```text
name
bus non-owning reference
optional CS GPIO non-owning reference
CS active level
platform_spi_device_config_t
initialized flag
```

Device 不复制完整 Platform lifecycle 状态机。

允许：

```text
cs == NULL
```

此时 transaction 仍管理 Bus ownership，但不操作 GPIO CS。

## 3.3 Public APIs

第一版公共接口至少：

```text
platform_spi_device_init()
platform_spi_device_deinit()
platform_spi_transaction_begin()
platform_spi_write()
platform_spi_transaction_end()
```

精确命名可按仓库公共 API 习惯微调，但不得改变冻结语义。

---

# 4. Task 3 — Platform SPI Transaction State Machine Tests First

在现有 Host test harness 中增加最接近 Platform SPI 的测试，不建立第二套测试框架。

最低覆盖：

```text
valid device init PASS
NULL mandatory pointer rejected
invalid mode rejected
invalid bit order rejected
maxClockHz == 0 rejected
unsupported dataBits rejected deterministically
optional NULL CS accepted

device begin when bus not STARTED -> INVALID_STATE
begin success -> activeDevice == device
second begin while active -> BUSY
write without begin -> INVALID_STATE
write from non-active device -> INVALID_STATE
end from wrong device -> INVALID_STATE
end success -> activeDevice == NULL

device deinit while active -> BUSY
bus stop/deinit while active -> BUSY
```

GPIO spy/mock 场景若现有 harness 支持，还应覆盖：

```text
begin with CS -> set active level
end with CS -> set inactive level
NULL CS -> no GPIO access
CS inactive failure on end -> software activeDevice still cleared, error returned
```

---

# 5. Task 4 — Define Bus Ops and applyConfig Contract

Platform Bus ops 第一版只保留：

```text
applyConfig()
write()
```

不加入：

```text
read
transfer
async
DMA
lock/unlock
```

`applyConfig()` 必须在 `transaction_begin()` 中执行一次，而不是每次 `write()` 执行。

Phase 1 合同：

```text
current mode == requested mode
current bit order == requested bit order
current data bits == requested data bits == 8
actual SPI clock <= device maxClockHz
```

满足 -> `PLATFORM_ERR_OK`

合法但当前固定硬件配置无法满足 -> `PLATFORM_ERR_NOT_SUPPORTED`

禁止静默忽略 device config。

---

# 6. Task 5 — Implement STM32 SPI Impl

**Create:**

```text
04_Impl/impl_mcu/impl_platform_spi.h
04_Impl/impl_mcu/impl_platform_spi.c
```

实现具体 SPI1 construct/binding，风格参考现有 UART Impl：

```text
Platform bus
 -> void *implContext
 -> STM32 private SPI context
 -> SPI_HandleTypeDef *
 -> &hspi1
```

Platform header 不得暴露 `SPI_HandleTypeDef`。

## 6.1 Hardware lifecycle

Phase 1 保持：

```text
MX_SPI1_Init() = hardware configuration owner
```

因此 SPI Impl lifecycle init 不再次建立第二套 `HAL_SPI_Init()` 配置源。

Impl lifecycle 负责：

```text
validate private context
validate HAL SPI handle / initialized hardware state
clear active transaction state as appropriate
follow existing Platform lifecycle transition rules
```

start/stop/deinit 必须复用当前 Platform common lifecycle 语义；不要为 SPI 发明新状态。

## 6.2 applyConfig STM32 implementation

从 `hspi1.Init` 与实际 RCC/PCLK 配置读取/推导：

```text
CPOL / CPHA -> SPI mode
FirstBit -> bit order
DataSize -> data bits
BaudRatePrescaler + peripheral clock -> actual SCK
```

不得长期硬编码：

```text
actualClockHz = 12500000
```

12.5 MHz 是当前已验证硬件事实，不是 Impl 固定常量。

Phase 1 只 validation，不动态修改：

```text
CPOL
CPHA
FirstBit
DataSize
BaudRatePrescaler
```

## 6.3 Blocking write

Impl write 使用：

```text
HAL_SPI_Transmit()
```

采用有限 timeout，不使用 `HAL_MAX_DELAY`。

Timeout 在 Phase 1 为 STM32 Impl 内部策略，不向 Platform public API 暴露 HAL-style timeout 参数。

HAL error mapping沿用现有错误体系：

```text
HAL_OK      -> PLATFORM_ERR_OK
HAL_BUSY    -> PLATFORM_ERR_BUSY
HAL_TIMEOUT -> PLATFORM_ERR_TIMEOUT
HAL_ERROR   -> PLATFORM_ERR_IO
```

如 UART Impl 已存在通用 mapping 风格，优先保持一致。

---

# 7. Task 6 — Complete Transaction Semantics

`platform_spi_transaction_begin(device)` 顺序：

```text
validate device
validate initialized
validate bus STARTED
validate activeDevice == NULL
applyConfig(device config)
set CS active if present
activeDevice = device
return OK
```

`platform_spi_write(device, data, length)`：

```text
validate data / length
validate device initialized
validate bus STARTED
validate activeDevice == device
call bus->ops->write()
```

同步语义：

```text
return only after transfer completed or failed
caller may reuse/modify buffer after return
```

`platform_spi_transaction_end(device)`：

```text
validate activeDevice == device
set CS inactive if present
clear activeDevice
return result
```

重要：

```text
begin success transfers transaction ownership to caller
write success/failure does NOT auto-end
caller MUST call end after successful begin
```

若 CS inactive GPIO 操作失败：

```text
clear activeDevice anyway
return GPIO error
```

---

# 8. Task 7 — Device Init / Deinit Semantics

`platform_spi_device_init()`：

```text
validate configuration
bind bus
bind optional CS
save active level
save config
set CS inactive if present
initialized = true
```

推荐实际系统顺序：

```text
CubeMX hardware init
 -> Platform GPIO ready
 -> SPI Bus construct
 -> SPI Bus lifecycle init/start
 -> SPI Device init
 -> future ST7789 init
```

`platform_spi_device_deinit()`：

```text
active transaction owned by this device -> BUSY
set CS inactive if present
clear references/config
initialized = false
```

所有关系均为 non-owning static references；不得 malloc/free。

---

# 9. Task 8 — Production Build Integration

将新增 production source 按现有工程组织加入 Keil project/group 与 include path。

检查：

```text
no duplicate generated SPI source
no modification of unrelated CubeMX generated regions
platform_spi does not include HAL header
impl_platform_spi is the HAL binding location
existing SPI1 CubeMX config unchanged
```

如 Host test build 使用显式 source list，同步加入必要 Platform SPI test source/mocks。

---

# 10. Task 9 — Focused Tests + Full Regression

Focused tests 必须覆盖：

```text
SPI Device config validation
Bus lifecycle state validation
transaction ownership
CS active/inactive behavior
NULL CS behavior
applyConfig compatible config
applyConfig incompatible config
blocking write forwarding
HAL error mapping
```

然后运行全部现有 Host regression。

要求：

```text
all prior tests remain PASS
no Phase 1~9 behavior change
no new architecture violation
```

执行 `嵌入式项目C代码设计规范.md` 的 Coding Standard Review。

---

# 11. Task 10 — Keil Production Rebuild

要求：

```text
0 compile errors
no new relevant warnings
```

静态检查：

```text
Platform SPI public interface HAL-free
APP/Service do not include Impl SPI
no LCD-specific DC/RST/BL in generic SPI
no DMA/IRQ/mutex implementation added
no runtime dynamic SPI reconfiguration added
```

---

# 12. Task 11 — Optional Target Smoke Verification

LCD 最小 Bring-up 已经 PASS，因此默认不重复整套色块测试。

只有在 Host + Keil 无法充分验证新的 Platform -> Impl 调用链时，才做最小受控 target smoke test。

允许：

```text
short controlled Platform SPI transaction
verify no regression / no hang
```

禁止：

```text
restore old bring-up-only pure-color loop as production code
reintroduce direct HAL LCD driver path
use temporary test code as formal ST7789 implementation
```

---

# 13. Completion Gate

本计划完成后必须成立：

```text
03_Platform/platform_mcu/spi exists
04_Impl/impl_mcu/impl_platform_spi exists
SPI Bus / Device model compile and tests PASS
SPI1 HAL binding exists only in Impl side
blocking transaction begin/write/end works
current fixed SPI1 config is strictly validated
Host regression PASS
Keil rebuild PASS / 0 errors
```

并且下一阶段正式 ST7789 Driver 可以只依赖：

```text
Platform SPI
Platform GPIO
Platform delay/time capability
```

不得依赖：

```text
HAL_SPI_Transmit
SPI_HandleTypeDef
hspi1
CubeMX SPI implementation internals
```

---

# 14. Explicitly Deferred

以下内容不得在本计划中顺带实现：

```text
ST7789 formal driver
Display Platform BSP
font/text rendering
Display Task
Display Queue / snapshot IPC
UART periodic sensor TX removal
ONCE semantic migration
SPI read / full duplex
SPI DMA
SPI interrupt transfer
SPI bus mutex
runtime CPOL/CPHA/clock switching
backlight PWM
Touch / CTP
```

这些在 SPI Phase 1 完成并 review 后，重新讨论并形成下一份设计与实施计划。

---

# 15. Codex Stop Condition

完成 SPI Platform + Impl Phase 1 后停止。

不要自动继续写 ST7789 Driver。

最终向人工开发者报告：

```text
changed files
new tests
Host regression result
Keil build result
any target smoke result
remaining limitations
whether completion gate is satisfied
```
