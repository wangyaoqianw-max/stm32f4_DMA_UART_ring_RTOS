# 工程长期记忆与交接说明

更新时间：2026-09-03

> 本文件是 AI Agent 与人工开发者恢复工程上下文时的长期入口。
> 只保存长期目标、稳定架构合同、已验证基线、当前 Phase、当前计划状态和下一步。
> 详细业务需求以 `00_Doc/00_项目需求/最终功能需求.md` 为准。
> 整体阶段拆分以 `00_Doc/04_Agent/development_roadmap.md` 为准。
> 当前具体施工步骤只以 `00_Doc/04_Agent/implementation_plan.md` 为准。

---

# 1. 项目定位

工程根目录：

```text
RTT_elog_DMA_UART_ring_project/
```

目标环境：

```text
MCU        : STM32F411CEU6 / Cortex-M4F
Flash      : 512 KB
RAM        : 128 KB
UART       : USART1 / 115200 8N1
RTOS       : CMSIS-RTOS2 + FreeRTOS
Debug Log  : EasyLogger + SEGGER RTT
Sensors    : DHT20 + MPU6050
I2C        : Software I2C over GPIO
Input      : 1 x User Key
Indicator  : 1 x Status LED
```

最终目标：

> 在已验证 UART DMA + RingBuffer + FreeRTOS + 五层架构基础上，完成按键控制、Software I2C、DHT20、MPU6050、LED 状态反馈、UART 文本命令和 APP Control FSM，形成完整数据采集系统。

---

# 2. 稳定架构合同

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

CubeMX 生成文件只做初始化、Scheduler、IRQ / HAL Callback 和薄胶水；长期业务逻辑不得堆入生成文件。

APP Control FSM 是唯一 `STOPPED / RUNNING` 状态源。Button / UART 只是控制输入。

---

# 3. 已完成 / 已验证基线

```text
Platform UART / STM32 UART Impl              VERIFIED
UART DMA RX / TX                             VERIFIED
UART Service                                 VERIFIED
SPSC RingBuffer RX                           VERIFIED
APP Communication Phase 1                    VERIFIED
Platform OS                                  VERIFIED
Service Log -> Platform Log -> RTT            VERIFIED
Platform GPIO Phase 1                         VERIFIED
STM32 GPIO Impl + Board Binding               VERIFIED
Board GPIO Target Smoke                       VERIFIED
Software I2C Phase 3                          VERIFIED
LED Module Phase 4                            VERIFIED
```

真实板测已确认：

```text
PC13 Status LED                               PASS
PA0 User Key                                  PASS
PB6 / PB7 Software I2C electrical behavior    PASS
LED OFF / ON / 3 blink / final OFF            PASS
Indicator RTT sequence                        PASS
Existing UART communication regression        PASS
```

User Key 电气基线：

```text
PA0
Input / Pull-Up / no EXTI
released = HIGH
pressed  = LOW
```

---

# 4. 已冻结 UART / RingBuffer 合同

RX：

```text
USART1
 -> DMA Circular + IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform UART event
 -> UART Service
 -> SPSC RingBuffer
 -> Platform Notify From ISR
 -> APP Communication Task
```

RingBuffer：

```text
SPSC byte stream
caller-owned storage
usable capacity = storageSize - 1
no malloc/free
no silent overwrite
```

当前：

```text
Producer = UART Service RX callback
Consumer = APP Communication Task
```

新增应用命令必须复用该 RX 链，不建立第二套旁路。

---

# 5. ISR / Task 基线

ISR / HAL Callback 只允许：

```text
capture
copy necessary data
lightweight state update
ISR-safe notify
quick exit
```

禁止：

```text
blocking
ordinary mutex
malloc/free
full protocol parsing
Button debounce / gesture FSM
Software I2C transaction
Sensor business
LED blink delay
large formatted logging
```

Button Phase 5 使用 polling，不启用 EXTI。

---

# 6. LED Phase 4 冻结结果

专项设计：

```text
00_Doc/02_架构设计/LED_Phase1设计.md
```

