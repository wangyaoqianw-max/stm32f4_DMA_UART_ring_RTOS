# 工程长期记忆与交接说明

更新时间：2026-09-05

> 本文件是 AI Agent / Codex 与人工开发者恢复工程上下文时的长期入口。  
> Phase 1~9 核心工程已经完成并通过 Host / Keil / Target 综合验证，作为稳定 Application Baseline 保留。  
> Display Extension 已完成硬件资源确认、CubeMX 配置和 ST7789T3 最小 Bring-up 目标板验证；临时测试代码已回退，SPI1 / LCD GPIO 的 CubeMX 配置保留。  
> 下一对话从“正式 LCD 驱动与显示数据流实现计划”开始，不重复做硬件点亮验证，也不要直接把临时 Bring-up 代码当作正式实现。  
> 显示阶段完成后，再以本工程的成熟架构与可复用模块为基底，新建独立 Bootloader + OTA 学习工程。

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

项目核心能力：

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

Display Extension 用于继续验证：

```text
既有五层架构如何扩展新的板级设备
显示设备应该落在哪一层
采集数据如何从 Acquisition 进入 Display
是否需要新的 Display Task / Queue
UART 产品输出职责是否需要收缩
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

固定依赖规则：

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

原 RTOS Task/Event 阶段与 Final APP Integration 已合并为 Phase 9。

不存在原规划中的独立 Phase 10；Phase 1~9 作为第一阶段核心工程正式结束。

旧 `00_Doc/04_Agent/implementation_plan.md` 属于已完成历史实施计划，不再作为新的施工入口。

---

# 4. Phase 1~9 最终产品行为

默认启动：

```text
APP state = STOPPED
LED = OFF
UART RX active
RTT active
no periodic acquisition
```

Button：

```text
SINGLE
STOPPED -> START -> RUNNING
 -> LED ON
 -> immediate first complete sample/report
 -> then every 2 s

LONG >= 3 s
RUNNING -> STOP -> STOPPED
 -> LED OFF
 -> stop future periodic sampling

DOUBLE
STOPPED -> SAMPLE_ONCE
 -> complete DHT20 + MPU6050 sample
 -> complete UART report TX success
 -> LED blink 3 times
 -> OFF
 -> remain STOPPED
```

UART commands：

```text
START\r\n
STOP\r\n
ONCE\r\n
STATUS\r\n
HELP\r\n
```

协议：

```text
strict CRLF
uppercase only
case-sensitive
no trim
no arguments
fixed-size storage
```

---

# 5. UART DMA + RingBuffer 稳定架构

RX：

```text
USART1 RX
 -> DMA2_Stream2 / Channel 4 / Circular
 -> IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform UART RX_DATA
 -> UART Service
 -> SPSC RingBuffer
 -> Communication Task
```

原则：

```text
single producer / single consumer
RingBuffer lock-free
no ordinary mutex
no second RX path
```

TX：

```text
Communication Task
 -> service_uart_write()
 -> platform_uart_write_async()
 -> STM32 UART Impl
 -> HAL_UART_Transmit_DMA()
 -> DMA2_Stream7 / Channel 4 / Normal
 -> TX complete callback
 -> Platform event
 -> UART Service ownerThread wake
```

原则：

```text
Communication Task = sole USART1 product TX requester
one active TX transaction
RX + TX simultaneously allowed
no TX RingBuffer / Queue / worker inside UART Service
service_uart_write() returns only after DMA no longer uses caller buffer
USART1 = product control/data
RTT = diagnostics
```

Display Extension 当前尚未冻结 UART 最终职责；“保留 UART RX 控制、LCD 接管传感器数据显示”是待正式设计的方向，不是已冻结合同。

---

# 6. 最终四任务模型

当前稳定 4 个产品 Task：

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

CubeMX `defaultTask` 在 USER CODE 区直接 `osThreadExit()`，不是第五个长期产品 Task。

Display Extension 是否增加 Display Task 尚未冻结，必须在下一阶段正式讨论后决定。

---

# 7. APP Control FSM

唯一业务状态：

```text
STOPPED
RUNNING
```

只有 Control Task 修改该状态。

`onceActive` / `onceSource` 是 operation context，不是第三业务状态。

统一控制事件：

```text
APP_CTRL_START
APP_CTRL_STOP
APP_CTRL_SAMPLE_ONCE
APP_CTRL_GET_STATUS
```

Button 与 UART 都映射到同一 FSM。

原则：

```text
输入源可以很多
业务状态真值只能有一个 owner
```

---

# 8. Unified Acquisition Service

位置：

```text
02_Service/service_acquisition/
```

职责：

```text
DHT20 read
 -> MPU6050 read
 -> one complete atomic acquisition result
