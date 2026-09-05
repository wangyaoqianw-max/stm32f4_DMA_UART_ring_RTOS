# Embedded Acquisition + Display Extension Development Roadmap

> 文档类型：Development Phase Roadmap  
> 状态：CURRENT BASELINE  
> 更新时间：2026-09-05  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 文档职责

本文件回答：

```text
哪些阶段已经完成？
当前工程基线是什么？
Display Extension 进行到哪里？
下一阶段应该从哪里开始？
```

当前主要参考：

```text
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
```

当前没有 Active Implementation Plan；上一份 SPI Phase 1 计划已经完成并归档在：

```text
00_Doc/04_Agent/implementation_plan.md
```

---

# 2. Phase 1~9 Core Application Baseline

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

不存在独立 Phase 10。

Phase 1~9 除修复缺陷外原则上保持稳定。

---

# 3. Stable Core Capabilities

当前稳定能力：

```text
UART DMA RX + IDLE / HT / TC
UART DMA TX
UART Service + SPSC RingBuffer
strict CRLF command protocol
Platform OS
RTT + EasyLogger diagnostics
Platform GPIO
Software I2C
Button + Button Service
Indicator Service
DHT20
MPU6050
Unified Acquisition Service
4-task FreeRTOS application model
APP Control FSM
```

最终产品任务：

```text
Communication Task
Control Task
Acquisition Task
Indicator Task
```

CubeMX defaultTask 不是产品任务。

---

# 4. Stable Product Behavior

默认：

```text
APP state = STOPPED
LED = OFF
UART RX active
RTT active
no periodic acquisition
```

控制：

```text
Button SINGLE -> START
Button LONG   -> STOP
Button DOUBLE -> ONCE

UART:
START
STOP
ONCE
STATUS
HELP
```

采集：

```text
START -> immediate first DHT20 + MPU6050 sample
then every 2 s by absolute deadline
```

当前 ONCE success 仍定义为：

```text
complete acquisition success
AND complete UART report TX success
```

Display 接入后该语义需要重新讨论。

---

# 5. Display Extension Roadmap

当前屏幕：

```text
P169H002-CTP
ST7789T3
240 x 280
RGB565
Touch excluded from current stage
```

Display Extension 当前状态：

```text
Hardware Resource Review             COMPLETE
CubeMX SPI1 + LCD GPIO               COMPLETE
Minimal ST7789 Bring-up              TARGET VERIFIED
Temporary Bring-up Code              REVERTED
SPI Platform + STM32 Impl Phase 1    COMPLETE / HOST + KEIL VERIFIED
Formal ST7789 Driver                 NEXT DESIGN PHASE
Display Task / IPC                   NOT DESIGNED
UART Product Output Migration        NOT DESIGNED
ONCE Semantic Migration              NOT DESIGNED
Touch / CTP                          DEFERRED
```

---

# 6. Verified LCD Hardware Contract

引脚：

```text
PA1  -> LCD_BL
PA4  -> LCD_CS
PA5  -> SPI1_SCK
PA6  -> LCD_DC
PA7  -> SPI1_MOSI
PB10 -> LCD_RST
```

目标板已确认：

```text
SPI1 Mode 3
12.5 MHz current clock
8 bit
MSB First
software CS
240 x 280
X_OFFSET = 0
Y_OFFSET = 20
RGB565 high-byte first
BL High = ON
BL Low  = OFF
BLACK / WHITE / RED / GREEN / BLUE PASS
```

最小 Bring-up 只作为硬件事实来源，不作为正式驱动代码。

---

# 7. SPI Platform + STM32 Impl Phase 1

状态：

```text
COMPLETE
```

结果：

```text
Focused Host tests : PASS / 2 groups
Host regression    : PASS / 36 groups
Keil rebuild       : PASS / 0 errors
Target test        : NOT REQUIRED BY PLAN
```

已建立：

```text
SPI Bus
SPI Device
optional CS
configurable CS active level
explicit begin / write / end transaction
blocking synchronous TX
fixed CubeMX config validation
STM32 SPI1 private HAL binding
```

当前没有：

```text
SPI read
full-duplex transfer
SPI DMA
SPI IRQ transfer
SPI mutex
runtime mode / clock switching
```

这些只有出现真实需求后才扩展。

---

# 8. Current Architecture Direction

Display 正式链目标：

```text
ST7789 Driver
    ↓
Platform SPI + Platform GPIO + Platform delay/time
    ↓
STM32 Impl
    ↓
HAL / SPI1 / GPIO
```

正式 ST7789 Driver 不得直接依赖：

```text
SPI_HandleTypeDef
hspi1
HAL_SPI_Transmit()
CubeMX SPI internals
```

DC / RST / BL 属于 LCD 设备层，不进入 generic SPI。

---

# 9. Next Phase — Formal ST7789 Driver

下一阶段先讨论设计，不直接施工。

建议讨论顺序：

```text
1. ST7789 Driver 在 Platform BSP / device layer 的具体落点
2. 驱动对象模型与依赖注入
3. init / reset / backlight / command-data API
4. set_window / fill / fill_rect
5. 基础 text / number rendering 范围
6. delay / timeout / error propagation
7. reference vendor code 哪些只作为命令表与字体资源保留
8. Host test 边界
9. 正式 ST7789 path 的 target verification
```

这一步完成后再创建新的 Implementation Plan。

---

# 10. Later Display Integration Phases

正式 ST7789 Driver 之后再讨论：

```text
Display abstraction
Display data snapshot
Display Task 是否必要
Display Queue / overwrite / latest-value strategy
Acquisition -> Display data flow
UART periodic sensor TX 是否退出
STATUS / HELP / ACK 是否继续走 UART
ONCE completion semantic
partial refresh
text layout
```

不要在 ST7789 基础驱动阶段一次性冻结这些业务层问题。

---

# 11. Performance / Resource Constraints

STM32F411CEU6：

```text
Flash = 512 KiB
SRAM  = 128 KiB
```

LCD 全屏 RGB565 framebuffer：

```text
240 * 280 * 2 = 134400 B
```

因此当前工程不使用全屏 framebuffer。

优先：

```text
direct region update
small line/block buffer
partial refresh
```

SPI DMA 暂不加入；只有正式显示刷新暴露明显 CPU 占用或阻塞问题后再评估。

---

# 12. Current Stop Point

```text
Core Phase 1~9                         COMPLETE / TARGET VERIFIED
Display Hardware / CubeMX              COMPLETE
Minimal ST7789 Bring-up                TARGET VERIFIED
SPI Platform + STM32 Impl Phase 1      COMPLETE / HOST + KEIL VERIFIED
Current Active Implementation Plan     NONE
Next                                  FORMAL ST7789 DRIVER DESIGN DISCUSSION
```

不要重新做 LCD 最小 Bring-up，也不要直接从旧 SPI Phase 1 Implementation Plan 继续施工。