冻结链：

```text
Indicator Service
    ↓
Platform LED
    ↓
Platform GPIO
    ↓
STM32 GPIO Impl
```

Platform LED：

```text
lightweight caller-owned object
no platform_device_t
no registry / manager
no impl_led
```

Indicator Service：

```text
SERVICE_INDICATOR_EVENT_STOPPED      -> OFF
SERVICE_INDICATOR_EVENT_RUNNING      -> ON
SERVICE_INDICATOR_EVENT_ONCE_SUCCESS -> blink 3 times -> OFF
```

配置：

```text
PROJECT_STATUS_LED_ACTIVE_LEVEL = LOW
blink count = 3
blink ON = 100 ms
blink OFF = 100 ms
```

最终系统独立 Indicator Task 的存在方向已冻结；永久 priority / stack / queue policy 留到 Phase 9。

---

# 7. Button Phase 5 正式设计

专项设计：

```text
00_Doc/02_架构设计/Button_Phase1设计.md
```

当前状态：

```text
COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

正式能力链：

```text
PA0 HIGH / LOW
    ↓
Platform GPIO
    ↓
Platform Button -> PRESSED / RELEASED
    ↓
Button Service -> SINGLE / DOUBLE / LONG
    ↓
Future APP -> START / SAMPLE_ONCE / STOP
```

## 7.1 Platform Button

第一版：

```text
caller-owned lightweight input device
owns one platform_gpio_t
activeLevel
pull
initialized
no malloc/free
no platform_device_t
no registry / manager
no impl_button
```

公共能力：

```text
platform_button_init
platform_button_read
platform_button_deinit
```

BSP：

```text
platform_bsp_button_construct_user_key()
 -> platform_bsp_gpio_construct_user_key()
```

静态配置：

```text
PROJECT_USER_KEY_ACTIVE_LEVEL = PLATFORM_GPIO_LEVEL_LOW
PROJECT_USER_KEY_PULL         = PLATFORM_GPIO_PULL_UP
```

## 7.2 Button Service

Service 不持有 Platform Button，不主动读取 GPIO，也不直接读取 OS Tick。

输入：

```text
platform_button_state_t buttonState
uint32_t nowMs
```

输出：

```text
NONE
SINGLE
DOUBLE
LONG
```

内部：

```text
raw state
 -> time-based debounce
 -> stable edge
 -> gesture FSM
```

时间配置：

```text
PROJECT_BUTTON_SAMPLE_PERIOD_MS = 10
PROJECT_BUTTON_DEBOUNCE_MS      = 30
PROJECT_BUTTON_DOUBLE_CLICK_MS  = 300
PROJECT_BUTTON_LONG_PRESS_MS    = 3000
```

核心规则：

```text
SINGLE waits until double window expires
second stable PRESS <= 300 ms -> DOUBLE candidate
LONG >= 3000 ms -> emit once immediately
LONG release -> no SINGLE
second press becomes LONG -> LONG only
uint32 elapsed checks use subtraction for wraparound
```

---

# 8. Button Host Test 基线

必须新增：

```text
Tests/platform_button/
Tests/platform_bsp_button/
Tests/service_button/
```

至少覆盖：

```text
active-low / active-high translation
User Key LOW active + PULL_UP composition
press / release bounce
single only
double only, no preceding SINGLE
2999 / 3000 ms LONG boundary
long hold only one LONG
LONG release no SINGLE
second-press LONG conflict
300 ms double boundary
expired second press preserved as new gesture
initial pressed behavior
irregular process intervals
uint32 wraparound
```

Host Service Test 不真实等待 3 s，只注入逻辑时间。

---

# 9. Phase 5 FreeRTOS Target Smoke

目标板验证必须运行在 Scheduler 已启动后的真实 Task Context。

临时测试结构：

```text
Button Smoke Task
    -> platform_button_read
    -> platform_time_get_ms
    -> service_button_process
    -> Serial Assistant + RTT event
    -> Platform Queue