```

不负责：

```text
2 s scheduling
START / STOP / ONCE
Task / Queue
UART
LED
shared I2C lifecycle
```

成功语义：

```text
DHT20 OK && MPU6050 OK -> complete acquisition success
otherwise             -> whole acquisition failed
```

采用 temporary result：两者都成功才 commit caller output，失败不产生 partial business result。

原则：

```text
Task decides WHEN
Service decides HOW
Platform decides HOW DEVICE IS ACCESSED
```

---

# 9. Acquisition Task 调度

Command：

```text
START_PERIODIC
STOP_PERIODIC
SAMPLE_ONCE
```

周期策略：

```text
STOPPED -> Queue WAIT_FOREVER
START -> immediate first sample
RUNNING -> queue_receive(timeout until absolute deadline)
nextDeadline += 2000 ms
```

STOP 到达正在执行的同步 Sensor transaction 时：

```text
不粗暴中断 Software I2C
finish low-level transaction safely
check pending STOP before periodic publish
STOP already observed -> discard stale periodic result
```

---

# 10. APP IPC 稳定基线

```text
Control Queue                  depth 8
Acquisition Command Queue      depth 4
Communication Outbound Queue   depth 8
Indicator Queue                depth 4
```

原则：

```text
bounded
value-copy
no temporary stack pointer
no infinite producer block
queue full observable
```

当前没有：

```text
APP state mutex
I2C mutex
Queue Set
Event Group as business event bus
runtime malloc/free for APP business data flow
```

Display Extension 很可能需要新的显示数据 IPC，但 Display Queue / snapshot 合同尚未冻结。

---

# 11. ONCE 当前事务语义

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
 -> blink 3 times
```

重要：Display Extension 若改变 UART 产品输出职责，必须重新讨论 ONCE completion semantic。

不能只把 `communicationQueue` 改成 `displayQueue` 就结束，因为当前成功 LED 明确依赖 `ONCE_TX_RESULT`。

待讨论选项包括但不限于：

```text
Acquisition success = ONCE complete
Display publish success = ONCE complete
Display refresh completion = ONCE complete
```

此项尚未冻结。

---

# 12. Phase 9 验证闭环

```text
Host regression : PASS / 34 of 34 test groups
Keil rebuild    : PASS / 0 Error(s)
Target test     : PASS
```

目标板综合验证包括：

```text
boot STOPPED / LED OFF
Button START / STOP / ONCE
UART START / STOP / ONCE / STATUS / HELP
immediate first acquisition
2 s periodic acquisition/report
DHT20 + MPU6050 sequential shared Software I2C
UART TX DMA
UART RX while TX active
ONCE success feedback
```

2026-09-05 已确认 Phase 9 目标板综合功能正常。

---

# 13. Display Extension 当前进度

## 13.1 屏幕与范围

当前显示模组：

```text
Module      : P169H002-CTP
Controller  : ST7789T3
Resolution  : 240 x 280
Interface   : 4-wire SPI display path
Pixel       : RGB565 / 2 bytes per pixel
Touch       : NOT IN CURRENT STAGE
```

本阶段暂不接入：

```text
TP_INT
TP_RST
TP_SCL
TP_SDA
CST816 / CTP driver
```

## 13.2 已确认板级引脚

```text
PA1  -> LCD_BL
PA4  -> LCD_CS
PA5  -> SPI1_SCK
PA6  -> LCD_DC
PA7  -> SPI1_MOSI
PB10 -> LCD_RST
```

其中：

```text
PA4 = software CS GPIO
PA6 = DC GPIO，不使用 SPI1_MISO
PA1 = GPIO backlight for first stage
```

## 13.3 CubeMX 已保留配置

SPI1：

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

现有 USART1 DMA 保持：

```text
DMA2 Stream2 -> USART1_RX / Circular
DMA2 Stream7 -> USART1_TX / Normal
```

LCD GPIO 默认：

```text
LCD_CS   PA4   HIGH   Push-Pull   High Speed
LCD_DC   PA6   HIGH   Push-Pull   High Speed
LCD_RST  PB10  HIGH   Push-Pull   Low Speed
LCD_BL   PA1   LOW    Push-Pull   Low Speed
```

