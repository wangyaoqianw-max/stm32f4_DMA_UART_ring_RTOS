# 工程交接说明

更新时间：2026-08-29

## 1. 工程快照

- 工程根目录：`RTT_elog_DMA_UART_ring_project/`
- 当前分支：`main`
- 当前活动阶段：UART Phase 2A — DMA RX + IDLE + Platform RX_DATA Event
- 当前状态：`READY_FOR_IMPLEMENTATION`
- MCU：STM32F411CEU6，UFQFPN48
- 软件环境：STM32 HAL、CMSIS-RTOS2 / FreeRTOS、Keil、EasyLogger、SEGGER RTT

固定依赖方向：

```text
APP -> Service -> Platform -> Impl -> Vendor / HAL / RTOS / Hardware
```

当前阶段只建立：

```text
USART1 RX
 -> DMA Circular
 -> IDLE / HT / TC
 -> STM32 UART Impl
 -> Platform RX_DATA Event
```

不进入 RingBuffer / UART Service / RTOS Notification。

---

## 2. 已完成阶段

### Log Phase 1 — COMPLETED

已完成：

- Platform Log 与 EasyLogger / RTT 解耦。
- Host Test PASS。
- Keil Build PASS。
- RTT Runtime Smoke Test PASS。

### UART Phase 1 — COMPLETED

真实板测已确认：

- construct PASS；
- CREATED 状态写保护 PASS；
- init/start PASS；
- Blocking TX PASS；
- Blocking fixed-length RX PASS；
- stop / STOPPED guard PASS；
- restart + TX PASS；
- deinit PASS。

USB-UART 实际收到：

```text
UART_PHASE1_TX_OK
UART_RESTART_OK
```

PC -> MCU：

```text
PING
```

RTT 确认固定长度 RX PASS。

临时测试代码已经恢复，恢复后 Keil Full Rebuild：

```text
0 Error(s), 13 Warning(s)
```

---

## 3. Keil Build 目录状态

Keil 构建输出已经整理为独立目录，生成物不再作为工程源码跟踪。

构建时仍需遵守：

```text
Agent / Git 写操作结束
    ↓
Keil Clean / Rebuild
```

如出现：

```text
C4051E
L6449E
Invalid argument
```

优先按 Keil / Windows 文件 I/O 环境问题处理，不修改 UART 业务代码规避。

---

## 4. Phase 2A CubeMX Baseline — VERIFIED

用户已完成 CubeMX 配置并 Generate Code。

当前 `.ioc` / 生成代码确认：

```text
USART1_TX              PA9
USART1_RX              PA10
115200 / 8N1

USART1_RX DMA          DMA2 Stream2 / Channel 4
Direction              Peripheral -> Memory
Peripheral Increment   Disable
Memory Increment       Enable
Peripheral Width       Byte
Memory Width           Byte
Mode                   Circular
Priority               Medium
FIFO                   Disable

USART1_IRQn            Priority 5 / Sub 0
DMA2_Stream2_IRQn      Priority 5 / Sub 0
```

生成代码已存在：

```text
DMA_HandleTypeDef hdma_usart1_rx
__HAL_LINKDMA(uartHandle, hdmarx, hdma_usart1_rx)
DMA2_Stream2_IRQHandler -> HAL_DMA_IRQHandler(...)
USART1_IRQHandler       -> HAL_UART_IRQHandler(...)
```

`main.c` 当前初始化顺序：

```text
MX_GPIO_Init()
MX_DMA_Init()
MX_USART1_UART_Init()
```

CubeMX 侧当前无阻塞项。

---

## 5. Phase 2A Design — APPROVED

设计文档：

```text
00_Doc/02_架构设计/UART_Phase2A_DMA_RX设计.md
```

状态：

```text
APPROVED / FROZEN FOR IMPLEMENTATION
```

核心方案：

```text
HAL_UARTEx_ReceiveToIdle_DMA
+ DMA Circular
+ HT / TC / IDLE
+ Platform RX_DATA Event
```

Platform 不感知 HT / TC / IDLE 的来源差异。

---

## 6. Buffer Ownership — Scheme A

方案 A 已批准并冻结。

现有 Platform API 保持：

```c
platform_uart_read_async(uart, buffer, bufferSize)
```

语义改为启动持续 RX Session。

所有权：

```text
Memory Storage Owner      Caller
DMA Control Owner         STM32 UART Impl
RX Session Buffer Writer  DMA / Impl
RX_DATA Consumer          callback read-only
```

