# RTOS Display Integration 设计

> 文档类型：Display Integration Design  
> 状态：DESIGN FROZEN  
> 日期：2026-09-06  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 设计目标

本阶段在既有 Phase 1~9 Core Application、SPI Platform + STM32 Impl Phase 1、ST7789 + Minimal Graphics Phase 1 的基础上，将 ST7789 正式接入 RTOS 产品运行链。

本阶段完成：

```text
Display Task / Display Queue
Boot -> Main UI
Display runtime ownership
measurement output migration: UART -> LCD
Control state -> LCD
ONCE semantic migration
Display failure isolation
app_system composition-root integration
SPI Bus board binding / lifecycle facade integration
Task / Queue resource baseline
```

本阶段不做：

```text
Display Service
Generic Display backend / surface
Touch / CTP
SPI DMA
Backlight PWM
Chinese / UTF-8
GUI widget framework
Full-screen framebuffer
Sensor stale/error UI
Display health recovery / periodic retry
Low-power optimization
```

---

# 2. 稳定分层与职责

固定依赖：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

Display 正式链：

```text
APP Display Task
    ↓
Minimal Graphics / Text
    ↓
Platform ST7789 Driver
    ↓
Platform SPI + GPIO + Time
    ↓
STM32 / FreeRTOS Impl
```

本阶段不增加 Display Service。

原因：当前屏幕只承担产品数据显示和状态呈现，不存在独立、可复用的业务服务能力；ST7789 与基础 Graphics 已属于 Platform，页面编排属于 APP。

---

# 3. 最终产品 Task Ownership

最终 5 个产品 Task：

| Task | 主要职责 | 初始 Stack | Priority |
| --- | --- | ---: | --- |
| Communication Task | UART RX parser、命令输入、控制响应 TX | 2048 B | ABOVE_NORMAL |
| Control Task | Button 10 ms polling、唯一 APP Control FSM | 1024 B | ABOVE_NORMAL |
| Acquisition Task | 2 s scheduling、ONCE、唯一 sensor/I2C runtime accessor | 1536 B | NORMAL |
| Display Task | ST7789 / Graphics sole runtime owner、Boot/Main UI | 1536 B | NORMAL |
| Indicator Task | LED semantic execution、ONCE 成功闪烁 | 768 B | BELOW_NORMAL |

冻结 ownership：

```text
Communication Task = sole USART1 runtime communication owner
Control Task       = sole APP state/FSM owner
Acquisition Task   = sole Sensor/I2C runtime owner
Display Task       = sole ST7789/Graphics runtime owner
Indicator Task     = sole LED semantic executor
```

禁止：

```text
Acquisition / Control / Communication 直接调用 ST7789 / Graphics API
Display Task 直接修改 Control FSM
Display Task 直接访问 Sensor / I2C
```

---

# 4. Display Task 生命周期

Display Task 只有两个生命周期阶段，不建立独立业务 FSM：

```text
Task Entry
  ↓
DISPLAY STARTUP
  ↓
MAIN UI
  ↓
wait Display Queue forever
```

启动流程：

```text
scheduler start
  ↓
Display Task Entry
  ↓
platform_st7789_init()
  ↓
clear / draw Boot Page
  ↓
backlight ON
  ↓
PROJECT_DISPLAY_BOOT_DURATION_MS
  ↓
draw Main UI static layout
  ↓
drain pending Display Queue
  ↓
render latest cache
  ↓
normal queue loop
```

`platform_st7789_init()` 只能在 Task Context 执行；`app_system_init()` 只负责静态对象、Queue、Thread 和 SPI Bus lifecycle 准备，不在 scheduler 前初始化 ST7789。

CubeMX `defaultTask` 继续只执行 `osThreadExit()`，不承担 display bootstrap。

---

# 5. Boot Page

第一版 Boot Page 只承担视觉启动提示，不作为系统 readiness gate，也不承担传感器自检语义。

暂定内容：

```text
SENSOR MONITOR

STM32F4 + RTOS

STARTING...
```

冻结规则：

```text
ST7789 init
 -> draw complete Boot Page while backlight OFF
 -> backlight ON
 -> dwell fixed duration
 -> Main UI
```

Baseline：

```c
#define PROJECT_DISPLAY_BOOT_DURATION_MS    (1000U)
```

禁止显示未经验证的：

```text
SYSTEM OK
SENSORS OK
ALL READY
```

启动页期间其他产品 Task 正常运行，不增加全局 startup gate。

---

# 6. Main UI 信息架构

第一版采用工程监视器风格，不做复杂 GUI。

