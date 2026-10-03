# Closed Implementation Plan — RTOS Display Integration

> 状态：COMPLETE / HOST + KEIL + TARGET FUNCTION VERIFIED  
> 日期：2026-09-06  
> 设计依据：`00_Doc/02_架构设计/RTOS_Display_Integration_Design.md`

最终验证证据：

```text
Implementation            COMPLETE
Host full regression      PASS 40/40
Keil full rebuild         PASS / 0 errors
Modified production code  no new warnings
Target functional test    PASS
```

未作为本功能阶段关闭阻塞项执行：

```text
Dedicated LCD fault-injection target test
Task stack high-water mark observation
Queue peak occupancy observation
```

以上转为后续可选可靠性/资源优化工作。

---

# 1. 目标

在不破坏 Phase 1~9 已验证 Core Application 的前提下，将 ST7789 正式接入 RTOS 产品链，完成：

```text
Display Task
Display Queue
Boot -> Main UI
UART measurement output migration
ONCE semantic migration
SPI Platform integration gap
app_system composition-root integration
Host / Keil / Target functional verification
```

结果：全部完成。

---

# 2. 执行约束保持结果

实现保持：

```text
APP -> Service -> Platform -> Impl -> Vendor
APP -> Impl FORBIDDEN
Service -> Impl FORBIDDEN
no runtime malloc/free
Queue copy-by-value
no stack-pointer enqueue
```

Display：

```text
Display Task = sole ST7789 / Graphics runtime owner
no Display Service
no full framebuffer
partial refresh
Display failure does not redefine acquisition success
```

ONCE：

```text
success = DHT20 OK && MPU6050 OK
```

UART：

```text
retain full command/response/debug function
remove sensor measurement reports only
```

---

# 3. Task 1 — SPI Platform Integration Gap

状态：COMPLETE

完成：

```text
Platform BSP constructor for display SPI Bus / SPI1 binding
Platform public SPI Bus lifecycle facade
reuse existing SPI lifecycle model
APP no direct impl_platform_spi dependency
APP no lifecycle function-table dereference
```

验证：Host/Keil PASS，架构边界保持。

---

# 4. Task 2 — Shared APP Types / IPC Migration

状态：COMPLETE

完成：

```text
app_control_state_t moved to shared control types
APP_CONTROL_MESSAGE_ONCE_COMPLETE introduced
APP_DISPLAY_MESSAGE_SYSTEM_STATE introduced
APP_DISPLAY_MESSAGE_MEASUREMENT introduced
app_display_message_t value-copy IPC introduced
APP_CONTROL_RESPONSE_OK_ONCE introduced
old UART measurement outbound message types removed
```

删除：

```text
APP_CONTROL_MESSAGE_ONCE_ACQUISITION_FAILED
APP_CONTROL_MESSAGE_ONCE_TX_RESULT
APP_COMM_OUTBOUND_PERIODIC_REPORT
APP_COMM_OUTBOUND_ONCE_REPORT
```

Communication Response Queue 使用：

```text
app_control_response_t
```

---

# 5. Task 3 — Communication APP Simplification

状态：COMPLETE

保留：

```text
UART RX DMA + RingBuffer path
strict CRLF parser
START / STOP / ONCE / STATUS / HELP
control event submit
control response TX
local HELP / error responses
```

删除：

```text
sensor report formatting
ENV / IMU report buffers
periodic report TX
ONCE report TX
ONCE TX completion -> Control
sensor report statistics
Communication controlQueue dependency used by old ONCE TX completion
```

新增 UART 成功响应：

```text
OK ONCE\r\n
```

Communication 已退出 sensor data plane。

---

# 6. Task 4 — Control APP Migration

状态：COMPLETE

完成：

```text
initial SYSTEM_STATE(STOPPED) publish
START -> RUNNING + Indicator + Display + UART response
STOP -> STOPPED + Indicator + Display + UART response
Display publish = best-effort NO_WAIT
ONCE_COMPLETE replaces old acquisition/TX completion split
ONCE_COMPLETE(OK) -> clear onceActive + success blink + UART OK ONCE
ONCE_COMPLETE(error) -> clear onceActive + no blink + UART acquisition error
```

Control FSM 仍是唯一业务状态真值。

---

# 7. Task 5 — Acquisition APP Migration

状态：COMPLETE

配置：

```text
remove communicationQueue
add displayQueue
retain controlQueue
```

Periodic：

```text
sample success
 -> Display MEASUREMENT / NO_WAIT
```

ONCE：

```text
sample failure
 -> Control ONCE_COMPLETE(error) / WAIT_FOREVER

sample success
 -> Display MEASUREMENT / NO_WAIT
 -> Control ONCE_COMPLETE(OK) / WAIT_FOREVER
```

Display Queue failure 不改变 ONCE acquisition success。

---

# 8. Task 6 — APP Display Module

状态：COMPLETE

实现：

```text
01_APP/app_display.h
01_APP/app_display.c
Host tests
```

Display context：

```text
initialized
available
systemState + valid
latestMeasurement + valid
stateDirty
measurementDirty
```

启动：