CubeMX 已生成并保留 `spi.c/.h`，HAL SPI module 已启用，Keil 工程已纳入 `spi.c` 与 `stm32f4xx_hal_spi.c`。

## 13.4 最小 Bring-up Target Verification

2026-09-05 已完成最小 ST7789T3 Bring-up 测试。

测试范围仅包含：

```text
GPIO control
hardware reset
blocking HAL_SPI_Transmit()
ST7789 initialization
set window
RGB565 full-screen fill
backlight polarity
BLACK / WHITE / RED / GREEN / BLUE
```

人工目标板现象：

```text
前段纯黑：PA1 Low，背光关闭
稍亮黑色后依次 WHITE / RED / GREEN / BLUE：PA1 High，背光开启
最后纯黑：测试结束 PA1 恢复 Low
```

确认结果：

```text
LCD_BACKLIGHT_ON_LEVEL  = HIGH
LCD_BACKLIGHT_OFF_LEVEL = LOW

ST7789T3 initialization       PASS
SPI1 Mode 3 @ 12.5 MHz       PASS
240 x 280 display area       PASS
X_OFFSET = 0                 PASS
Y_OFFSET = 20                PASS
RGB565 high-byte first       PASS
BLACK / WHITE / RED / GREEN / BLUE PASS
20 px offset error           NOT OBSERVED
flower screen / unstable     NOT OBSERVED
```

因此：

```text
LCD Minimal Bring-up Target Verification = PASS
```

这些属于已经目标板确认的硬件事实，可以作为正式实现的输入条件。

## 13.5 Bring-up 测试代码状态

临时最小点亮测试代码已经人工回退。

当前仓库保留：

```text
CubeMX SPI1 configuration
LCD GPIO configuration
spi.c / spi.h generated support
HAL SPI support
Phase 1~9 stable application code
```

不保留临时：

```text
pure-color test loop
bring-up-only ST7789 code
manual BL test sequence
```

正式 LCD 驱动必须重新按项目分层和接口设计实施，不直接把临时实验代码原样塞回工程。

---

# 14. Display Extension 已确认与未确认边界

已经确认，可视为 Target-Verified Hardware Contract：

```text
P169H002-CTP / ST7789T3
240 x 280
PA1/PA4/PA5/PA6/PA7/PB10 mapping
SPI1 Mode 3
12.5 MHz first-stage clock
blocking HAL_SPI_Transmit() is viable
software CS
RGB565
X_OFFSET 0
Y_OFFSET 20
BL High = ON
BL Low  = OFF
```

当前不要提前冻结：

```text
正式 ST7789 driver 文件边界
是否立刻增加 generic platform_spi
是否新增 Display Task
Display Queue depth / overwrite strategy
Display snapshot data contract
字体资源组织
局部刷新策略
UART TX 是否全部退出产品输出
STATUS / HELP / OK 是否继续走 UART
ONCE completion semantic
是否需要 SPI DMA
是否需要 backlight PWM
```

下一阶段应逐项讨论、验证、再冻结。

---

# 15. 当前资源基线

Phase 9 最终 Keil MAP：

```text
Total RO Size   = 55808 B  / 54.50 KiB
Total RW Size   = 45616 B  / 44.55 KiB
Total ROM Size  = 55876 B  / 54.57 KiB
```

STM32F411CEU6：

```text
Flash = 512 KiB
SRAM  = 128 KiB
```

约：

```text
Flash usage ≈ 10.7%
RAM linked usage ≈ 34.8%
```

明显 RAM 大项：

```text
EasyLogger async buffer / elog_async.o ZI = 20480 B
FreeRTOS heap_4 ucHeap                  = 15360 B
```

Display 约束：

```text
240 * 280 * 2 = 134400 B
```

因此 STM32F411CEU6 不应建立全屏 RGB565 framebuffer；正式实现优先考虑小块 buffer / line buffer / direct region update。

SPI DMA 当前没有必要因为“可能更快”就提前加入，应在正式显示刷新出现实际阻塞或 CPU 占用问题后再讨论。

---

# 16. 可复用经验基线

优先复用：

```text
代码级
- RingBuffer
- platform_common
- platform_os
- 部分 platform_mcu

能力级
- UART Service
- Log interface / backend binding
- UART + DMA + RingBuffer 异步字节流基础设施

设计模式级
- Unified Acquisition Service
- Control FSM
- Composition Root
- Data / Context / Statistics
- Task / Service responsibility split
```