240 x 280 + ASCII 8x16 下，逻辑容量约 30 列 x 17 行。

暂定主页面：

```text
SENSOR MONITOR

STATE : STOPPED

ENVIRONMENT
TEMP  : +23.4 C
HUM   :  45.6 %

ACCEL (g)
X     : +0.012
Y     : -0.034
Z     : +0.998

GYRO (dps)
X     : +12.3
Y     :  -0.8
Z     :  +1.2
```

显示工程量，不显示 raw sensor value。

精度：

```text
Temperature   1 decimal, C
Humidity      1 decimal, %
Acceleration  3 decimals, g
Gyroscope     1 decimal, dps
```

ASCII Phase 1 不支持 `°`，因此温度单位使用 `C`。

第一次有效采集前显示：

```text
--
```

不得使用 `0.0` 冒充“尚无有效数据”。

STOP 后保留最后一次有效测量值；只更新 STATE 为 STOPPED。

ONCE 在 STOPPED 下成功时：

```text
STATE remains STOPPED
measurement updates to latest ONCE data
```

第一版不显示：

```text
ONCE
SAMPLING
DATA STALE
SENSOR ERROR
last update timestamp
```

---

# 7. Main UI 刷新策略

首次进入 Main UI：

```text
draw static layout once
```

静态内容：

```text
标题
区域标题
字段名称
单位
```

动态内容：

```text
RUNNING / STOPPED
Temperature
Humidity
Accel X/Y/Z
Gyro X/Y/Z
```

运行期只局部刷新动态区域，不每 2 s 清整屏重画。

动态 value region 建议：

```text
fill_rect(background)
 -> draw_string(new value)
```

避免较短字符串覆盖旧字符串后残留尾部字符。

---

# 8. Display IPC

Display Queue 使用现有 `platform_queue_t`，copy-by-value，不增加新的 RTOS Queue overwrite API。

消息：

```c
typedef enum
{
    APP_DISPLAY_MESSAGE_SYSTEM_STATE = 0,
    APP_DISPLAY_MESSAGE_MEASUREMENT,
    APP_DISPLAY_MESSAGE_MAX
} app_display_message_type_t;

typedef struct
{
    app_display_message_type_t type;
    union
    {
        app_control_state_t systemState;
        app_acquisition_data_t measurement;
    } payload;
} app_display_message_t;
```

Producer / Consumer：

| Message | Producer | Consumer |
| --- | --- | --- |
| SYSTEM_STATE | Control Task | Display Task |
| MEASUREMENT | Acquisition Task | Display Task |

所有 producer 使用 non-blocking send：

```text
PLATFORM_OS_NO_WAIT
```

Display Queue 属于输出链；Queue 满不得回滚业务状态或采集成功。

Baseline：

```c
#define PROJECT_DISPLAY_QUEUE_DEPTH    (4U)
```

---

# 9. Display presentation cache

Display 不保存第二份业务真值，只保存 presentation cache。

建议 context：

```c
typedef struct
{
    platform_bool_t initialized;
    platform_bool_t available;

    app_control_state_t systemState;
    platform_bool_t systemStateValid;

    app_acquisition_data_t latestMeasurement;
    platform_bool_t measurementValid;

    platform_bool_t stateDirty;
    platform_bool_t measurementDirty;
} app_display_context_t;
```

唯一业务状态真值仍然是：

```text
Control FSM / app_control.context.state
```

Display 中的 `systemState` 只是显示副本。

---

# 10. Latest-state rendering / Queue coalescing

Display Queue 本身仍是 bounded FIFO，不定义为 overwrite queue。

Display Task 的消费策略：

```text
WAIT_FOREVER receive first message
  ↓
update cache + dirty flag
  ↓
NO_WAIT drain all currently pending messages
  ↓
coalesce into latest cache
  ↓
render final latest state once
```

例如：

```text
STATE STOPPED
MEASUREMENT A
STATE RUNNING
MEASUREMENT B
MEASUREMENT C
```

最终只需要渲染：

```text
state       = RUNNING
measurement = C
```

这是一种 latest-state rendering/coalescing 策略，不保证 Queue 满时最新消息一定保留。

当前 2 s 采集周期和低频状态变化下，Depth 4 足够作为 bring-up baseline。

---

# 11. Control State -> Display

Control Task 是 STOPPED/RUNNING 唯一状态拥有者。

Control 在：

```text
Task first entry
START success
STOP success
```

best-effort 向 Display Queue 发布：

```text
SYSTEM_STATE(STOPPED/RUNNING)
```

