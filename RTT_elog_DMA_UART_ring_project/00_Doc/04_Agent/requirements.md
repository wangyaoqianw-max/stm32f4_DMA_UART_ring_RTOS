# 工程需求说明

更新时间：2026-09-06

验收状态：

```text
Implementation            COMPLETE
Host                       PASS 40/40
Keil                       PASS / 0 errors
Target                     PENDING MANUAL BOARD TEST
Failure isolation target   PENDING MANUAL BOARD TEST
Resource observation       PENDING MANUAL BOARD TEST
```

---

# 1. 项目目标

本项目用于建立一个工程化 STM32F411 + FreeRTOS 多任务采集系统，核心能力包括：

```text
UART DMA RX / TX
RingBuffer
Button / UART unified control
DHT20 + MPU6050 unified acquisition
RTOS task ownership / IPC
LED semantic indication
ST7789 product display
Layered architecture
```

项目不是单纯的“串口 + DMA + 环形缓冲”实验，而是一个用于学习和验证可复用嵌入式软件架构、任务边界和数据流设计的完整工程。

---

# 2. 硬件与运行环境

MCU：

```text
STM32F411CEU6
Cortex-M4F
Flash 512 KiB
SRAM 128 KiB
```

RTOS / Framework：

```text
FreeRTOS / CMSIS-RTOS2
STM32 HAL
RTT + EasyLogger
```

主要设备：

```text
USART1
DHT20
MPU6050
Status LED
User Button
P169H002-CTP / ST7789T3 LCD
```

LCD：

```text
240 x 280
RGB565
4-wire SPI display path
SPI1 Mode 3 @ 12.5 MHz
Touch deferred
```

---

# 3. 分层需求

固定架构：

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

APP 不得直接依赖 HAL handle 或 STM32-specific Impl API。

---

# 4. 静态资源要求

禁止 runtime malloc/free。

要求：

```text
static object ownership
fixed-size Queue
copy-by-value IPC
no stack-pointer enqueue
no full-screen framebuffer
```

240 x 280 RGB565 framebuffer 大于 128 KiB SRAM，因此 LCD 必须使用小型固定 scratch buffer + chunked write。

---

# 5. UART 通信需求

USART1 RX：

```text
DMA Circular
IDLE / HT / TC
Platform UART
UART Service
SPSC RingBuffer
Communication Task
```

UART 命令：

```text
START
STOP
ONCE
STATUS
HELP
```

协议保持 strict CRLF。

UART 必须保留：

```text
command receive
command parsing
control request submit
control response TX
local HELP / parser error response
DMA RX / RingBuffer
TX DMA
```

Display Integration 后 UART 不再发送：

```text
periodic sensor measurement report
ONCE sensor measurement report
```

新增成功响应：

```text
OK ONCE\r\n
```

---

# 6. APP Control FSM 需求

唯一业务状态：

```text
STOPPED
RUNNING
```

唯一拥有者：

```text
Control Task
```

Button：

```text
SINGLE -> START
DOUBLE -> ONCE
LONG   -> STOP
```

UART：

```text
START -> START
STOP -> STOP
ONCE -> ONCE
STATUS -> GET_STATUS
HELP -> Communication local
```

ONCE 期间可使用：

```text
onceActive
onceSource
```

但不得引入第三业务状态。

---

# 7. Acquisition 需求

Unified Acquisition Service 必须：

```text
read DHT20
read MPU6050
return one atomic complete result
```

成功条件：

```text
DHT20 OK && MPU6050 OK
```

Acquisition Task 必须：

```text
2 s absolute-deadline periodic scheduling
START immediate first sample
STOP suppress stale periodic result
ONCE only in STOPPED
sole sensor / shared-I2C runtime access
```

不得直接控制：

```text
UART
LED
ST7789 / Graphics
```

---

# 8. ONCE 语义需求

ONCE 成功必须严格定义为：

```text
complete dual-sensor acquisition success
```

不得依赖：

```text
UART TX success
Display Queue success
LCD render success
future output path success
```

Indicator `ONCE_SUCCESS` 必须只表示采集成功。

ONCE completion 通过 Control Queue：

```text
APP_CONTROL_MESSAGE_ONCE_COMPLETE(result)
```

该 completion 必须可靠提交，以保证 `onceActive` 最终释放。

---

# 9. Display Task 需求

新增永久第五个产品 Task：

```text
Display Task
```

它必须是：

```text
sole ST7789 / Graphics runtime owner
```

禁止其他产品 Task 直接调用 ST7789 / Graphics。

Display Task startup：

```text
platform_st7789_init
 -> Boot Page
 -> backlight ON
 -> 1000 ms dwell
 -> Main UI
 -> event-driven refresh
```