调用者提供长期有效的静态 Buffer。

Phase 2A 板测基准：

```text
256 bytes
```

成功 `readAsync()` 后 Buffer 使用权借给 Impl，直到：

```text
cancel(RX)
RX error
lifecycle stop
```

`RX_DATA.event.data` 只保证在 callback 执行期间有效；callback 返回后不得继续保存该 DMA Buffer 指针。

基础 `architecture.md` 中旧的“DMA Buffer 属于 UART Impl”在 Phase 2A 中解释为：

> DMA Buffer 的硬件控制、位置状态和访问纪律属于 Impl；静态 Buffer 存储允许由调用者持有。

Phase 2A 以已批准设计文档为该语义的优先解释。

---

## 7. RX Event / Position Contract

HAL：

```c
HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart,
                           uint16_t Pos)
```

Impl 保存：

```text
rxBuffer
rxBufferSize
rxLastPosition
rxActive
```

新增数据算法：

```text
Pos > last
    -> [last, Pos)

Pos < last
    -> [last, bufferSize)
    -> [0, Pos)

Pos == last
    -> duplicate/no new data, ignore

Pos == bufferSize
    -> emit tail then normalize lastPosition to 0
```

每个连续片段独立产生：

```text
PLATFORM_UART_EVENT_RX_DATA
```

不得把跨 Buffer 尾首的数据伪装成一个连续指针。

---

## 8. Why HT + TC + IDLE

256-byte DMA Buffer / 115200 8N1：

```text
128 bytes ≈ 11.1 ms
256 bytes ≈ 22.2 ms
```

因此：

```text
short burst       -> IDLE
continuous stream -> HT / TC
mixed traffic     -> HT / TC / IDLE
```

不关闭 HT。

`lastPosition` 负责去除可能的重复位置事件。

---

## 9. Callback / ISR Boundary

当前：

```text
USE_HAL_UART_REGISTER_CALLBACKS = 0
```

Phase 2A 由自定义 STM32 UART Impl override：

```c
HAL_UARTEx_RxEventCallback(...)
HAL_UART_ErrorCallback(...)
```

不得修改 Vendor HAL。

Callback / ISR 只允许：

- 判断 USART1；
- 更新轻量 RX Context；
- 计算新增数据位置；
- 产生 Platform Event；
- 板测时做必要的静态 Buffer 数据复制/计数。

禁止：

- 阻塞；
- 普通 Mutex；
- malloc/free；
- 完整协议解析；
- 大量日志；
- USART1 debug print。

Phase 2A 不创建 RTOS Notification。

---

## 10. Cancel / Stop / Error Contract

### cancel(RX)

活动 RX：

```text
HAL_UART_AbortReceive
 -> clear RX session
 -> CANCELED / RX Event
```

无活动 RX：

```text
PLATFORM_ERR_INVALID_STATE
```

Phase 2A：

```text
cancel(TX)  -> NOT_SUPPORTED
writeAsync  -> NOT_SUPPORTED
```

### lifecycle stop

活动 RX 时先 Abort，只有成功后才进入 STOPPED。

stop 不发送 CANCELED Event。

### HAL RX Error

映射：

```text
ORE              -> PLATFORM_ERR_OVERFLOW
DMA / PE / NE / FE -> PLATFORM_ERR_IO
```

ERROR 后释放 RX Session，不在 ISR 中自动重启 DMA。

---

## 11. Current Implementation Plan

文件：

```text
00_Doc/04_Agent/implementation_plan.md
```

当前：

```text
Status: READY_FOR_IMPLEMENTATION
Phase: UART Phase 2A
```

执行顺序：

```text
Host tests first
    ↓
STM32 readAsync + RX context
    ↓
RxEvent position processing
    ↓
Cancel / Stop / Error
    ↓
Platform regression
    ↓
Keil Clean Rebuild
    ↓
Board Smoke Test
    ↓
restore temporary test code
    ↓
final Rebuild
```

---

## 12. Required Board Verification

最低场景：

### Short / IDLE

```text
HELLO
```

确认总长度 5、内容一致。

### Continuous

连续至少：

```text
600 bytes
```

确认：

- >256 bytes 后继续接收；
- 无丢失；
- 无重复；
- 顺序正确。

### Mixed Boundary

覆盖：

```text
HT 128
TC 256
IDLE
```

### Cancel + Restart

取消后重新启动 `readAsync()`，再次正常接收。

### Lifecycle Stop + Restart