Display Queue 投递失败：

```text
Control state transition remains valid
UART response remains valid
Indicator behavior remains valid
Display may temporarily show stale state
```

初始 STOPPED 不由 Display 硬编码；Control 首次运行时发布初始 STOPPED。

---

# 12. Measurement Output Migration

Phase 9 旧数据链：

```text
Acquisition
 -> Communication Outbound Queue
 -> Communication Task
 -> UART report
```

本阶段迁移为：

```text
Acquisition
 -> Display Queue / MEASUREMENT
 -> Display Task
 -> ST7789
```

UART 模块功能完整保留，只是不再向 PC 发送周期/ONCE sensor measurement。

UART 保留：

```text
DMA Circular RX
IDLE / HT / TC
RingBuffer
strict CRLF parser
START / STOP / ONCE / STATUS / HELP
Control request submit
Control response TX
local parser responses
```

产品职责调整：

```text
UART = command / response / debug communication channel
LCD  = measurement presentation / product data display channel
```

`app_acquisition` 不再直接依赖 Communication。

---

# 13. ONCE Semantic Migration

新的 ONCE 成功定义：

```text
DHT20 success
AND
MPU6050 success
```

即完整 Unified Acquisition 成功。

ONCE 成功不再依赖：

```text
UART TX
Display Queue send
LCD render
future storage / Bluetooth / CAN / Modbus output
```

Indicator `ONCE_SUCCESS` 的语义固定为：

> 本次完整数据采集成功。

不是“所有输出链路成功”。

Control Queue 统一 completion：

```c
APP_CONTROL_MESSAGE_ONCE_COMPLETE
```

携带：

```c
platform_error_t result;
```

语义：

```text
result == PLATFORM_ERR_OK
    -> onceActive = false
    -> Indicator ONCE_SUCCESS
    -> UART source: OK ONCE

result != PLATFORM_ERR_OK
    -> onceActive = false
    -> no success blink
    -> UART source: ERR ACQUISITION_FAILED
```

删除旧：

```text
APP_CONTROL_MESSAGE_ONCE_ACQUISITION_FAILED
APP_CONTROL_MESSAGE_ONCE_TX_RESULT
```

ONCE completion 属于 Control FSM 控制面消息，不是 telemetry；Acquisition 向 Control Queue 提交 completion 使用可靠阻塞语义：

```text
PLATFORM_OS_WAIT_FOREVER
```

以保证已接受的 ONCE 最终释放 `onceActive`。

Display measurement publish 仍为 best-effort NO_WAIT，与 ONCE completion 相互独立。

---

# 14. UART ONCE 响应

Phase 9 中 UART `ONCE` 成功时，sensor report 本身充当成功结果。

本阶段 measurement 不再发往 PC，因此新增：

```text
OK ONCE\r\n
```

Control response 增加：

```c
APP_CONTROL_RESPONSE_OK_ONCE
```

保留：

```text
ERR ACQUISITION_FAILED\r\n
ERR BUSY\r\n
ERR ALREADY_RUNNING\r\n
```

Button 来源的 ONCE 不发送 UART response。

---

# 15. Communication Outbound Queue 精简

Measurement migration 后 Communication Outbound Queue 只剩 Control response。

因此删除：

```text
APP_COMM_OUTBOUND_PERIODIC_REPORT
APP_COMM_OUTBOUND_ONCE_REPORT
app_communication_outbound_type_t
app_communication_outbound_message_t
```

Communication Response Queue item 直接采用：

```c
app_control_response_t
```

Communication 删除旧 sensor report formatting / TX / ONCE TX completion 路径，包括：

```text
app_communication_format_report()
app_communication_send_report()
app_communication_submit_once_result()
reportCount
reportFailureCount
onceCompletionSubmitFailureCount
APP_COMM_ENV_REPORT_BUFFER_SIZE
APP_COMM_IMU_REPORT_BUFFER_SIZE
```

Communication config 删除：

```text
controlQueue
```

因为 Communication 不再生产 ONCE completion。

Communication 最终只负责：

```text
RX command adapter
TX control-response adapter
UART local HELP / parser error responses
```

---

# 16. Shared APP Control Types

`app_control_state_t` 既被 Control FSM 使用，也被 Display IPC 使用。

因此从 `app_control.h` 移至：

```text
01_APP/app_control_types.h
```

该文件继续作为 APP 层共享控制语义类型的稳定位置。

Display 只读取状态 snapshot，不拥有业务状态。

---

# 17. Display Failure Isolation

Display 定义为 non-critical output subsystem。

LCD failure 不得：

