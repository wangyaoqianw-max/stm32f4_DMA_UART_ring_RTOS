# Active Implementation Plan — RTOS Display Integration

> 状态：IMPLEMENTED / HOST + KEIL VERIFIED / TARGET PENDING
> 日期：2026-09-06  
> 设计依据：`00_Doc/02_架构设计/RTOS_Display_Integration_Design.md`

当前验证证据：

```text
Host full regression      PASS 40/40
Keil full rebuild         PASS / 0 errors
Modified production code no new warnings
Target verification       PENDING MANUAL BOARD TEST
Resource observation      PENDING MANUAL BOARD TEST
```

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
Host / Keil / Target verification
```

不重新设计已冻结基础模块。

---

# 2. 执行约束

必须保持：

```text
APP -> Service -> Platform -> Impl -> Vendor
APP -> Impl FORBIDDEN
Service -> Impl FORBIDDEN
no runtime malloc/free
Queue copy-by-value
no stack-pointer enqueue
```

Display 规则：

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

# 3. Task 1 — Close SPI Platform Integration Gap

目标：保证 `app_system` 不直接依赖 Impl SPI。

预计修改：

```text
03_Platform/platform_mcu/spi/platform_spi.h
03_Platform/platform_mcu/spi/platform_spi.c
03_Platform/platform_bsp/... SPI BSP files as appropriate
04_Impl/impl_mcu/impl_platform_spi.* only if needed for binding exposure
Host tests for Platform SPI lifecycle facade / BSP binding
```

实施：

```text
1. Add Platform BSP constructor for display SPI Bus / SPI1 binding.
2. Add Platform public lifecycle facade for SPI Bus init/start/stop/deinit.
3. Reuse existing lifecycle ops; do not create parallel SPI model.
4. APP must not dereference bus->device.lifecycle directly.
5. APP must not include impl_platform_spi.h.
```

验收：

```text
Host SPI tests PASS
existing SPI/ST7789 tests remain PASS
no APP -> Impl dependency
```

---

# 4. Task 2 — Migrate Shared APP Types / IPC

预计修改：

```text
01_APP/app_control_types.h
01_APP/app_ipc_types.h
related Host tests
```

实施：

```text
1. Move app_control_state_t to app_control_types.h.
2. Replace old Control completion types with APP_CONTROL_MESSAGE_ONCE_COMPLETE.
3. Add APP_DISPLAY_MESSAGE_SYSTEM_STATE.
4. Add APP_DISPLAY_MESSAGE_MEASUREMENT.
5. Add app_display_message_t value-copy union.
6. Add APP_CONTROL_RESPONSE_OK_ONCE.
7. Remove old Communication measurement message wrapper if no longer needed.
```

删除：

```text
APP_CONTROL_MESSAGE_ONCE_ACQUISITION_FAILED
APP_CONTROL_MESSAGE_ONCE_TX_RESULT
APP_COMM_OUTBOUND_PERIODIC_REPORT
APP_COMM_OUTBOUND_ONCE_REPORT
```

若 Communication Response Queue 只携带一种 payload，则直接使用：

```text
app_control_response_t
```

验收：

```text
no temporary stack pointer in IPC
all message sizes fixed
no HAL / FreeRTOS concrete handle in APP IPC
```

---

# 5. Task 3 — Simplify Communication APP

预计修改：

```text
01_APP/app_communication.h
01_APP/app_communication.c
Host tests
```

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
ONCE TX completion submit to Control
report statistics
controlQueue dependency used only for ONCE TX completion
```

新增 UART response：

```text
OK ONCE\r\n
```

Communication Outbound consumer 改为消费：

```text
app_control_response_t
```

验收：

```text
Communication no longer depends on app_acquisition_data_t
ONCE success response supported
all existing parser/control responses retained
```

---

# 6. Task 4 — Migrate Control APP

预计修改：

```text
01_APP/app_control.h
01_APP/app_control.c
Host tests
```

新增依赖：

```text
Display Queue
```

实施：