活动 DMA RX 中 stop；restart 后重新 `readAsync()` 正常。

---

## 13. Phase 2A Scope Guard

禁止提前实现：

```text
UART Service
Ring Buffer
FreeRTOS Notification
Communication Task
Protocol Parser
DMA TX
Async TX
通用 DMA Platform Framework
impl_dma 通用抽象
Vendor HAL 修改
Platform UART API Redesign
```

如果实现证明必须修改冻结 Platform API：

```text
BLOCKED
```

停止并重新设计。

---

## 14. Completion Rule

只有以下都有真实证据，才能写：

```text
UART Phase 2A = COMPLETED
```

至少包括：

- Host tests PASS；
- Keil Full Rebuild 0 Error；
- Short/IDLE PASS；
- Continuous >256 bytes PASS；
- HT/TC/IDLE 混合边界无丢失/重复；
- Cancel + Restart PASS；
- Stop + Restart PASS；
- Phase 1 regression PASS；
- 临时测试代码恢复；
- 恢复后最终 Rebuild 0 Error；
- 本 handoff 更新真实结果。

没有板测证据时只能写：

```text
CODE_COMPLETE_PENDING_HARDWARE_VERIFICATION
```

---

## 15. Next Phase

Phase 2A 完成后才进入 Phase 2B：

```text
Platform RX_DATA Event
       ↓ ISR
UART Service
       ↓
Ring Buffer
       ↓
ISR-safe Notification
       ↓
Communication Task
```

Phase 2A 完成前不得开始 Phase 2B。

---

## 16. Phase 2A Implementation Handoff — 2026-08-29

状态：

```text
CODE_COMPLETE_PENDING_HARDWARE_VERIFICATION
```

### 修改文件

- `04_Impl/impl_mcu/impl_platform_uart.c`
  - 实现 USART1 `HAL_UARTEx_ReceiveToIdle_DMA()` 持续 RX Session。
  - 实现 IDLE / HT / TC 共用的 Position / Delta 上报、Wrap 拆分和重复 Position 抑制。
  - 实现 RX cancel、lifecycle stop 的 DMA RX Abort，以及 HAL RX Error 事件映射。
  - 以项目自定义 weak callback override 提供 `HAL_UARTEx_RxEventCallback()` 和
    `HAL_UART_ErrorCallback()`；未修改 Vendor HAL 或 CubeMX 生成主体。
- `Tests/impl_platform_uart/test_impl_platform_uart.c`
  - 添加 DMA 启动期回调、重复启动、单调增量、TC 归一化、Wrap、重复 Position、cancel/restart、stop 和 ORE Error 的 Host 测试。
- `Tests/impl_platform_uart/usart.h`
  - 扩展可控 Fake HAL，记录 DMA RX 启动和 Abort 调用。

### Host Test 结果

```text
PASS  Tests/impl_platform_uart
PASS  Tests/platform_uart
PASS  Tests/platform_uart/test_platform_uart_types.c
PASS  Tests/platform_log
PASS  git diff --check
```

### Position / Wrap Test 结果

Host Test 已验证：

- `[0,10)`、`[10,20)` 单调新增；
- `Pos == lastPosition` 不产生重复 `RX_DATA`；
- `last=240, Pos=20` 按 `[240,256)`、`[0,20)` 顺序产生两个事件。
- `Pos=256` 的 TC 边界上报尾部后归一化为 `0`，紧随的 `Pos=0` 不重复上报。
- Fake HAL 在 `HAL_UARTEx_ReceiveToIdle_DMA()` 返回前触发回调，验证首段数据不会因启动期 IRQ 竞态丢失。

### Cancel / Stop / Error Test 结果

- `cancel(RX)` 调用 `HAL_UART_AbortReceive()`、发送 `CANCELED/RX`、并允许重新 `readAsync()`。
- 活动 RX 的 stop 调用 Abort 且后续模拟 HAL RX Callback 不产生 `RX_DATA`。
- HAL start 的 `BUSY`、`ERROR` 分别映射为 `PLATFORM_ERR_BUSY`、`PLATFORM_ERR_IO`，不建立活动 Session。
- HAL ORE 映射为 `PLATFORM_ERR_OVERFLOW`，发送 `ERROR/RX` 并允许重新 `readAsync()`。

### Keil Build 状态

```text
CODE_COMPLETE_PENDING_KEIL_VERIFICATION
```

当前 Codex Host 环境未执行真实 Keil Build。请在本地执行：