```text
stop Acquisition
stop Control
stop Communication
stop Indicator
change APP FSM
change acquisition success result
```

## 17.1 Startup init failure

```text
platform_st7789_init() FAILED
 -> log error
 -> backlight remains OFF
 -> available = false
 -> no automatic retry
 -> Display Task remains alive
 -> continue draining Display Queue and updating cache
```

## 17.2 Boot/Main static drawing failure

```text
backlight OFF best-effort
platform_st7789_deinit() best-effort
available = false
continue degraded queue consumer
```

## 17.3 Runtime dynamic render failure

```text
renderFailureCount++
log WARN/ERROR
keep dirty flag = true
available remains true
next Display event retries latest cache
```

不加入 100 ms/1 s 周期性主动重试，避免故障屏幕形成永久忙循环。

---

# 18. Display Cache / Dirty 规则

消息处理：

```text
SYSTEM_STATE
 -> update systemState
 -> systemStateValid = true
 -> stateDirty = true

MEASUREMENT
 -> update latestMeasurement
 -> measurementValid = true
 -> measurementDirty = true
```

刷新成功后：

```text
stateDirty = false
measurementDirty = false
```

刷新失败：

```text
对应 dirty flag 保持 true
```

Display unavailable 时仍消费 Queue、维护 cache，但不执行 ST7789 / Graphics 操作。

---

# 19. Composition Root 集成

`app_system.c` 新增静态资源：

```text
g_displaySpiBus
g_display
g_displayQueue
g_appDisplay
g_displayThread
```

不增加 Display Service。

正式依赖：

```text
app_system
  ↓
Platform BSP SPI board binding
  ↓
STM32 SPI Impl
```

禁止 `01_APP/app_system.c` 直接 include / 调用 `impl_platform_spi*`。

SPI Phase 1 当前已有 Impl constructor 和 lifecycle ops；正式集成补齐：

```text
Platform BSP SPI1 display-bus constructor
Platform SPI Bus lifecycle facade
```

例如：

```c
platform_bsp_spi_construct_display_bus(...)
platform_spi_bus_init(...)
platform_spi_bus_start(...)
platform_spi_bus_stop(...)
platform_spi_bus_deinit(...)
```

准确命名实施时沿用现有工程风格；不得让 APP 直接操作 `bus->device.lifecycle->...`。

---

# 20. app_system 初始化顺序

冻结高层顺序：

```text
1. Static BSP construct
   UART / LED / Button / Soft-I2C GPIO
   Display SPI Bus
   ST7789 display

2. Platform lifecycle
   SPI Bus init/start
   Software I2C
   DHT20 / MPU6050
   LED / Button

3. Service init
   UART / Button / Indicator / Acquisition

4. Queue create
   Control
   Acquisition
   Communication Response
   Indicator
   Display

5. APP init
   Control
   Acquisition
   Communication
   Indicator
   Display

6. Thread create
   Communication
   Control
   Acquisition
   Indicator
   Display
```

Thread 均在 scheduler 启动前创建，创建顺序不作为业务启动顺序合同。

Display Thread 可作为最后一个产品线程创建，以最小化 Phase 9 现有结构改动。

---

# 21. app_display_init() 边界

`app_display_init()` 只允许：

```text
validate dependencies
bind ST7789 object
bind SPI Bus
bind Display Queue
initialize presentation cache
initialized = true
available = false
```

禁止在 `app_display_init()` 中：

```text
platform_st7789_init()
backlight ON
boot page draw
osDelay
```

物理显示器启动统一属于 `app_display_task_entry()`。

---

# 22. SPI Bus 生命周期

SPI Bus lifecycle `init/start` 当前不需要 Task-only delay，因此允许在 `app_system_init()` pre-scheduler 阶段完成。

目标状态：

```text
app_system_init complete
 -> SPI Bus STARTED
 -> ST7789 still uninitialized
 -> backlight OFF
```

scheduler start 后：

```text
Display Task
 -> platform_st7789_init()
```

ST7789 deinit 不负责 stop/deinit SPI Bus。

---

# 23. Rollback

`app_system_init()` 失败时继续使用严格逆序 rollback。

新增资源逆序：

```text
terminate Display Thread
...
delete Display Queue
...
stop SPI Bus
SPI Bus deinit
reset Display/SPI static storage
```

由于 scheduler 尚未运行，`app_system_init()` 失败 rollback 时 Display Task 不可能执行过 `platform_st7789_init()`，因此此路径不需要 ST7789 runtime deinit。

未来若增加 runtime `app_system_deinit()`，顺序必须是：