重要工程原则：

```text
Task decides WHEN
Service decides HOW
Platform decides HOW DEVICE IS ACCESSED

Queue submission success != business execution success
唯一 owner 优先于到处加 mutex
ISR 只做 capture/event，复杂业务放 Task
absolute deadline 优于简单 delay(period)
```

---

# 17. 下一对话的正式入口：Display Implementation Planning

下一对话不要重新讨论“屏幕能不能点亮”；该问题已经 Target Verified。

建议从以下顺序继续：

```text
1. 正式 ST7789 驱动在五层架构中的落点
   - Platform BSP 是否直接作为设备能力层
   - Vendor reference code 只保留哪些数据/资源
   - 是否暂缓 generic platform_spi

2. 正式 LCD 基础 API
   - init
   - backlight
   - set_window
   - fill_rect / fill
   - ASCII / number 基础绘制
   - error / timeout contract

3. 字体与绘图资源
   - lcdfont.h 是否直接使用、裁剪或重新组织
   - 是否只保留当前页面需要的 ASCII
   - buffer 大小和 Flash/RAM 权衡

4. Display APP / Task 模型
   - 是否新增 Display Task
   - Display Queue / latest-value / overwrite 策略
   - Acquisition -> Display 数据快照合同
   - Display Task 优先级 / stack

5. UART 产品输出职责调整
   - UART RX 控制是否保留
   - 周期 sensor report 是否停止 TX
   - STATUS / HELP / ACK 是否继续保留
   - RTT 继续作为 diagnostics

6. ONCE 语义重新定义
   - 当前 ONCE 与 UART TX success 强耦合
   - 显示接入后重新定义事务完成条件

7. 性能优化最后讨论
   - SPI DMA
   - partial refresh
   - de-dup
   - backlight PWM
```

只有前述设计讨论收束后，才创建新的 Implementation Plan；不要复用旧 Phase 1~9 implementation plan。

---

# 18. 推荐恢复资料

新对话优先读取：

```text
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/architecture.md
00_Doc/02_架构设计/P169H200屏幕参考文件/P169H002_ST7789显示接入使用文档.md
00_Doc/02_架构设计/P169H200屏幕参考文件/P169H002-CTP NEW 规格书SPEC.md
05_Vendors/lcd/
RTT_elog_DMA_UART_ring_project.ioc
Core/Src/spi.c
Core/Src/gpio.c
Core/Inc/main.h
01_APP/app_system.c
01_APP/app_acquisition.*
01_APP/app_control.c
01_APP/app_ipc_types.h
```

需要参考原稳定 APP 设计时再读取：

```text
00_Doc/00_项目需求/最终功能需求.md
00_Doc/02_架构设计/Final_RTOS_Application_Integration_Phase9设计.md
00_Doc/02_架构设计/UART_Application_Communication_Phase8设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
```

---

# 19. 当前停止点

```text
Phase 1~9 core application:
COMPLETE / BASELINE FROZEN / TARGET VERIFIED

Display Extension - hardware resource review:
PASS

Display Extension - CubeMX SPI1 + GPIO:
PASS / GENERATED / RETAINED

Display Extension - minimal ST7789 Bring-up:
TARGET VERIFIED PASS
TEMPORARY TEST CODE REVERTED

Display Extension - formal driver architecture:
NOT DESIGNED / NEXT DISCUSSION

Display Extension - Display Task / IPC:
NOT DESIGNED

Display Extension - UART output responsibility migration:
NOT DESIGNED

Display Extension - formal Implementation Plan:
NOT CREATED

Touch / CTP:
OUT OF CURRENT STAGE

Optional runtime resource measurement:
PENDING

Planned following project:
Bootloader + OTA / SEPARATE PROJECT
```

下一对话的任务不是继续做实验代码，而是：

```text
从已经 Target-Verified 的 LCD 硬件合同出发
 -> 讨论正式驱动落层与 API
 -> 讨论 Display Task / IPC / UART / ONCE 业务变化
 -> 分阶段冻结设计
 -> 形成新的 Implementation Plan
 -> 再交给 Codex 正式实施
```

当前 Phase 1~9 基线除修复缺陷外原则上保持稳定；Display Extension 作为独立增量阶段推进，不把所有后续设计一次性卡死。