```text
Clean Targets
→ Rebuild all target files
```

验收要求：`0 Error(s)`。

### Hardware Verification 状态

未执行，不能声称通过。待板测：Short + IDLE、Multiple bursts、连续超过 256 bytes、Wrap、Cancel / Restart；测试输出优先 RTT，避免使用 USART1 自身大量打印。

### Deviations / Remaining Warnings / Blockers

- Deviations：无；Platform UART 公共 API、CubeMX DMA 配置、IRQ 薄入口均未修改。
- Remaining Warnings：Host 编译使用 `-Wall -Wextra -Werror`，无警告；Keil 编译结果待验证。
- Blockers：无。

---

## 17. Phase 2A Board Smoke Test Code — 2026-08-29

状态：

```text
BOARD_TEST_CODE_READY
```

### 修改文件

- `Core/Src/freertos.c`
  - 仅在 CubeMX `USER CODE` 区添加以 `UART PHASE2A BOARD TEST BEGIN/END` 标记的临时板测代码。
  - 使用静态 Platform UART、256-byte DMA RX Buffer、1024-byte Capture Buffer。
  - ISR callback 仅复制 `RX_DATA`、更新计数和错误/取消状态；日志与比较全部位于 `StartDefaultTask()`。
  - 实现 Short + IDLE、Multiple Bursts、640-byte Continuous、300-byte HT/TC/IDLE、Cancel/Restart、Stop/Restart 的人工板测状态机。

### 验证结果

- `git diff --check`：PASS。
- `Tests/impl_platform_uart`：PASS。
- `Tests/platform_uart`：PASS。
- 当前 Codex 环境未发现可执行 Keil 工具链；本轮未实际执行 Keil Build。
- 用户已在新增板测代码前报告 Keil `0 Error(s)`；烧录前必须对当前工作区重新执行 `Clean Targets -> Rebuild all target files`。

### Hardware Verification

```text
PENDING
```

尚未执行任何真实板测，不得将 UART Phase 2A 标记为 COMPLETED。

---

## 18. Phase 2A Board Verification Result — 2026-08-29

状态：

```text
CODE_COMPLETE_PENDING_KEIL_VERIFICATION
```

### Hardware Verification

真实目标板经 RTT 验证通过：

- Short + IDLE：PASS。
- Multiple Bursts：PASS。
- Continuous 640 Bytes：PASS，`received=640`、`mismatch=0`。
- HT/TC/IDLE Boundary 300 Bytes：PASS。
- Cancel Event 与 Old RX Session Stopped：PASS。
- Cancel Restart：PASS。
- Lifecycle Stop、No Canceled Event、Stop Restart：PASS。
- 最终报告：`UART PHASE2A BOARD TEST: PASS`，`CAPTURE OVERFLOW=0`、`ERROR EVENTS=0`。

### Temporary Test Restore

`Core/Src/freertos.c` 中以 `UART PHASE2A BOARD TEST BEGIN/END` 标记的临时测试代码已全部移除，默认任务恢复为空闲 `osDelay(1)` 循环。

### Remaining Verification

当前 Codex 环境不能执行 Keil。临时测试代码恢复后仍需在本地执行：

```text
Clean Targets
→ Rebuild all target files
```

只有确认恢复后工程为 `0 Error(s)`，才可将 UART Phase 2A 标记为 `COMPLETED`。

---

## 19. RTOS Platform Phase 1 — Coding Standard Review — 2026-08-30

```text
Coding Standard Review: PASS
```

Review 范围：本阶段已创建或修改的 `03_Platform/platform_os/`、
`04_Impl/impl_os/freertos/` 与 `Tests/platform_os/` 自研文件。

- 已按《嵌入式项目C代码设计规范.md》检查文件头、Header Guard、命名、注释语言、
  公共 API Doxygen、4 空格缩进、控制语句大括号、单行单语句与 Yoda Condition。
- 公共 API 文档维护在 Platform Header；Impl `.c` 不重复公共 API Doxygen。
- 已检查 NULL、对象生命周期、毫秒 timeout、CMSIS 错误映射与 ISR/Task 接口边界。
- 未修改 Vendor、CMSIS、FreeRTOS 或 CubeMX 生成文件。
- Header Isolation 与当前 Fake CMSIS Host Test 均以 `-Wall -Wextra -Werror` 通过。

当前 RTOS Platform Phase 1 仍处于实现中；已完成 Public Header、Time/Delay、Thread、
Software Timer，尚未完成 Mutex、Semaphore、Queue、Notification、Keil 集成和板测。