```text
1. Publish initial SYSTEM_STATE(STOPPED) from Control Task runtime context.
2. START success -> state RUNNING -> Indicator -> Display SYSTEM_STATE -> UART response.
3. STOP success -> state STOPPED -> Indicator -> Display SYSTEM_STATE -> UART response.
4. Display publish is best-effort NO_WAIT and must not roll back FSM.
5. Replace acquisition-failure / TX-result handlers with one ONCE_COMPLETE handler.
6. ONCE_COMPLETE(OK): clear onceActive, blink success, UART source -> OK ONCE.
7. ONCE_COMPLETE(error): clear onceActive, no blink, UART source -> ERR ACQUISITION_FAILED.
```

注意：

```text
Control FSM remains sole state truth.
Display state is presentation snapshot only.
```

验收：

```text
START/STOP semantics unchanged except display publication
ONCE no longer waits for UART or Display completion
initial STOPPED reaches Display
```

---

# 7. Task 5 — Migrate Acquisition APP

预计修改：

```text
01_APP/app_acquisition.h
01_APP/app_acquisition.c
Host tests
```

配置迁移：

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

重要：

```text
Display Queue failure must not change ONCE completion result.
```

统计迁移：

```text
rename/remove Communication-specific publish counters
add Display publish counters/failures if useful
retain sample / stale / skipped-period statistics
```

验收：

```text
Acquisition has no direct Communication dependency
periodic STOP stale suppression preserved
ONCE accepted -> eventually exactly one completion
```

---

# 8. Task 6 — Implement APP Display Module

新增：

```text
01_APP/app_display.h
01_APP/app_display.c
Host tests
```

对象依赖：

```text
platform_st7789_t *display
platform_spi_bus_t *spiBus if required by task startup contract
platform_queue_t *queue
```

Context：

```text
initialized
available
systemState + valid
latestMeasurement + valid
stateDirty
measurementDirty
```

`app_display_init()`：

```text
validate / bind only
no ST7789 physical init
no osDelay
no boot draw
```

Task entry：

```text
platform_st7789_init
 -> draw Boot Page
 -> backlight ON
 -> 1000 ms dwell
 -> Main UI static layout
 -> drain queue
 -> render latest cache
 -> event loop
```

Queue loop：

```text
WAIT_FOREVER receive first
 -> update cache
 -> NO_WAIT drain backlog
 -> render dirty state / measurement
```

Failure：

```text
startup init/draw failure -> degraded queue consumer
runtime render failure -> keep dirty, retry on next event
no periodic retry loop
```

---

# 9. Task 7 — Implement Main UI

页面必须遵循冻结信息架构：

```text
SENSOR MONITOR
STATE : RUNNING / STOPPED
ENVIRONMENT
TEMP
HUM
ACCEL X/Y/Z
GYRO X/Y/Z
```

首次值：

```text
--
```

STOP：

```text
retain latest valid measurement
```

格式：

```text
Temp 1 decimal
Humidity 1 decimal
Accel 3 decimals
Gyro 1 decimal
```

Implementation note：

```text
If %f/snprintf cost is undesirable, fixed-point formatting may be used,
but the frozen UI precision must not change.
```

刷新：

```text
static layout once
clear dynamic value rect
redraw dynamic value only
```

不得每 2 s 全屏 redraw。

---

# 10. Task 8 — Composition Root Integration

预计修改：

```text
01_APP/app_system.c
00_Config/project_config.h
Keil project/source lists if required
Host build lists if required
```

新增静态资源：

```text
g_displaySpiBus
g_display
g_displayQueue
g_appDisplay
g_displayThread
```

新增配置：

```text
PROJECT_DISPLAY_TASK_STACK_SIZE_BYTES = 1536
PROJECT_DISPLAY_TASK_PRIORITY = NORMAL
PROJECT_DISPLAY_QUEUE_DEPTH = 4
PROJECT_DISPLAY_BOOT_DURATION_MS = 1000
```

Pre-scheduler：

```text
construct display SPI Bus
construct ST7789
SPI Bus init/start
create Display Queue
init APP modules
create Display Thread
```

ST7789 physical init 不得出现在 `app_system_init()`。

Rollback：

```text
strict reverse order
terminate Display Thread before queues
remove Display Queue
stop/deinit SPI Bus after higher-level resources
reset all added static storage
```

---

# 11. Task 9 — Host Test Update

需要更新旧 Phase 9 tests，避免继续断言 UART measurement output / ONCE TX semantics。

至少覆盖：

