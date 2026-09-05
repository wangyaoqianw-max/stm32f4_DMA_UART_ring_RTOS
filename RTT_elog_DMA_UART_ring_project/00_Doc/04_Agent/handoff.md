# 工程长期记忆与交接说明

更新时间：2026-09-05

> 本文件是 AI Agent / Codex 与人工开发者恢复工程上下文时的长期入口。  
> Phase 1~9 Core Application 已完成并通过 Host / Keil / Target 综合验证。  
> Display Extension 已完成硬件资源确认、CubeMX SPI1 + LCD GPIO、ST7789T3 最小 Bring-up，以及 SPI Platform + STM32 Impl Phase 1。  
> 当前没有 Active Implementation Plan。  
> 下一阶段从“正式 ST7789 Driver 架构/API 设计讨论”开始；不要重新做最小点亮验证，也不要直接继续旧 SPI Phase 1 施工计划。

---

# 0. 当前状态总览

```text
Phase 1~9 Core Application               COMPLETE / TARGET VERIFIED
Final Integrated Board Test              PASS

Display Hardware Resource Review         COMPLETE
CubeMX SPI1 + LCD GPIO                    COMPLETE
Minimal ST7789 Bring-up                   TARGET VERIFIED
Temporary Bring-up Code                   REVERTED
SPI Platform + STM32 Impl Phase 1         COMPLETE / HOST + KEIL VERIFIED
Formal ST7789 Driver                      NEXT DESIGN PHASE
Display Task / IPC                        NOT DESIGNED
UART Product Output Migration             NOT DESIGNED
ONCE Semantic Migration                   NOT DESIGNED
Touch / CTP                               DEFERRED

Current Active Implementation Plan        NONE
```

SPI Phase 1 验证：

```text
Focused Host tests : PASS / 2 groups
Host regression    : PASS / 36 groups
Keil rebuild       : PASS / 0 errors
Warnings           : 13 existing warnings, no new relevant warning
Target test        : NOT REQUIRED BY PLAN
```

---

# 1. 工程定位

工程根目录：

```text
RTT_elog_DMA_UART_ring_project/
```

目标环境：

```text
MCU        : STM32F411CEU6 / Cortex-M4F
Flash      : 512 KiB
SRAM       : 128 KiB
UART       : USART1 / 115200 8N1
RTOS       : CMSIS-RTOS2 + FreeRTOS
Debug Log  : EasyLogger + SEGGER RTT
Sensors    : DHT20 + MPU6050
I2C        : Software I2C over PB6/PB7
Input      : PA0 User Key
Indicator  : PC13 Status LED
Display    : P169H002-CTP / ST7789T3 / 240x280
```

项目当前定位：

```text
稳定 Core Application
 +
Display Extension incremental architecture exercise
```

---

# 2. 稳定分层合同

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

允许：

```text
APP -> Service
APP -> Platform
Service -> Platform
Platform -> Impl
```

禁止：

```text
APP -> Impl
Service -> Impl
```

职责：

```text
APP      : 业务状态、任务调度、业务编排
Service  : 可复用业务能力，不绑定具体 MCU
Platform : 设备 / OS 能力抽象
Impl     : STM32 / FreeRTOS 等具体适配
Vendor   : HAL / CMSIS / FreeRTOS / third-party
```

CubeMX generated files 只承担：

```text
hardware initialization
scheduler bootstrap
IRQ / HAL callback
thin glue
```

---

# 3. Phase 1~9 Core Application Baseline

```text
Phase 1  GPIO STM32 Impl                         COMPLETED
Phase 2  Board Resource + CubeMX Configuration   COMPLETED
Phase 3  Software I2C                            COMPLETED
Phase 4  LED Module                              COMPLETED
Phase 5  Button Module                           COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 6  DHT20 Environment Module                COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 7  MPU6050 Motion Module                   COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 8  UART Application Communication          COMPLETED / HOST + KEIL + TARGET VERIFIED
Phase 9  Final RTOS Application Integration      COMPLETED / HOST + KEIL + TARGET VERIFIED

Final Integrated Board Test                      PASS
```

不存在独立 Phase 10。

Phase 1~9 除缺陷修复外原则上保持稳定。

---

# 4. 稳定四任务模型

```text
Communication Task   2048 B   ABOVE_NORMAL
Control Task         1024 B   ABOVE_NORMAL
Acquisition Task     1536 B   NORMAL
Indicator Task        768 B   BELOW_NORMAL
```

职责：

```text
Communication Task
- UART RX parser
- UART Service ownerThread
- sole USART1 product TX requester

Control Task
- Button polling
- sole APP Control FSM
- business orchestration

Acquisition Task
- sampling scheduling
- sole DHT20 / MPU6050 runtime accessor
- sole shared Software I2C runtime accessor

Indicator Task
- LED semantic execution
```

CubeMX `defaultTask` 不是第五个产品 Task。

