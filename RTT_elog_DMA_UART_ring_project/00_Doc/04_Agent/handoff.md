# 工程长期记忆与交接说明

更新时间：2026-09-05

> 本文件是 AI Agent / Codex 与人工开发者恢复工程上下文时的长期入口。  
> Phase 1~9 核心工程已经完成并通过 Host / Keil / Target 综合验证，作为稳定 Application Baseline 保留。  
> Display Extension 已完成硬件资源确认、CubeMX SPI1 配置和 ST7789T3 最小 Bring-up 目标板验证。临时 Bring-up 测试代码已经回退。  
> 当前新施工阶段已经冻结为 **SPI Platform + STM32 Impl Phase 1**。  
> 当前正式执行入口：`00_Doc/04_Agent/implementation_plan.md`。  
> 当前主设计：`00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md`。

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
I2C        : Software I2C over PB6/PB7
Input      : PA0 User Key
Indicator  : PC13 Status LED
Display    : P169H002-CTP / ST7789T3 / 240x280
```

当前项目核心能力：

```text
Button + UART unified control
 -> APP Control FSM
 -> FreeRTOS 4-task application model
 -> DHT20 + MPU6050 unified acquisition
 -> shared Software I2C
 -> UART DMA + IDLE + RingBuffer communication
 -> RTT / EasyLogger diagnostics
 -> LED semantic feedback
```

当前增量目标：

```text
在稳定五层架构中正式接入 LCD 显示能力
先建立可复用 SPI Platform / Impl 基础设施
再进入正式 ST7789 Driver
最后讨论 Display Task / IPC / UART / ONCE 业务迁移
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

固定依赖：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED

APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

职责：

```text
APP      : 业务状态、任务调度、业务编排
Service  : 可复用业务能力，不绑定具体 MCU
Platform : 设备/OS 能力抽象与统一接口
Impl     : STM32 / FreeRTOS 等具体实现适配
Vendor   : HAL / CMSIS / FreeRTOS / 第三方库
```

CubeMX generated files 只承担：

```text
hardware initialization
scheduler bootstrap
IRQ / HAL Callback
thin glue
```

禁止把主要业务重新塞回 generated files。

---

# 3. Phase 1~9 稳定基线

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
Project Core                                     COMPLETE / BASELINE FROZEN
```

Phase 1~9 除修复缺陷外原则上保持稳定。

原 `implementation_plan.md` 的 Phase 9 历史内容已经被当前 SPI Phase 1 施工计划替换；历史架构信息由设计文档和 git history 保留。

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
- sole product UART TX requester
- product response/report formatting

Control Task
- Button 10 ms polling
- Button gesture processing
- sole APP Control FSM
- business orchestration

Acquisition Task
- acquisition scheduling
- sole runtime DHT20 / MPU6050 accessor
- sole shared Software I2C runtime accessor

Indicator Task
- LED semantic execution
- ONCE success blink
```

CubeMX `defaultTask` 不是第五个长期产品 Task。

当前 SPI Phase 1 不增加任何新 Task。

---

# 5. APP / UART / Acquisition 稳定事实

APP Control FSM 唯一业务状态：

```text
STOPPED
RUNNING
```

Button 与 UART 都映射到同一 FSM。

Unified Acquisition Service：

```text
DHT20 read
 -> MPU6050 read
 -> complete atomic acquisition result
```

当前周期：

```text
START -> immediate first sample
then every 2 s by absolute deadline
```

UART RX：

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

UART TX：

```text
Communication Task
 -> UART Service
 -> Platform UART async
 -> STM32 UART Impl
 -> HAL_UART_Transmit_DMA()
```

原则：

```text
Communication Task = sole USART1 product TX requester
USART1 = product control/data
RTT = diagnostics
```

Display Extension 是否最终停止周期 sensor UART TX 尚未冻结。

---

# 6. 当前 ONCE 语义

Phase 9 当前成功条件：

```text
DHT20 success
AND MPU6050 success
AND complete UART report TX success
```

完整链：

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

当前 SPI Phase 1 不修改 ONCE。

---

# 7. Display Extension 已确认硬件合同

屏幕：