---

## 20. RTOS Platform Phase 1 — Host Complete / Keil Integration — 2026-08-30

状态：

```text
CODE_COMPLETE_PENDING_KEIL_VERIFICATION
```

### 本轮完成范围

- 完成 Mutex、Binary/Counting Semaphore、Queue、Thread Notification 的 CMSIS-RTOS2
  适配实现与 Host Test；至此冻结的七类 Platform OS 能力均已实现。
- `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx` 已加入
  `03_Platform/platform_os`、`04_Impl/impl_os/freertos` 的 include path，并新增
  `impl/impl_os/freertos` Group，包含七个 `impl_freertos_*.c` 源文件。
- 未创建 UART Service、RingBuffer、service_log；未修改 UART DMA Impl、Vendor FreeRTOS/CMSIS
  或 CubeMX 生成代码。

### 公共 API / Backend

- 公共 Platform OS API：Thread、Mutex、Semaphore、Queue、Thread Notification、Software Timer、
  Time/Delay；公共 Handle 均保持 opaque，未暴露 CMSIS-RTOS2 或 FreeRTOS 类型。
- Backend：全部通过 CMSIS-RTOS2；未直接调用原生 FreeRTOS API。
- Timer 直接将 Platform `callback`、`argument`、`name` 映射到 `osTimerNew()`；未增加私有 Timer
  context、分配器、registry 或额外 Handle 指针。

### Host Test / Regression

```text
PASS  Tests/platform_os/test_platform_os.c          (-Wall -Wextra -Werror)
PASS  Tests/platform_os/test_platform_os_headers.c  (Header Isolation)
PASS  Tests/platform_uart/test_platform_uart.c
PASS  Tests/platform_uart/test_platform_uart_types.c
PASS  Tests/impl_platform_uart/test_impl_platform_uart.c
PASS  Tests/platform_log/test_platform_log.c
PASS  git diff --check
```

- Host Fake CMSIS 覆盖 timeout 向上取整、Thread、Mutex/Semaphore、Queue、Notification、Timer
  的参数映射、错误映射、ISR 路由和对象生命周期。
- Header Isolation 检查确认 Platform OS 公共 Header 不包含也不暴露 CMSIS-RTOS2 / FreeRTOS
  具体 Header 或 Handle 类型。

### Coding Standard Review

```text
Coding Standard Review: PASS
```

- 本阶段自研 C/H 与 Host Test 已复查文件头、公共 API Doxygen 位置、中文设计注释、命名、
  大括号、Yoda Condition、NULL/timeout/资源生命周期、ISR/Task 边界。
- 未为统一风格修改既有 Vendor 或 CubeMX 代码。

### Keil / Board Verification

```text
Keil Build: PENDING
Board Smoke Test: PENDING
```

- 当前环境未发现可调用 `UV4.exe`，未执行真实 `Clean Targets -> Rebuild all target files`，
  因此不能声称 Keil `0 Error(s)`。
- 已按计划在板测前停止；未写入 `Core/Src/freertos.c` 临时板测代码。
- 下一步人工操作：打开 MDK 工程，执行 `Clean Targets -> Rebuild all target files`；仅在实际
  取得 `0 Error(s)` 后，才进入最小 RTOS Platform Board Smoke Test。

### UART Phase 2A 历史状态

- UART Phase 2A 真实板测仍为 PASS，临时板测代码已恢复；恢复后的 Keil `0 Error(s)` 仍无真实
  记录，故其状态保持 `CODE_COMPLETE_PENDING_KEIL_VERIFICATION`，未改为 `COMPLETED`。

### Warnings / Deviations / Blockers

- Warnings：Host 编译按 `-Wall -Wextra -Werror` 通过；Keil 编译警告待真实构建确认。
- Deviations：无。
- Blockers：无代码阻塞，仅等待 Keil 人工构建验证与后续板测。

---

## 21. RTOS Platform Phase 1 — Keil Build Verification — 2026-08-30

状态：

```text
CODE_COMPLETE_PENDING_HARDWARE_VERIFICATION
```

### Keil Build

用户已完成当前 MDK 工程编译并确认：

```text
0 Error(s)
```

- Keil Integration 已完成；当前阶段不再处于 `CODE_COMPLETE_PENDING_KEIL_VERIFICATION`。
- Board Smoke Test 尚未执行，仍不得将 RTOS Platform Phase 1 标记为 `COMPLETED`。
- 本轮仅更新验证记录，未改动任何 C 源码、CubeMX、Vendor、UART DMA Impl、UART Service、RingBuffer
  或 service_log。