当前 SPI / LCD Platform 基础设施不增加新 Task。

---

# 5. APP Control / Acquisition 稳定事实

APP 唯一业务状态：

```text
STOPPED
RUNNING
```

只有 Control Task / APP FSM 修改该状态。

输入：

```text
Button SINGLE -> START
Button LONG   -> STOP
Button DOUBLE -> ONCE

UART START / STOP / ONCE / STATUS / HELP
```

Unified Acquisition Service：

```text
DHT20 read
 -> MPU6050 read
 -> complete atomic result
```

成功：

```text
DHT20 OK && MPU6050 OK
```

周期：

```text
START -> immediate first sample
then every 2 s by absolute deadline
```

---

# 6. UART DMA + RingBuffer 稳定架构

RX：

```text
USART1 RX
 -> DMA Circular
 -> IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform UART
 -> UART Service
 -> SPSC RingBuffer
 -> Communication Task
```

TX：

```text
Communication Task
 -> UART Service
 -> Platform UART async
 -> STM32 UART Impl
 -> HAL_UART_Transmit_DMA()
```

原则：

```text
RingBuffer single producer / single consumer
no ordinary RX mutex
no second RX path
Communication Task = sole product TX requester
USART1 = product control/data
RTT = diagnostics
```

Display Extension 未来可能停止周期 sensor UART TX，但尚未冻结。

---

# 7. 当前 ONCE 语义

Phase 9 当前成功条件：

```text
DHT20 success
AND MPU6050 success
AND complete UART report TX success
```

链：

```text
Control SAMPLE_ONCE
 -> Acquisition
 -> Unified Acquisition Service
 -> Communication ONCE_REPORT
 -> UART TX success
 -> ONCE_TX_RESULT OK
 -> Control
 -> Indicator ONCE_SUCCESS
```

Display 接入后 ONCE completion semantic 必须重新设计。

不要简单把 `communicationQueue` 换成 `displayQueue`。

---

# 8. Display Hardware Contract

屏幕：

```text
Module      : P169H002-CTP
Controller  : ST7789T3
Resolution  : 240 x 280
Interface   : 4-wire SPI display path
Pixel       : RGB565
Touch       : OUT OF CURRENT STAGE
```

引脚：

```text
PA1  -> LCD_BL
PA4  -> LCD_CS
PA5  -> SPI1_SCK
PA6  -> LCD_DC
PA7  -> SPI1_MOSI
PB10 -> LCD_RST
```

LCD GPIO 默认：

```text
LCD_CS   PA4   HIGH
LCD_DC   PA6   HIGH
LCD_RST  PB10  HIGH
LCD_BL   PA1   LOW
```

---

# 9. Minimal ST7789 Bring-up Result

2026-09-05 已完成目标板验证：

```text
ST7789T3 initialization       PASS
SPI1 Mode 3 @ 12.5 MHz       PASS
240 x 280 display area       PASS
X_OFFSET = 0                 PASS
Y_OFFSET = 20                PASS
RGB565 high-byte first       PASS
BLACK / WHITE / RED / GREEN / BLUE PASS
flower screen / unstable     NOT OBSERVED
```

背光：

```text
LCD_BACKLIGHT_ON_LEVEL  = HIGH
LCD_BACKLIGHT_OFF_LEVEL = LOW
```

临时 Bring-up 测试代码已经回退。

当前仓库只保留：

```text
CubeMX SPI1 configuration
LCD GPIO configuration
spi.c / spi.h
HAL SPI support
```

正式 ST7789 Driver 不得直接复制临时实验代码作为架构实现。

---

# 10. SPI Platform + STM32 Impl Phase 1 完成记录

正式设计：

```text
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
```

已完成施工记录：

```text
00_Doc/04_Agent/implementation_plan.md
```

新增：

```text
03_Platform/platform_mcu/spi/platform_spi_types.h
03_Platform/platform_mcu/spi/platform_spi.h
03_Platform/platform_mcu/spi/platform_spi.c

04_Impl/impl_mcu/impl_platform_spi.h
04_Impl/impl_mcu/impl_platform_spi.c
```

测试：

```text
Tests/platform_spi/test_platform_spi.c
Tests/impl_platform_spi/spi.h
Tests/impl_platform_spi/test_impl_platform_spi.c
```

---

# 11. SPI Bus / Device 稳定模型

```text
platform_spi_bus_t
 = MCU SPI Controller / Bus
 = Platform lifecycle device
 = ops + implContext + activeDevice

platform_spi_device_t
 = lightweight SPI slave descriptor
 = bus reference
 = optional CS GPIO
 = CS active level
 = SPI device config
 = initialized state
```

CS 属于 generic SPI Device。

LCD 专属：

```text
DC
RST
BL
```

不进入 generic SPI。

允许：

```text
cs == NULL
```

---

# 12. SPI Transaction 稳定合同