Display startup 不得作为全系统 startup gate。

---

# 10. Display UI 需求

Boot Page：

```text
SENSOR MONITOR
STM32F4 + RTOS
STARTING...
```

Main UI 至少显示：

```text
STATE: RUNNING / STOPPED
Temperature
Humidity
Accel X/Y/Z (g)
Gyro X/Y/Z (dps)
```

精度：

```text
Temperature   1 decimal
Humidity      1 decimal
Acceleration  3 decimals
Gyroscope     1 decimal
```

第一次有效 measurement 前显示：

```text
--
```

STOP 后必须保留最后一次有效 measurement。

运行期必须优先局部刷新，不允许每 2 s 无条件整屏 clear/redraw。

---

# 11. Display IPC 需求

Display Queue 消息：

```text
SYSTEM_STATE
MEASUREMENT
```

Producer：

```text
Control      -> SYSTEM_STATE
Acquisition  -> MEASUREMENT
```

Consumer：

```text
Display Task
```

Queue 必须 copy-by-value。

第一版 Depth：

```text
4
```

生产者使用 NO_WAIT；Display Queue failure 不得回滚业务状态或采集成功。

Display Task 必须采用 consumer-side coalescing：

```text
wait first message
 -> drain pending messages
 -> update latest presentation cache
 -> render latest state once
```

---

# 12. Display Failure Isolation 需求

Display 定义为 non-critical output subsystem。

LCD failure 不得导致：

```text
Control FSM failure
Acquisition failure result
UART shutdown
Indicator shutdown
whole-system Error_Handler
```

Startup init failure：

```text
log
BL OFF
available = false
no automatic retry
Display Task stays alive
continue draining queue
```

Runtime render failure：

```text
record failure
keep dirty flag
retry only when next Display event arrives
```

---

# 13. SPI Integration 需求

正式 Display Integration 必须保持：

```text
APP -> Platform -> Impl
```

APP 不得直接 include/call：

```text
impl_platform_spi.h
impl_platform_spi1_construct()
HAL_SPI_*
hspi1
```

必须通过 Platform 层提供：

```text
board-specific display SPI bus constructor
SPI Bus lifecycle facade
```

SPI Bus 可在 pre-scheduler `app_system_init()` 中 init/start；ST7789 physical init 必须在 Display Task 中完成。

---

# 14. Communication Simplification 需求

Display Integration 后 Communication 不再处理 sensor measurement payload。

应删除：

```text
periodic report formatting
ONCE report formatting
sensor report UART TX path
ONCE TX completion -> Control
```

Communication Response Queue item 可直接使用：

```text
app_control_response_t
```

Communication 继续保持 command/response channel，不删除 UART 模块能力。

---

# 15. Composition Root 需求

`app_system` 新增：

```text
Display SPI Bus
ST7789 object
Display Queue
app_display
Display Thread
```

pre-scheduler 只负责：

```text
construct objects
SPI Bus lifecycle
Queue creation
APP init
Thread creation
```

Display Task 负责：

```text
ST7789 init
Boot/Main UI
runtime render
```

失败 rollback 必须严格逆序。

---

# 16. Task / Queue Baseline

```text
Communication Task   2048 B   ABOVE_NORMAL
Control Task         1024 B   ABOVE_NORMAL
Acquisition Task     1536 B   NORMAL
Display Task         1536 B   NORMAL
Indicator Task        768 B   BELOW_NORMAL
```

```text
Control Queue                8
Acquisition Command Queue    4
Communication Response       8
Display Queue                4
Indicator Queue              4
```

资源值是 bring-up baseline；不得在当前迁移中基于猜测缩栈或缩 Queue。

---

# 17. 验收需求

Host：

```text
Display IPC validation
coalescing
initial --
state update
measurement update
STOP retains latest values
ONCE completion independent from Display publish
Communication no measurement formatting
rollback paths
architecture boundary checks
```

Keil：

```text
0 errors
no new warnings in new/modified production files
```

Target：

```text
Boot Page visible
Boot -> Main UI
initial STOPPED + --
START -> RUNNING + immediate measurement
2 s LCD refresh
STOP -> STOPPED + retained values
Button ONCE -> measurement update + LED 3 blinks
UART ONCE -> measurement update + OK ONCE + LED 3 blinks
STATUS / HELP / START / STOP retained
no ENV/IMU measurement reports to PC
LCD failure isolated from other subsystems
no obvious periodic whole-screen flicker
```

---

# 18. 当前不做

```text
Touch / CTP
SPI DMA
Display Service
Generic display abstraction
GUI widgets
Chinese / UTF-8
Backlight PWM
Sensor stale/error UI
Periodic Display recovery
Low-power optimization
Resource shrinking without evidence
```