```text
Module      : P169H002-CTP
Controller  : ST7789T3
Resolution  : 240 x 280
Interface   : 4-wire SPI display path
Pixel       : RGB565 / 2 bytes per pixel
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

含义：

```text
PA4 = software CS GPIO
PA6 = LCD DC GPIO，不作为 SPI1 MISO
PA1 = first-stage GPIO backlight
```

CubeMX SPI1 已保留：

```text
Master
TX Only Simplex
8 bit
MSB First
Software NSS
CPOL High
CPHA 2nd Edge
SPI Mode 3
Prescaler /8
SPI clock 12.5 MHz
SPI DMA disabled
SPI interrupt disabled
```

LCD GPIO 默认：

```text
LCD_CS   PA4   HIGH
LCD_DC   PA6   HIGH
LCD_RST  PB10  HIGH
LCD_BL   PA1   LOW
```

---

# 8. ST7789 最小 Bring-up 已完成

2026-09-05 目标板确认：

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

因此：

```text
LCD Minimal Bring-up Target Verification = PASS
```

临时测试代码已经人工回退。

当前仓库只保留：

```text
CubeMX SPI1 configuration
LCD GPIO configuration
spi.c / spi.h
HAL SPI support
Phase 1~9 stable application code
```

禁止重新把 Bring-up-only 代码原样当作正式实现。

---

# 9. 当前冻结设计：SPI Platform + STM32 Impl Phase 1

正式设计文档：

```text
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
```

正式执行计划：

```text
00_Doc/04_Agent/implementation_plan.md
```

当前目标：

```text
future ST7789 Driver
    ↓
Platform SPI Bus / Device
    ↓
STM32 SPI Impl
    ↓
HAL SPI1
```

本 Phase 完成后，ST7789 Driver 不得直接接触 HAL SPI。

---

# 10. SPI Bus / Device 冻结模型

核心区分：

```text
platform_spi_bus_t
 = MCU SPI Controller / Bus resource

platform_spi_device_t
 = a SPI slave attached to a bus
```

Bus 负责：

```text
Platform device lifecycle
ops
Impl binding
active transaction state
future synchronization point
```

Device 负责：

```text
name
bus reference
optional CS GPIO
CS active level
device SPI config
lightweight initialized state
```

CS：

```text
belongs to generic SPI Device
NOT SPI Bus
```

LCD 专属：

```text
DC
RST
BL
```

禁止进入 generic SPI。

允许：

```text
SPI Device cs == NULL
```

用于外部固定 NSS / 无独立 CS / 外部逻辑管理 CS 的设备。

---

# 11. SPI Device Config 冻结合同

```text
mode
bitOrder
dataBits
maxClockHz
```

当前 Phase 1：

```text
mode       : descriptor supports 0/1/2/3
bitOrder   : descriptor supports MSB/LSB
actual implementation target = current fixed hardware config
dataBits   : only 8 bit supported
maxClockHz : must > 0
```

`maxClockHz` 表示设备最大允许 SCK，不是目标必须恰好工作的频率。

当前 LCD 后续配置输入：

```text
bus       = SPI1
CS        = PA4
CS active = LOW
mode      = MODE 3
bitOrder  = MSB FIRST
dataBits  = 8
actual SCK current = 12.5 MHz
```

LCD `maxClockHz` 必须从项目参考资料确认，Codex 不得猜测器件上限。

---

# 12. SPI Transaction 冻结合同

公共模型：

```text
platform_spi_transaction_begin(device)
platform_spi_write(device, data, length)
platform_spi_transaction_end(device)
```

不采用：

```text
every write automatically toggles CS
```

原因：一个 SPI device operation 可能需要：

```text
CS active
 -> command
 -> address
 -> data/read
 -> CS inactive
```

ST7789 后续典型：

```text
begin
 -> DC command
 -> write command
 -> DC data
 -> write data
 -> end
```

Transaction 规则：

```text
begin success -> caller owns transaction
write does not auto-end even on failure
caller must call end after successful begin
```

Bus 使用 `activeDevice` 做状态检测；当前不是 RTOS mutex。

---

# 13. applyConfig 冻结合同

Bus ops Phase 1：

```text
applyConfig()
write()
```

`applyConfig()` 在 begin 时执行一次。

Phase 1 只严格验证当前 CubeMX 固定配置：

```text
current mode == requested mode
current bit order == requested bit order
current data bits == requested data bits == 8
actual SCK <= maxClockHz
```

不动态切换：

```text
CPOL / CPHA
FirstBit
DataSize
BaudRatePrescaler
```

不满足且当前 Impl 无法提供：

```text
PLATFORM_ERR_NOT_SUPPORTED
```

禁止静默忽略 config。

STM32 Impl 必须从 HAL/RCC 实际配置推导 SCK，不把 12.5 MHz 写死为长期实现常量。

---

# 14. SPI Bus 生命周期

SPI Bus 复用现有 Platform device lifecycle。

推荐顺序：

```text
CubeMX MX_SPI1_Init()
 -> Impl construct
 -> Platform Bus init
 -> Platform Bus start
 -> SPI Device init
 -> future ST7789 init