Indicator Smoke Task
    -> Platform Queue receive
    -> service_indicator_handle_event
    -> Platform LED
```

Smoke 使用现有 Platform OS：

```text
platform_thread
platform_queue
platform_time
```

不直接使用 raw FreeRTOS Queue / HAL_Delay / osDelay / vTaskDelay。

Smoke-only 映射：

```text
SINGLE -> RUNNING      -> LED ON
DOUBLE -> ONCE_SUCCESS -> 3 blinks -> OFF
LONG   -> STOPPED      -> LED OFF
```

注意：该映射只用于 Button + Indicator 集成验证。正式产品仍然是：

```text
DOUBLE
 -> APP SAMPLE_ONCE
 -> Sensor acquisition
 -> UART TX success
 -> ONCE_SUCCESS
```

观察通道：

```text
USART1 Serial Assistant
RTT / EasyLogger
PC13 LED
```

推荐序列：

```text
startup -> OFF
single  -> SINGLE + ON
double  -> DOUBLE + 3 blink + OFF
single  -> SINGLE + ON
long >= 3 s -> LONG + OFF
```

必须同时确认无重复事件、无 HardFault、UART / RTT 正常共存。

Smoke 完成后删除正常启动路径中的临时 Task / Queue hook，并重新 Normal-path Keil Full Rebuild。

---

# 10. Phase 5 明确不冻结

以下永久 RTOS 设计仍留到 Phase 9：

```text
Permanent Button Task ownership
Permanent Button Task priority
Permanent Button Task stack
Final Button -> APP IPC
Final event buffering / queue policy
```

Phase 5 Smoke Task / Queue 只是验证 Harness，不得成为永久架构既成事实。

---

# 11. 当前执行计划

唯一 Active Plan：

```text
00_Doc/04_Agent/implementation_plan.md
```

当前内容：

```text
Button Phase 5 Implementation Plan
Status: COMPLETED
```

计划已包含：

```text
Mandatory Preflight
Platform Button TDD
BSP Button TDD
Button Service state-machine TDD
Keil production integration
FreeRTOS Button + Indicator smoke harness
Serial Assistant + RTT + LED target verification
Smoke cleanup
Final regression / coding review / handoff
```

执行必须按 Task 顺序推进并逐项记录真实结果。

---

# 12. 当前下一步

当前工程已经完成 Button Phase 5 的：

```text
Requirements alignment     DONE
Architecture discussion    DONE
Design freeze              DONE
Formal design document     DONE
Implementation plan        DONE
```

Phase 5 验证证据：

```text
Host Test: Platform Button / BSP Button / Button Service 与既有回归 PASS
Keil normal production rebuild: 0 Error(s), 20 Warning(s)
Target board: user-confirmed PASS
Serial Assistant: START / READY / SINGLE / DOUBLE / LONG
RTT: START / READY / SINGLE / DOUBLE / LONG
Temporary Button / Indicator Smoke Task、Queue、启动钩子和 Keil Test 组：REMOVED
Coding Standard Review: PASS
```

下一步：

```text
Stop after Phase 5. Do not start Phase 6 until a new design and implementation plan are approved.
```

执行完成前不得开始：

```text
Phase 6 DHT20 implementation
Final APP Control FSM
Permanent Button RTOS architecture
```

---

# 13. 后续路线

```text
Phase 3  Software I2C                   COMPLETED
Phase 4  LED Module                     COMPLETED
Phase 5  Button Module                  CURRENT / PLAN READY
Phase 6  DHT20 Environment Module
Phase 7  MPU6050 Motion Module
Phase 8  UART Application Communication
Phase 9  RTOS Task / Event Design
Phase 10 Final APP Integration
Final Integrated Board Test
```

---

# 14. 当前暂缓范围

```text
SPI / LCD / GUI
W25Q64
AT24C02
Bluetooth
Roll / Pitch / Yaw
DMP
Kalman / Complementary Filter
复杂二进制 UART Protocol
Button EXTI / low-power wake
无需求驱动的框架扩展
```

不得主动加入当前开发主线。