```text
platform_st7789_init
 -> Boot Page
 -> backlight ON
 -> 1000 ms dwell
 -> Main UI static layout
 -> drain queue
 -> render latest cache
 -> event loop
```

Queue 消费：

```text
WAIT_FOREVER receive first
 -> update cache
 -> NO_WAIT drain backlog
 -> render dirty latest state / measurement
```

启动失败进入 degraded queue consumer；runtime render failure 保留 dirty 并等待下一 Display event 重试。

---

# 9. Task 7 — Main UI

状态：COMPLETE / TARGET FUNCTION VERIFIED

页面：

```text
SENSOR MONITOR
STATE : RUNNING / STOPPED
ENVIRONMENT
TEMP
HUM
ACCEL X/Y/Z
GYRO X/Y/Z
```

格式：

```text
Temp       1 decimal
Humidity   1 decimal
Accel      3 decimals
Gyro       1 decimal
```

行为：

```text
initial measurement = --
STOP retains latest valid measurement
static layout once
partial dynamic-region refresh
```

目标板显示功能已确认正常。

---

# 10. Task 8 — Composition Root Integration

状态：COMPLETE

新增静态资源：

```text
g_displaySpiBus
g_display
g_displayQueue
g_appDisplay
g_displayThread
```

配置：

```text
PROJECT_DISPLAY_TASK_STACK_SIZE_BYTES = 1536
PROJECT_DISPLAY_TASK_PRIORITY = NORMAL
PROJECT_DISPLAY_QUEUE_DEPTH = 4
PROJECT_DISPLAY_BOOT_DURATION_MS = 1000
```

Pre-scheduler：

```text
construct display SPI Bus / ST7789
SPI Bus init/start
create Display Queue
init APP modules
create Display Thread
```

ST7789 physical init 保持在 Display Task Context。

Rollback 保持 strict reverse order。

---

# 11. Task 9 — Host Test Update

状态：PASS

```text
Host full regression PASS 40/40
```

覆盖包括：

```text
SPI BSP / lifecycle facade
Display message validation
Display coalescing
initial -- rendering
SYSTEM_STATE update
MEASUREMENT update
STOP retains latest measurement
Display degraded startup behavior
runtime render dirty retry behavior
Control initial STOPPED publish
START / STOP display state publish
ONCE_COMPLETE OK / error
ONCE completion independent from Display Queue result
Acquisition no Communication dependency
Communication no sensor report formatting
UART OK ONCE response
app_system rollback
```

---

# 12. Task 10 — Keil / Static Verification

状态：PASS

```text
Keil full rebuild PASS
0 errors
new/modified production code no new warnings
```

架构检查确认 APP 未引入 Impl SPI 依赖。

---

# 13. Task 11 — Target Functional Verification

状态：PASS

人工板测确认当前功能正常，覆盖本阶段主要功能链：

```text
Boot Page -> Main UI
initial STOPPED / placeholder behavior
START -> RUNNING + immediate acquisition
~2 s LCD measurement refresh
STOP -> STOPPED + retain latest measurement
Button ONCE
UART ONCE -> OK ONCE
START / STOP / STATUS / HELP UART regression
UART no periodic/ONCE ENV/IMU measurement reports
ONCE success LED behavior
```

因此当前 RTOS Display Integration 可以作为正常目标板功能基线直接使用。

独立 Display fault-injection（例如安全断开 LCD 后验证其他链路）未在本次关闭记录中单独声明 PASS；该项转为后续可靠性验证候选。

---

# 14. Task 12 — Resource Observation

状态：DEFERRED / OPTIONAL

尚未记录：

```text
Communication Task high-water mark
Control Task high-water mark
Acquisition Task high-water mark
Display Task high-water mark
Indicator Task high-water mark
Display Queue peak occupancy
Communication Response Queue peak occupancy
```

当前目标板功能已稳定运行，因此这些数据用于后续证据驱动的 stack/Queue 优化，不阻塞本功能阶段关闭。

---

# 15. Task 13 — Documentation Closeout

状态：COMPLETE

已同步：

```text
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/implementation_plan.md
```

---

# 16. Completion Result

当前完成证据：

```text
Display Task implemented
Display Queue implemented
Boot/Main UI implemented
UART measurement migration complete
ONCE semantic migration complete
Communication sensor-data dependency removed
Acquisition Communication dependency removed
APP -> Impl boundary clean
Host full regression PASS 40/40
Keil rebuild PASS / 0 errors
Target functional verification PASS
documentation synchronized
```

结论：

```text
RTOS Display Integration
= COMPLETE / HOST + KEIL + TARGET FUNCTION VERIFIED
```

独立 LCD fault-injection 与资源高水位观测为后续可选项，不改变该功能阶段完成结论。

---

# 17. Closed Plan Boundary

后续不得因进入其他阶段而重新设计或破坏：

```text
Display Task sole runtime ownership
no Display Service baseline
ONCE acquisition-only success semantic
UART command/response role
LCD measurement presentation role
Acquisition -X-> Communication measurement dependency
consumer-side Display Queue coalescing
APP -> Platform -> Impl SPI boundary
```

当前无 Active Implementation Plan。