```

当前 Phase 1：

```text
CubeMX = SPI1 hardware configuration owner
```

因此 Impl lifecycle 不再建立第二套 `HAL_SPI_Init()` 配置源。

SPI Device 是轻量 descriptor，不复制完整 CREATED / INITIALIZED / STARTED / STOPPED 生命周期。

所有引用为 static non-owning reference；不使用 runtime malloc/free。

---

# 15. STM32 Impl 冻结边界

建议文件：

```text
04_Impl/impl_mcu/impl_platform_spi.h
04_Impl/impl_mcu/impl_platform_spi.c
```

推荐绑定：

```text
Platform SPI Bus
 -> void *implContext
 -> STM32 private SPI context
 -> SPI_HandleTypeDef *
 -> &hspi1
```

`void *implContext` 合法，因为它是 Impl 私有 type-erasure context。

禁止：

```text
Platform public object directly stores &hspi1 as a HAL contract
Platform header includes stm32f4xx_hal_spi.h
```

Blocking write：

```text
HAL_SPI_Transmit()
```

必须有限 timeout，不使用 `HAL_MAX_DELAY`。

HAL error mapping沿用现有 Platform error 语义。

---

# 16. 当前计划明确不做

```text
ST7789 formal driver
Display Platform BSP
font / text rendering
Display Task
Display Queue / snapshot IPC
UART periodic sensor TX removal
ONCE semantic migration
SPI read
full-duplex transfer
SPI DMA
SPI interrupt transfer
SPI bus mutex
runtime Mode / clock switching
backlight PWM
Touch / CTP
```

不要因为“以后可能需要”就在 SPI Phase 1 提前加入这些能力。

---

# 17. 当前资源约束

Phase 9 最终资源基线：

```text
Total RO Size   ≈ 54.50 KiB
Total RW Size   ≈ 44.55 KiB
Total ROM Size  ≈ 54.57 KiB
```

STM32F411CEU6：

```text
Flash = 512 KiB
SRAM  = 128 KiB
```

屏幕全帧 RGB565：

```text
240 * 280 * 2 = 134400 B
```

因此后续 Display 不建立 full-screen framebuffer；优先 direct region update / small buffer / line buffer。

SPI DMA 必须在实际刷新阻塞/CPU 占用出现后再讨论。

---

# 18. 当前施工入口

Codex 当前只执行：

```text
00_Doc/04_Agent/implementation_plan.md
```

必须先读：

```text
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
00_Doc/04_Agent/implementation_plan.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/execution_rules.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
```

本轮完成点：

```text
SPI Platform + Impl Phase 1
Host regression PASS
Keil rebuild PASS
formal ST7789 can start on top of Platform SPI
```

Codex 完成本阶段后必须停止，不自动继续 ST7789 Driver。

---

# 19. SPI Phase 1 完成后的下一讨论入口

下一阶段才讨论：

```text
Formal ST7789 Driver / Platform BSP
```

重点：

```text
1. ST7789 driver 文件边界
2. GPIO DC / RST / BL ownership
3. init command sequence porting
4. set_window / fill / fill_rect
5. basic text/font strategy
6. error / timeout contract
7. buffer size and RAM policy
```

ST7789 基础能力完成后，再讨论：

```text
Display Service / APP
Display Task 是否需要
Display Queue / latest snapshot strategy
Acquisition -> Display data contract
UART product output responsibility
ONCE completion semantic
SPI DMA optimization
```

不要一次性把所有显示阶段设计卡死。

---

# 20. 推荐恢复资料

优先：

```text
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/implementation_plan.md
00_Doc/04_Agent/architecture.md
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
00_Doc/02_架构设计/P169H200屏幕参考文件/P169H002_ST7789显示接入使用文档.md
00_Doc/02_架构设计/P169H200屏幕参考文件/P169H002-CTP NEW 规格书SPEC.md
03_Platform/platform_common/
03_Platform/platform_mcu/gpio/
03_Platform/platform_mcu/i2c/
03_Platform/platform_mcu/uart/
04_Impl/impl_mcu/impl_platform_uart.*
Core/Src/spi.c
Core/Inc/spi.h
05_Vendors/lcd/
```

需要稳定业务背景时再读：

```text
00_Doc/00_项目需求/最终功能需求.md
00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
```

---

# 21. 当前停止点

```text
Phase 1~9 core application:
COMPLETE / BASELINE FROZEN / TARGET VERIFIED

Display hardware review:
PASS

CubeMX SPI1 + LCD GPIO:
PASS / RETAINED

Minimal ST7789 Bring-up:
TARGET VERIFIED PASS
TEMPORARY TEST CODE REVERTED

SPI Platform + STM32 Impl Phase 1 design:
DESIGN FROZEN

SPI Platform + STM32 Impl Phase 1 implementation plan:
READY FOR CODEX

Formal ST7789 Driver:
NOT STARTED / NEXT AFTER SPI PHASE 1

Display Task / IPC / UART migration:
NOT DESIGNED

Touch / CTP:
OUT OF CURRENT STAGE
```
