# Embedded Acquisition + Display Extension Development Roadmap

> 文档类型：Development Phase Roadmap  
> 状态：CURRENT BASELINE  
> 更新时间：2026-09-06  
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

主要参考：

```text
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/requirements.md
00_Doc/02_架构设计/SPI_Platform_Impl_Phase1设计.md
00_Doc/02_架构设计/ST7789_Graphics_Phase1设计.md
```

当前没有 Active Implementation Plan；上一份 SPI Phase 1 计划已经完成并作为施工记录保留在：

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

---

# 3. Stable Core Capabilities

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

产品任务：

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

UART START / STOP / ONCE / STATUS / HELP
```

采集：

```text
START -> immediate first DHT20 + MPU6050 sample
then every 2 s by absolute deadline
```

当前 ONCE success：

```text
complete acquisition success
AND complete UART report TX success
```

Display 接入后重新讨论。

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

当前状态：

```text
Hardware Resource Review                  COMPLETE
CubeMX SPI1 + LCD GPIO                    COMPLETE
Minimal ST7789 Bring-up                   TARGET VERIFIED
Temporary Bring-up Code                   REVERTED
SPI Platform + STM32 Impl Phase 1         COMPLETE / HOST + KEIL VERIFIED
ST7789 + Minimal Graphics Phase 1 Design  FROZEN
ST7789 + Minimal Graphics Implementation  COMPLETE / HOST + KEIL VERIFIED
Display Task / IPC                        NOT DESIGNED
UART Product Output Migration             NOT DESIGNED
ONCE Semantic Migration                   NOT DESIGNED
Touch / CTP                               DEFERRED
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

目标板确认：

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

状态：COMPLETE。

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

只有出现真实需求后才扩展。

---

# 8. ST7789 + Minimal Graphics Phase 1 Design

状态：

```text
DESIGN FROZEN
```

正式文档：

```text
00_Doc/02_架构设计/ST7789_Graphics_Phase1设计.md
```

正式链：

```text
APP / future Bootstrap / Display Task
        ↓
Minimal Graphics / Text
        ↓
Platform ST7789 Driver
        ↓
Platform SPI + GPIO + Time
        ↓
STM32 / FreeRTOS Impl
```

当前不增加 Display Service，不做 generic display backend/surface。

原因：这是第一次正式接触 SPI TFT；先完成一块真实设备，再从后续更多屏幕经验中提炼通用复用层。

---

# 9. Frozen ST7789 Scope

资源模型：

```text
ST7789 owns:
    SPI Device descriptor
    CS/DC/RST/BL GPIO descriptors

SPI Bus:
    shared non-owning dependency
```

Lifecycle：

```text
BSP construct
 -> static binding only

ST7789 init
 -> Task Context only
 -> GPIO configure
 -> SPI Device init
 -> hardware reset
 -> table-driven controller init
 -> READY
 -> backlight remains OFF
```

关键 init 规则：

```text
0x11 Sleep Out -> transaction end -> delay 120 ms
0x29 Display On -> init sequence end
0x2C RAMWR not part of init
```

Region write：

```text
one region = one SPI transaction
CASET -> RASET -> RAMWR -> pixel chunks
```

坐标：

```text
x / y / width / height
strict bounds
no clipping
logical X 0..239
logical Y 0..279
internal Y offset +20
```

Pixel：

```text
uint16_t RGB565 public form
high-byte-first SPI wire form
small fixed scratch buffer
chunked transfer
no full framebuffer
```

公共 drawing：

```text
draw_pixel
fill
fill_rect
write_rgb565
```

---

# 10. Frozen Minimal Graphics Scope

Phase 1：

```text
ASCII 8x16 only
printable ASCII 0x20..0x7E
opaque foreground/background
draw_char
draw_string
```

Vendor `lcdfont.h` 只作为字模来源，正式提取独立资源。

当前不做：

```text
Chinese / UTF-8
transparent text
alignment / wrap / clipping
number / float formatting API
printf wrapper
GUI / widget
```

业务值格式化由 APP / future Display logic 负责。

---

# 11. ST7789 Phase Verification Strategy

本阶段要求：

```text
focused Host tests
full Host regression
Keil rebuild
```

重点：

```text
BSP construct
reset / delay
init command order
Sleep Out delay
no RAMWR in init
rollback
transaction cleanup
CASET / RASET / RAMWR
offset
bounds
RGB565 endian
chunk write
font expansion
string x advance
```

本阶段不单独做 Target Verification：

```text
Formal ST7789 Target Verification
= DEFERRED / MERGED INTO RTOS DISPLAY INTEGRATION
```

Minimal Bring-up 已证明物理链路可用；后续系统级集成自然验证正式 Driver + Graphics。

---

# 12. ST7789 + Minimal Graphics Phase 1 Implementation

状态：COMPLETE / HOST + KEIL VERIFIED。

已完成：

```text
ST7789 Platform concrete driver / BSP construct
CS / DC / RST / BL owned descriptor lifecycle
logical 240 x 280 / physical X+0, Y+20 mapping
RGB565 high-byte-first / fixed 256-byte scratch chunks
printable ASCII 0x20..0x7E / opaque 8x16 Graphics
focused Host tests PASS
full Host regression PASS / 38 groups
Keil rebuild PASS / 0 errors / 13 pre-existing warnings
Coding Standard Review PASS
```

产品级显示静态参数集中在：

```text
00_Config/project_config.h
```

本阶段没有执行 standalone Target Verification：

```text
DEFERRED / MERGED INTO RTOS DISPLAY INTEGRATION
```

不要在本阶段补入 RTOS Display Task / IPC / UART migration / ONCE migration。

---

# 13. Current Next — RTOS Display Integration Design

下一阶段只进入设计，不直接实施。需要讨论：

```text
bootstrap/defaultTask role
startup gate
boot screen content/layout
Display Task ownership
Display Queue / snapshot / latest-value strategy
Acquisition -> Display data flow
main screen content/layout
partial refresh policy
UART periodic sensor TX 是否退出
STATUS / HELP / ACK 是否继续走 UART
ONCE completion semantic
```

未来希望支持类似手机/手表：

```text
scheduler start
 -> bootstrap
 -> ST7789 init
 -> boot screen
 -> backlight on
 -> startup diagnostics
 -> normal UI
```

该流程尚未集成，不要在当前 Phase 提前实现。

---

# 14. Performance / Resource Constraints

STM32F411CEU6：

```text
Flash = 512 KiB
SRAM  = 128 KiB
```

LCD full RGB565 framebuffer：

```text
240 * 280 * 2 = 134400 B
```

因此禁止 full framebuffer。

优先：

```text
direct region update
small fixed scratch buffer
partial refresh later if needed
```

SPI DMA 暂不加入；只有正式显示刷新暴露明显 CPU 占用或阻塞问题后再评估。

---

# 15. Current Stop Point

```text
Core Phase 1~9                         COMPLETE / TARGET VERIFIED
Display Hardware / CubeMX              COMPLETE
Minimal ST7789 Bring-up                TARGET VERIFIED
SPI Platform + STM32 Impl Phase 1      COMPLETE / HOST + KEIL VERIFIED
ST7789 + Minimal Graphics Design       FROZEN
ST7789 + Minimal Graphics Phase 1      COMPLETE / HOST + KEIL VERIFIED
Current Active Implementation Plan     NONE
Next                                   RTOS DISPLAY INTEGRATION DESIGN
```

不要重新做 LCD 最小 Bring-up、SPI Phase 1 或 ST7789 / Graphics Phase 1。