公共 API：

```text
platform_spi_transaction_begin(device)
platform_spi_write(device, data, length)
platform_spi_transaction_end(device)
```

规则：

```text
begin success -> caller owns transaction
write requires activeDevice == device
write does not auto-end
successful begin must call end
second begin while active -> BUSY
wrong-device write/end -> INVALID_STATE
```

不采用每次 `write()` 自动切 CS 的设计。

---

# 13. SPI Device Config 稳定合同

```text
mode
bitOrder
dataBits
maxClockHz
```

当前 Phase 1 能力：

```text
blocking synchronous TX only
8-bit only
optional software CS
fixed CubeMX config validation
```

`maxClockHz` 表示设备最大允许 SCK。

---

# 14. STM32 SPI Impl 稳定合同

调用链：

```text
impl_platform_spi1_construct()
 -> private STM32 context
 -> &hspi1
```

Platform public header 不暴露 HAL 类型。

`applyConfig()` 从实际 HAL/RCC 配置检查：

```text
Mode
FirstBit
DataSize
actual SCK
```

当前不做 runtime dynamic reconfiguration。

Blocking write：

```text
HAL_SPI_Transmit()
finite timeout = 1000 ms
```

HAL 状态映射：

```text
HAL_OK      -> PLATFORM_ERR_OK
HAL_BUSY    -> PLATFORM_ERR_BUSY
HAL_TIMEOUT -> PLATFORM_ERR_TIMEOUT
HAL_ERROR   -> PLATFORM_ERR_IO
```

---

# 15. CubeMX SPI1 Actual Configuration Note

设计讨论早期曾按：

```text
TX Only Simplex
```

描述。

实现阶段检查实际仓库：

```text
.ioc / Core/Src/spi.c = SPI_DIRECTION_2LINES
```

人工确认：

```text
不修改 CubeMX 当前配置
Platform Phase 1 仍只提供 TX API
不增加 read / full-duplex transfer
```

以后讨论 SPI read/full-duplex 时再单独扩展。

---

# 16. SPI Phase 1 Verification

```text
Focused Host tests : PASS / 2 groups
Host regression    : PASS / 36 groups
Keil rebuild       : PASS / 0 errors
Target test        : NOT REQUIRED BY PLAN
```

Coding / Architecture Review：

```text
Platform HAL-free
APP / Service no Impl dependency
no LCD-specific semantics in generic SPI
no DMA / IRQ / mutex / runtime config expansion
no runtime malloc/free
git diff --check PASS
```

---

# 17. Display Memory Constraint

STM32F411CEU6：

```text
SRAM = 128 KiB
```

全屏 RGB565 framebuffer：

```text
240 * 280 * 2 = 134400 B
```

因此正式显示实现禁止默认使用全屏 framebuffer。

优先：

```text
direct region update
small line/block buffer
partial refresh
```

SPI DMA 暂不提前加入。

---

# 18. 当前未冻结内容

不要提前实现或假定：

```text
formal ST7789 Driver API
Display abstraction
Display Task
Display Queue / snapshot
font organization
partial refresh policy
UART periodic sensor TX removal
STATUS / HELP / ACK final routing
ONCE completion migration
SPI DMA
runtime SPI mode / clock switching
backlight PWM
Touch / CTP
```

这些必须后续逐项讨论。

---

# 19. 下一正式入口

当前没有 Active Implementation Plan。

下一阶段：

```text
Formal ST7789 Driver Design
```

建议讨论顺序：

```text
1. ST7789 Driver 文件与层级落点
2. Driver object / dependency model
3. reset / init / backlight
4. command / data transaction helper
5. set_window
6. fill / fill_rect
7. text / number minimal scope
8. vendor reference code reuse boundary
9. error / timeout contract
10. Host test strategy
11. formal ST7789 target verification
```

设计冻结后再生成新的 Implementation Plan。

不要重新执行旧 SPI Phase 1 Plan。

---

# 20. 推荐恢复资料

优先读取：

```text
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/requirements.md
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
00_Doc/04_Agent/implementation_plan.md   # completed record only

03_Platform/platform_mcu/spi/
04_Impl/impl_mcu/impl_platform_spi.*
Core/Src/spi.c
Core/Inc/spi.h
05_Vendors/lcd/
00_Doc/02_架构设计/P169H200屏幕参考文件/
```

需要 Core APP 上下文时再读取：

```text
00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md
01_APP/app_system.c
01_APP/app_control.*
01_APP/app_acquisition.*
01_APP/app_communication.*
01_APP/app_ipc_types.h
```

---

# 21. 当前停止点

```text
Core Application                      COMPLETE
Display Hardware + Minimal Bring-up   COMPLETE
SPI Platform + STM32 Impl Phase 1     COMPLETE
Current Active Plan                   NONE
Next                                  FORMAL ST7789 DRIVER DESIGN DISCUSSION
```