```text
stop Display Task
 -> platform_st7789_deinit()
 -> SPI Bus stop
 -> SPI Bus deinit
```

---

# 24. Final APP IPC / Data Flow

控制链：

```text
UART RX
 -> Communication Task
 -> CONTROL_REQUEST
 -> Control Queue
 -> Control Task

Button
 -> Control Task
```

状态输出：

```text
Control Task
 -> Display Queue / SYSTEM_STATE
 -> Display Task
```

周期采集：

```text
Control
 -> Acquisition Command Queue
 -> Acquisition Task
 -> Unified Acquisition Service
 -> Display Queue / MEASUREMENT
 -> Display Task
 -> ST7789
```

ONCE：

```text
Button / UART
 -> Control
 -> Acquisition
 -> complete dual-sensor sample

success:
    -> Display Queue / MEASUREMENT   best-effort
    -> Control Queue / ONCE_COMPLETE(OK) reliable
    -> Control clears onceActive
    -> Indicator ONCE_SUCCESS
    -> UART source: OK ONCE

failure:
    -> Control Queue / ONCE_COMPLETE(error) reliable
    -> Control clears onceActive
    -> no success blink
    -> UART source: ERR ACQUISITION_FAILED
```

UART product output：

```text
Control Task
 -> Communication Response Queue
 -> Communication Task
 -> UART
```

Measurement 数据不再进入 Communication。

---

# 25. Static Resource Baseline

任务：

```text
Communication Task   2048 B   ABOVE_NORMAL
Control Task         1024 B   ABOVE_NORMAL
Acquisition Task     1536 B   NORMAL
Display Task         1536 B   NORMAL
Indicator Task        768 B   BELOW_NORMAL
```

Queue：

```text
Control Queue                8
Acquisition Command Queue    4
Communication Response       8
Display Queue                4
Indicator Queue              4
```

新增配置：

```c
#define PROJECT_DISPLAY_TASK_STACK_SIZE_BYTES    (1536U)
#define PROJECT_DISPLAY_TASK_PRIORITY            PLATFORM_THREAD_PRIORITY_NORMAL
#define PROJECT_DISPLAY_QUEUE_DEPTH              (4U)
#define PROJECT_DISPLAY_BOOT_DURATION_MS          (1000U)
```

Communication 删除浮点 sensor report formatting 后理论上可缩栈；Communication Response Queue 也可能缩小，但本阶段不做顺手优化。

冻结原则：

```text
complete migration
 -> Host / Keil / Target verification
 -> inspect stack high-water mark / Queue peak
 -> optimize only with evidence
```

---

# 26. 验收条件

Host / static verification 至少覆盖：

```text
Display IPC message validation
Display queue drain/coalescing
initial -- state
SYSTEM_STATE refresh
MEASUREMENT refresh
STOP retains latest measurement
ONCE completion independent from Display publish result
Communication no longer formats sensor report
Communication ONCE success response = OK ONCE
app_system rollback with new Display resources
APP does not depend on Impl SPI
```

Keil：

```text
0 errors
new/modified production files no new warnings
```

Target：

```text
Boot Page visible
Boot -> Main UI
initial STOPPED + -- values
START -> RUNNING + immediate first measurement
2 s measurement refresh
STOP -> STOPPED, last measurement retained
Button ONCE in STOPPED -> new measurement + LED 3 blinks
UART ONCE in STOPPED -> new measurement + OK ONCE + LED 3 blinks
UART no longer emits ENV/IMU periodic reports
UART START/STOP/STATUS/HELP retained
Display failure does not prevent UART/control/acquisition/indicator operation
no obvious whole-screen 2 s flicker
```

---

# 27. 冻结结论

本阶段正式冻结：

```text
Display Task is permanent fifth product Task
No Display Service
ST7789 / Graphics sole runtime owner = Display Task
Boot page is visual-only, no startup gate
Main UI shows STOPPED/RUNNING + latest valid DHT20/MPU6050 engineering values
Display Queue = SYSTEM_STATE + MEASUREMENT
bounded FIFO + consumer-side latest-state coalescing
partial dynamic-region refresh
UART retains full command/response/debug function, removes sensor measurement reports only
ONCE success = complete dual-sensor acquisition success
Indicator ONCE_SUCCESS = acquisition-path success
Display/UART failures do not redefine acquisition success
Communication exits sensor data plane
Acquisition no longer depends on Communication
Display is non-critical output subsystem
```

本设计冻结后，下一步进入独立实施计划并执行 Host / Keil / Target integration verification。