```text
SPI BSP / lifecycle facade
Display message validation
Display coalescing
initial -- rendering model
SYSTEM_STATE update
MEASUREMENT update
STOP retains latest measurement
Display init failure degraded behavior
runtime render failure dirty retry behavior
Control initial STOPPED publish
START / STOP display state publish
ONCE_COMPLETE OK / error
ONCE completion independent from Display Queue result
Acquisition no Communication dependency
Communication no sensor report formatting
UART OK ONCE response
app_system rollback
```

执行：

```text
focused tests first
then full Host regression
```

要求：

```text
all tests PASS
no removed Phase 9 semantic left as stale test expectation
```

---

# 12. Task 10 — Keil / Static Verification

执行完整 rebuild。

要求：

```text
0 errors
no new warning in new/modified production files
```

特别检查：

```text
include path
source group
stack/resource definitions
printf float linkage impact if used
APP -> Impl forbidden includes
```

如果 `%f` 引入明显不合理 Flash/stack cost，可改 fixed-point formatter 后重新验证，不改变 UI precision contract。

---

# 13. Task 11 — Target Verification

建议板测顺序：

## 11.1 Boot

```text
power on
 -> Boot Page visible
 -> no partial/random first frame after BL ON
 -> transition to Main UI
```

## 11.2 Initial State

```text
STATE STOPPED
measurement --
```

## 11.3 START

```text
Button SINGLE / UART START
 -> STATE RUNNING
 -> immediate first measurement
 -> every ~2 s refresh
 -> UART no ENV/IMU report
```

## 11.4 STOP

```text
Button LONG / UART STOP
 -> STATE STOPPED
 -> last valid measurement retained
 -> no new periodic measurement
```

## 11.5 Button ONCE

```text
STOPPED
 -> DOUBLE
 -> one measurement update
 -> LED blink 3x on acquisition success
 -> state stays STOPPED
```

## 11.6 UART ONCE

```text
STOPPED
 -> ONCE\r\n
 -> one measurement update
 -> OK ONCE\r\n
 -> LED blink 3x
 -> no sensor report text
```

## 11.7 UART Regression

```text
START
STOP
STATUS
HELP
unknown command
command too long
```

全部保留原行为。

## 11.8 Failure Isolation

通过安全方式验证 Display failure path 时：

```text
UART command remains usable
Control FSM remains usable
sensor acquisition continues
Indicator continues
no whole-system Error_Handler caused by LCD runtime failure
```

---

# 14. Task 12 — Resource Observation

在目标板验证后记录：

```text
Communication Task high-water mark
Control Task high-water mark
Acquisition Task high-water mark
Display Task high-water mark
Indicator Task high-water mark
Display Queue peak occupancy
Communication Response Queue peak occupancy
```

本阶段只记录证据；除非存在明显风险，不做额外资源优化。

---

# 15. Task 13 — Documentation Closeout

实施和验证完成后更新：

```text
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/implementation_plan.md
```

将状态从：

```text
DESIGNED / NOT IMPLEMENTED
```

更新为真实验证结果，例如：

```text
COMPLETE / HOST + KEIL + TARGET VERIFIED
```

不得提前填写未执行的 Target PASS。

---

# 16. Completion Criteria

当前状态：实现、Host、Keil、静态架构检查和文档同步已完成；Target verification、目标板 failure isolation 与资源观测尚未执行，因此本计划不得标记 COMPLETE。

只有以下全部完成，计划才可标记 COMPLETE：

```text
Display Task implemented
Display Queue implemented
Boot/Main UI implemented
UART measurement migration complete
ONCE semantic migration complete
Communication sensor-data dependency removed
Acquisition Communication dependency removed
APP -> Impl boundary clean
Host full regression PASS
Keil rebuild PASS
Target verification PASS
failure isolation PASS
documentation synchronized
```

---

# 17. Codex Execution Boundary

Codex 执行时必须：

```text
follow frozen design
inspect current repo before editing
make smallest coherent changes
update tests with semantic migration
run focused tests and full regression
run Keil build if environment supports it
never claim target verification without hardware evidence
```

如实现中发现设计与现有底层 API 存在真实冲突：

```text
stop architectural expansion
report exact conflict
prefer minimal adapter/facade consistent with frozen layering
```

不得自行引入：

```text
Display Service
generic GUI framework
SPI DMA
Touch
shared snapshot + mutex architecture
new business states
```