### UART Phase 2A 恢复后构建记录

本次当前工程 Keil `0 Error(s)` 发生在 UART Phase 2A 临时板测代码已恢复之后，补齐了其最后一项
恢复后构建验证。因此 UART Phase 2A 满足既有 Host、Keil、真实板测、临时测试恢复和恢复后 Rebuild
验收条件，状态更新为：

```text
COMPLETED
```

---

## 22. RTOS Platform Phase 1 — Board Smoke Test Code Ready — 2026-08-30

状态：

```text
BOARD_TEST_CODE_READY
```

### 临时测试代码

- 仅在 `Core/Src/freertos.c` 的 CubeMX `USER CODE` 区加入临时 RTOS Platform Smoke Test。
- 通过独立的 2 KiB Controller Thread 运行测试，保持 CubeMX 默认任务为空闲循环，避免默认 512-byte
  栈承载测试状态机。
- 自动验证：Time/Delay、Thread current/create/run/terminate、Mutex、Semaphore、Queue 的
  Producer/Consumer 顺序、Task Notification、周期 Software Timer。
- ISR Notification 使用既有已验证的 USART1 Platform UART DMA RX 路径：RX_DATA 回调中仅调用
  `platform_notify_set_from_isr()`，不记录日志、不阻塞、不分配、不解析数据。
- 所有测试结果及最终汇总仅由 Task Context 经 Platform Log / RTT 输出。

### 人工验证步骤

1. 在 MDK 工程执行 `Clean Targets -> Rebuild all target files`，确认实际 `0 Error(s)`。
2. 下载到目标板并打开 RTT。
3. 看到 `NOTIFY ISR READY` 后，在 60 秒内向 USART1 发送任意短数据。
4. 收集完整 RTT 输出；只有出现以下最终行，才可记录板测 PASS：

```text
RTOS PLATFORM BOARD TEST: PASS
```

### Coding Standard Review

```text
Coding Standard Review: PASS
```

- 临时自研代码已检查 USER CODE 边界、文件级状态命名、函数命名、中文设计/并发注释、显式大括号、
  资源生命周期、返回值、ISR/Task 边界、Yoda Condition、TAB 和超长行。
- `Core/Src/freertos.c` 是 CubeMX 文件；未修改 USER CODE 区之外的生成逻辑，格式保持局部 CubeMX
  风格作为生成文件例外。

### 当前限制

- 未执行本轮 Keil 构建，未下载，未取得任何新的 RTT 或板测结果。
- 临时代码尚未恢复，RTOS Platform Phase 1 仍为
  `CODE_COMPLETE_PENDING_HARDWARE_VERIFICATION`，不得标记 `COMPLETED`。

---

## 23. RTOS Platform Phase 1 — Board Smoke Test PASS / Restore Complete — 2026-08-30

状态：

```text
CODE_COMPLETE_PENDING_KEIL_VERIFICATION
```

### Board Verification

用户提供的真实 RTT 输出确认第二轮板测完整通过：

```text
TIME                PASS
THREAD              PASS
MUTEX               PASS
SEMAPHORE           PASS
QUEUE               PASS
NOTIFY TASK         PASS
TIMER               PASS
NOTIFY ISR          PASS
RTOS PLATFORM BOARD TEST: PASS
```

- `NOTIFY ISR PASS` 表明用户在 `NOTIFY ISR READY` 后发送的短 USART1 数据，经既有 Platform UART
  RX_DATA ISR 回调调用 `platform_notify_set_from_isr()` 后成功唤醒 Task。
- 首轮输出停在 ISR 等待阶段；第二轮在约 20571 ms 收到 ISR 通知，并在约 20572 ms 输出最终 PASS。

### Temporary Test Restore

- `Core/Src/freertos.c` 的临时 RTOS Board Test 已全部移除。
- 恢复后的 `freertos.c` 已与板测前 commit `03c6d98` 完全一致；无遗留 Thread、Timer、Queue、
  Buffer、Flag、UART Callback 或临时日志。
- `git diff --check`：PASS。

### Remaining Verification

- 仍需对恢复后的当前 MDK 工程执行 `Clean Targets -> Rebuild all target files` 并确认实际
  `0 Error(s)`。
- 仅完成这次恢复后 Rebuild，RTOS Platform Phase 1 才可标记为 `COMPLETED`。
