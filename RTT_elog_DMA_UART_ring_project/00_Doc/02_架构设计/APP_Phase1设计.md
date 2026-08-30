# APP Phase 1 设计

> 文档类型：专项设计 / Frozen Design Contract  
> 状态：FROZEN  
> 版本：V1.0  
> 日期：2026-08-30

---

## 1. 背景

当前工程已经完成并验证：

```text
Platform UART Phase 1
Platform UART DMA RX Phase 2A
Platform OS Phase 1
SPSC RingBuffer Phase 1
UART Service Phase 1
Platform BSP UART Binding Phase 1
```

当前真实 RX 链路已经到达任务上下文：

```text
USART1
  ↓
DMA Circular + IDLE / HT / TC
  ↓
STM32 UART Impl
  ↓
Platform UART
  ↓
UART Service
  ↓
SPSC RingBuffer
  ↓
Platform Notify
  ↓
Dedicated Consumer Task
```

此前真实板测中的 Consumer Task 只是临时验证代码，`01_APP/` 仍没有正式生产 APP 实现。

APP Phase 1 的目标是把已经验证的基础能力正式装配进 `01_APP/`，建立一个长期可扩展的产品级启动入口和 Communication Task，而不是继续在 CubeMX `freertos.c` 中堆积临时业务逻辑。

Platform BSP UART Binding Phase 1 已解决 APP 不得直接依赖 Impl 的装配缺口：

```text
Communication UART
    ↓
Platform BSP
    ↓
Impl BSP
    ↓
USART1
```

因此 APP Phase 1 可以只依赖 Service 与 Platform，不需要知道 `impl_platform_uart_usart1_construct()`、`huart1`、DMA Stream 或 STM32 HAL。

---

## 2. 固定依赖规则

当前架构依赖规则冻结为：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

APP 允许使用：

```text
UART Service
Platform BSP UART
Platform UART
Platform OS
Platform Log
Platform Common
```

APP 禁止直接 include 或使用：

```text
impl_platform_uart.h
impl_freertos_*.h
usart.h
UART_HandleTypeDef
DMA_HandleTypeDef
STM32 HAL UART / DMA API
cmsis_os2.h
FreeRTOS.h / task.h
具体 USART1 / DMA Stream / IRQ 资源
```

CubeMX 生成文件可以知道 HAL / CMSIS，但只能作为薄启动入口，不能重新承担 APP 业务职责。

---

## 3. Phase 1 目标

APP Phase 1 只建立第一条正式 APP 垂直链路：

```text
System Composition
      ↓
Communication Task
      ↓
UART Service wait_event()
      ↓
Drain RX byte stream
      ↓
APP byte-stream consumer
```

本阶段必须解决：

1. APP 持有 Communication UART、UART Service、Communication Task 与 backing storage；
2. APP 通过 Platform BSP 构造逻辑 Communication UART；
3. APP 通过 Platform Thread 创建正式 Communication Task；
4. APP 正确编排 Platform UART lifecycle 与 UART Service lifecycle；
5. Consumer Task 正确处理 `RX_AVAILABLE / DATA_LOSS / ERROR / STOPPED`；
6. Consumer Task 永远以 Service / RingBuffer 状态为真值，不把一次 Notify 当作一个数据包；
7. DATA_LOSS 与 UART ERROR 在 Task Context 中执行明确恢复；
8. CubeMX `freertos.c` 只保留一个薄 `app_system_init()` 调用入口；
9. 建立 Host / Keil / Board 验证门禁。

---

## 4. Phase 1 不做什么

本阶段明确不实现：

```text
Protocol Parser
Frame Queue
Command Dispatcher
UART Echo
Async TX Service
多 UART
多 Consumer
动态内存
Device Registry
通用 Factory / IoC Container
Message Bus / Event Bus
复杂 APP Manager
完整系统 Shutdown / Restart Framework
Watchdog Policy
业务协议状态机
```

Phase 1 的 Consumer 只消费连续字节流并维护最小 APP 统计。

后续 Protocol Parser 应接在 APP byte-stream consumer 后面，而不是倒逼 UART Service 改成 frame API。

---

## 5. APP 文件结构

Phase 1 采用两个模块：

```text
01_APP/
├── app_system.h
├── app_system.c
├── app_communication.h
└── app_communication.c
```

不增加：

```text
app_manager
app_device_manager
app_event_bus
app_factory
app_registry
```

### 5.1 app_system

职责：

- 作为当前产品 Composition Root；
- 持有静态对象和 backing storage；
- 定义 Communication UART 产品配置；
- 通过 Platform BSP 构造 Communication UART；
- 初始化 APP Communication 对象；
- 创建 Communication Task；
- 初始化 UART Service；
- 对启动装配失败执行同步回滚或返回错误；
- 不执行长期通信事件循环。

### 5.2 app_communication

职责：

- 作为正式 Communication Task 的行为模块；
- Scheduler 启动后执行 Platform UART hardware lifecycle start sequence；
- 开启 UART Service RX Session；
- `service_uart_wait_event()`；
- drain RingBuffer；
- 处理 DATA_LOSS；
- 处理 UART ERROR；
- 识别非预期 STOPPED；
- 维护 APP 自己拥有的消费 / 恢复统计；
- 使用 Platform Log 输出低频错误与恢复信息。

`app_communication` 不拥有 DMA / RingBuffer 算法，也不解析协议帧。

---

## 6. APP 对象与存储所有权

Phase 1 继续采用静态 / caller-owned allocation。

建议 `app_system.c` 持有：

```text
platform_uart_t              g_communicationUart
service_uart_t               g_uartService
platform_thread_t            g_communicationThread
app_communication_t          g_appCommunication
uint8_t                      g_uartDmaRxBuffer[]
uint8_t                      g_uartRingBufferStorage[]
```

所有权冻结为：

```text
platform_uart_t storage       -> APP owns
service_uart_t storage        -> APP owns
platform_thread_t object      -> APP owns lifecycle
app_communication_t           -> APP owns
DMA RX backing storage        -> APP owns
RingBuffer backing storage    -> APP owns

Communication UART binding    -> Platform BSP / Impl owns mapping
Platform UART hardware state  -> APP controls through Platform lifecycle
Active RX Session             -> UART Service owns
DMA RX writer                 -> DMA / STM32 Impl
RingBuffer Producer           -> UART Service callback
RingBuffer Consumer           -> Communication Task
```

不使用 `malloc/free`。

---

## 7. APP Communication 数据模型

APP Communication 只保存 APP 真正拥有的运行语义，不复制 UART Service 已有统计。

### 7.1 State

建议：

```c
typedef enum
{
    APP_COMMUNICATION_STATE_UNINITIALIZED = 0,
    APP_COMMUNICATION_STATE_INITIALIZED,
    APP_COMMUNICATION_STATE_RUNNING,
    APP_COMMUNICATION_STATE_ERROR,
    APP_COMMUNICATION_STATE_MAX
} app_communication_state_t;
```

Phase 1 不增加复杂 STOPPING / STOPPED 状态机，因为系统正常运行后 Communication Task 不提供产品级 shutdown API。

UART Session 的 STOPPING / STOPPED 仍由 UART Service 管理。

### 7.2 Config

建议：

```c
typedef struct
{
    platform_uart_t *uart;
    service_uart_t *service;
} app_communication_config_t;
```

APP Communication 不保存：

```text
USART instance
DMA handle
Platform Notify flags
RingBuffer object
HAL callback
```

### 7.3 Runtime Context

建议：

```c
typedef struct
{
    app_communication_state_t state;
    platform_error_t lastError;
} app_communication_context_t;
```

### 7.4 APP Statistics

建议只维护：

```c
typedef struct
{
    uint32_t processedChunkCount;
    uint32_t processedByteCount;
    uint32_t dataLossRecoveryCount;
    uint32_t uartErrorRecoveryCount;
    uint32_t fatalErrorCount;
} app_communication_statistics_t;
```

不复制：

```text
rxBytesReceived
rxBytesBuffered
rxBytesDropped
ringBufferHighWaterMark
uartErrorCount
```

这些仍由 UART Service 作为唯一来源。

---

## 8. 建议公共接口

### 8.1 app_system.h

Phase 1 只需要：

```c
platform_error_t app_system_init(void);
```

冻结调用合同：

> `app_system_init()` 必须在 `osKernelInitialize()` 完成后、`osKernelStart()` 之前调用，并且整个系统启动阶段只调用一次。

APP 本身不调用 CMSIS API；该时序由 CubeMX 启动入口保证。

### 8.2 app_communication.h

建议：

```c
#define APP_COMMUNICATION_INITIALIZER {0}

platform_error_t app_communication_init(
    app_communication_t *communication,
    const app_communication_config_t *config);

platform_error_t app_communication_start(
    app_communication_t *communication);

platform_error_t app_communication_process(
    app_communication_t *communication,
    uint32_t timeoutMs);

void app_communication_task_entry(void *argument);
```

其中：

- `init()`：只绑定 APP 层依赖，不启动硬件；
- `start()`：只能在 Scheduler 已运行的 Task Context 中调用；
- `process()`：执行一次 wait / drain / recovery 周期，便于 Host Test；
- `task_entry()`：正式任务入口，内部调用 `start()` 并循环调用 `process()`。

是否提供 status/statistics getter 可在实现计划阶段根据 Host Test 与调试需求决定；不要求为了形式增加无用 API。

---

## 9. 系统启动分成两个阶段

Phase 1 必须明确区分：

```text
Pre-Scheduler Composition
Post-Scheduler Runtime Start
```

这是本阶段最重要的时序边界之一。

---

## 10. Pre-Scheduler Composition

当前 `main.c` 的顺序为：

```text
HAL / Clock
    ↓
MX_GPIO_Init()
MX_DMA_Init()
MX_USART1_UART_Init()
    ↓
osKernelInitialize()
    ↓
MX_FREERTOS_Init()
    ↓
osKernelStart()
```

`app_system_init()` 从 `MX_FREERTOS_Init()` 的 USER CODE 区调用。

推荐装配顺序：

```text
1. platform_bsp_uart_construct_communication()
      ↓
   Platform UART object = CREATED

2. app_communication_init()
      ↓
   APP Communication = INITIALIZED

3. platform_thread_create()
      ↓
   创建 Communication Task
   Task 尚不会运行，因为 Scheduler 尚未 start

4. service_uart_init()
      ↓
   绑定 Platform UART callback
   consumerThread handle 已存在

5. app_system_init() return OK

6. osKernelStart()
```

必须先创建 Consumer Task，再 `service_uart_init()`，因为 UART Service Config 需要有效的 `platform_thread_t *consumerThread`。

必须在 Scheduler 启动前完成 `service_uart_init()`，避免 Communication Task 运行后发现 Service 尚未初始化的竞态。

本阶段不在 Pre-Scheduler 阶段执行：

```text
Platform UART lifecycle start
service_uart_start()
DMA RX start
```

避免 Scheduler 尚未运行时产生 RX ISR -> Notify 链路。

---

## 11. Post-Scheduler Runtime Start

`osKernelStart()` 后，Communication Task 获得运行机会。

`app_communication_task_entry()` 首先调用：

```text
app_communication_start()
```

推荐顺序：

```text
Platform UART lifecycle init
        ↓
Platform UART lifecycle start
        ↓
service_uart_start()
        ↓
APP Communication -> RUNNING
```

这保持 UART Service 已冻结的合同：

```text
service_uart_init()
    before
Platform UART STARTED
    before
service_uart_start()
```

`service_uart_start()` 开启新的 RX Session，并通过 Platform UART async RX 启动 DMA 接收。

### 11.1 初始启动失败

任何一步失败：

```text
APP Communication -> ERROR
lastError = original error
fatalErrorCount++
```

若 UART 已 STARTED 但 `service_uart_start()` 失败，应 best-effort 停止 Platform UART，使硬件进入安全非活动状态；回滚失败不得覆盖原始启动错误。

Phase 1 不在启动失败后无限快速自动重试。

Task 进入低频 error idle，而不是 busy loop。

---

## 12. Communication Task 主循环

正常运行：

```text
for (;;)
{
    app_communication_process(...);
}
```

`app_communication_process()` 的逻辑顺序冻结为：

```text
service_uart_wait_event()
        ↓
RX_AVAILABLE ?
        ↓ yes
Drain all currently readable bytes
        ↓
Handle ERROR / DATA_LOSS / STOPPED
```

关键原则：

> 先消费已经正确进入 RingBuffer 的有效数据，再执行会开始新 Session 的恢复动作。

因为 `service_uart_start()` 会 reset RingBuffer；如果先 restart，再 drain，会主动丢弃仍然有效的数据。

---

## 13. RX_AVAILABLE 处理

`SERVICE_UART_EVENT_RX_AVAILABLE` 不代表一个 packet / frame。

处理必须循环调用：

```c
service_uart_read(...)
```

直到：

```text
PLATFORM_ERR_EMPTY
```

每次成功读取：

```text
processedChunkCount++
processedByteCount += readLength
```

然后把该连续字节片段交给：

```text
APP byte-stream consumer
```

Phase 1 的 consumer 不解析帧，只完成字节消费与统计。

后续 Protocol Parser 应在这里接入。

禁止：

```text
把一次 wait_event 当作一个完整帧
假设一次 service_uart_read 就能 drain 全部数据
根据 DMA callback 边界拆业务消息
```

---

## 14. Combined Event 处理顺序

UART Service 明确允许组合事件，例如：

```text
RX_AVAILABLE | ERROR
RX_AVAILABLE | DATA_LOSS
RX_AVAILABLE | DATA_LOSS | ERROR
```

因此 APP 禁止使用互斥 `if / else if` 直接丢掉其他事件。

固定处理优先级：

```text
1. RX_AVAILABLE
   -> drain valid buffered bytes

2. ERROR
   -> UART ERROR recovery

3. DATA_LOSS
   -> DATA_LOSS recovery

4. STOPPED
   -> unexpected stop handling
```

恢复决策优先级：

```text
ERROR > DATA_LOSS
```

原因：

- ERROR 时 Service 已处于 ERROR，当前 RX Session 已结束；
- 从 ERROR 直接 `service_uart_start()` 就会开启新 Session，同时清除当前 Session 的 sticky data-loss；
- 不需要先执行 DATA_LOSS 的 stop + start。

---

## 15. DATA_LOSS Recovery

`SERVICE_UART_EVENT_DATA_LOSS` 是当前 Session sticky status。

如果 APP 只记录日志然后继续下一轮 `wait_event()`：

```text
DATA_LOSS 仍为 TRUE
    ↓
wait_event() 立即再次返回
    ↓
Consumer Task 可能形成 busy loop
```

因此 Phase 1 必须结束当前 Session 并开始新 Session。

固定顺序：

```text
1. drain RingBuffer 中仍然有效的数据

2. service_uart_stop()
      ↓
   cancel current RX Session

3. 检查真实 Service state

4. service_uart_start()
      ↓
   new RX Session
   RingBuffer reset
   dataLossOccurred clear

5. dataLossRecoveryCount++
```

### 15.1 stop() 返回错误的特殊语义

UART Service 已冻结：

> `service_uart_stop()` 可能在 RX 已经成功 cancel、Service 已经 STOPPED 后，因为 Task-context notify 失败而返回错误。

因此 APP 不能只根据返回值判断 Session 是否仍运行。

若 `service_uart_stop()` 返回错误：

```text
service_uart_get_status()
        ↓
state == STOPPED
        -> 允许继续 service_uart_start()

state != STOPPED
        -> recovery failed -> APP ERROR
```

不得直接重复 cancel。

---

## 16. UART ERROR Recovery

当收到：

```text
SERVICE_UART_EVENT_ERROR
```

UART Service 已进入：

```text
SERVICE_UART_STATE_ERROR
```

且 Platform UART ERROR 已经结束当前 RX Session。

固定恢复顺序：

```text
1. drain remaining RingBuffer

2. service_uart_get_status()
      ↓
   保存 / log lastError

3. service_uart_start()
      ↓
   ERROR -> RUNNING

4. uartErrorRecoveryCount++
```

APP 不调用：

```text
platform_uart_cancel(RX)
service_uart_stop()
```

因为 ERROR Session 已经结束。

Phase 1 只做一次直接恢复尝试。

若 `service_uart_start()` 失败：

```text
APP Communication -> ERROR
fatalErrorCount++
```

不做无限重试，不在短周期内制造 restart storm。

后续如果真实产品需要 bounded retry / backoff，应另行设计。

---

## 17. STOPPED 处理

正常 APP Phase 1 不会主动永久 stop UART Service。

DATA_LOSS recovery 中的 STOPPED 是 `service_uart_stop()` 同步过程内部的预期中间结果，并会立即 start 新 Session。

因此如果正常事件循环独立观察到：

```text
SERVICE_UART_EVENT_STOPPED
```

视为：

```text
unexpected stop / composition contract violation
```

Phase 1 不自动掩盖该问题。

处理：

```text
APP Communication -> ERROR
lastError = PLATFORM_ERR_CANCELED（或实现中等价错误）
fatalErrorCount++
log error
```

不自动无限 restart。

---

## 18. wait_event timeout

`service_uart_wait_event()` 的 timeout 表示当前没有新事件，不是通信故障。

Phase 1 规定：

```text
PLATFORM_ERR_TIMEOUT
    -> normal idle
    -> 不改变 APP state
    -> 进入下一轮 process
```

不因为 timeout 重启 UART。

等待时间只是调度 / 健康检查策略，不影响 RingBuffer 真值语义。

初始建议：

```text
APP_COMMUNICATION_WAIT_TIMEOUT_MS = 1000 ms
```

该值属于产品配置，不属于 UART Service 合同。

---

## 19. Fatal Error 行为

Phase 1 不建立完整 System Supervisor。

一旦 APP Communication 进入 ERROR：

```text
停止正常 wait / recovery 循环
记录 lastError
输出一次低频错误日志
进入低频 error idle
```

error idle 可使用：

```text
platform_time_delay_ms()
```

禁止错误路径形成无延时 busy loop 或高频日志刷屏。

后续 Watchdog / system reset / supervisor 策略不在本阶段设计。

---

## 20. Platform UART lifecycle 使用规则

APP 拥有 Platform UART hardware lifecycle。

允许调用：

```text
uart.device.lifecycle->init(uart)
uart.device.lifecycle->start(uart)
uart.device.lifecycle->stop(uart)
uart.device.lifecycle->deinit(uart)
```

但 APP 不直接访问 `implContext`，也不根据其中 STM32 类型做判断。

Phase 1 正常运行路径只需要：

```text
init -> start
```

错误启动回滚可 best-effort `stop`。

完整产品 shutdown 的：

```text
service stop
UART stop
service deinit
UART deinit
Task terminate
```

保留给后续系统生命周期设计，不在 Phase 1 对外提供 runtime shutdown API。

---

## 21. CubeMX / FreeRTOS 入口设计

当前 `Core/Src/freertos.c` 仍存在 CubeMX 默认：

```text
defaultTask
USART1_mutex_Init()
fputc / UART printf 相关历史依赖
```

这些属于已知技术债。

APP Phase 1 不为了清洁架构扩大到 `.ioc` / CubeMX 全量重构。

### 21.1 本阶段允许的 CubeMX 修改

仅在 USER CODE 区：

```text
#include "app_system.h"
```

并在 `MX_FREERTOS_Init()` 的 USER CODE Init 中调用：

```text
app_system_init()
```

若返回失败，由 CubeMX glue 调用现有 `Error_Handler()`；APP 模块本身不得 include `main.h` 或直接调用 HAL Error Handler。

目标形态：

```text
freertos.c
    ↓ thin call
app_system_init()
    ↓
APP / Service / Platform
```

### 21.2 defaultTask

Phase 1 暂时保留 CubeMX `defaultTask` 空循环，不把它作为正式 Communication Task。

正式 Communication Task 必须由 APP 通过：

```c
platform_thread_create()
```

创建。

原因：

- 保持 APP 对 Task lifecycle 的所有权；
- 不让 APP 依赖 CMSIS concrete task；
- 避免本阶段为了删除 defaultTask 修改 `.ioc` / 大量生成代码。

后续项目清理阶段再删除无意义 defaultTask。

---

## 22. 初始产品参数

以下参数属于 APP 产品配置，不属于可复用 Platform / Service 合同。

推荐初始值：

```text
Communication UART
    baudRate      = 115200
    dataBits      = 8
    stopBits      = 1
    parity        = NONE
    flowControl   = NONE

DMA RX Buffer
    size          = 128 bytes

RingBuffer Storage
    size          = 512 bytes
    usable        = 511 bytes

APP Read Buffer
    size          = 128 bytes

Communication Task
    priority      = PLATFORM_THREAD_PRIORITY_NORMAL
    stack         = 1024 bytes（初始值，真实板测后评估）

wait_event timeout
    1000 ms
```

这些数值可以在实现计划或板测后调整，但调整不能改变层级、所有权或 Service 事件语义。

APP Read Buffer 可作为 Communication Task 内部静态存储或 Task stack 局部数组；Phase 1 不要求把它提升为 Service backing storage。

---

## 23. Logging 策略

APP 可以直接使用 Platform Log。

建议仅在以下节点记录：

```text
Communication runtime start success
DATA_LOSS recovery
UART ERROR + lastError
recovery failure
unexpected STOPPED
fatal APP communication error
```

正常 RX chunk 不逐块打印，避免日志本身改变通信实时性。

ISR 中没有 APP Log。

UART Service callback 仍保持无大量日志的冻结约束。

---

## 24. Host Test 设计

APP Phase 1 的软件逻辑应优先通过 fake Service / Platform 验证。

至少覆盖：

### 24.1 app_system

```text
BSP UART construct success
APP Communication init success
Platform Thread create success
UART Service init success
正确对象 / backing storage 传递
失败时不继续后续装配
APP 不直接调用任何 Impl API
```

### 24.2 app_communication start

```text
UART lifecycle init -> start -> service start 顺序正确
UART init failure -> APP ERROR
UART start failure -> APP ERROR
service start failure -> best-effort UART stop + APP ERROR
successful start -> RUNNING
```

### 24.3 RX drain

```text
RX_AVAILABLE -> read until EMPTY
多次 read 才 drain 完成
processedChunkCount 正确
processedByteCount 正确
不把 wake 当 frame
```

### 24.4 Combined Event

```text
RX_AVAILABLE | ERROR
    -> drain first
    -> ERROR restart

RX_AVAILABLE | DATA_LOSS
    -> drain first
    -> stop + restart

RX_AVAILABLE | DATA_LOSS | ERROR
    -> drain first
    -> ERROR recovery only
    -> 不重复执行 DATA_LOSS stop/start
```

### 24.5 DATA_LOSS

```text
stop OK -> start
stop error + actual STOPPED -> 仍允许 start
stop error + actual RUNNING -> APP ERROR
restart failure -> APP ERROR
```

### 24.6 ERROR

```text
ERROR -> no cancel
ERROR -> direct service start
restart success -> RUNNING
restart failure -> APP ERROR
```

### 24.7 Other

```text
wait timeout -> normal idle
unexpected STOPPED -> APP ERROR
fatal error 不进入高频 retry
```

---

## 25. Keil Integration Gate

APP 实现完成后至少要求：

```text
01_APP sources added to Keil project
required APP include path added
Platform BSP / UART / OS / Log linkage remains intact
Full Rebuild = 0 Error(s)
new warnings reviewed
```

不得为了编译通过让 APP include Impl / HAL private headers。

---

## 26. Target Board Verification

APP Phase 1 必须重新做一次真实板级验证，因为这次验证的是：

> 临时 Consumer Task 已经被正式 `01_APP/` 生产链路替代。

推荐继续复用 UART Service Phase 1 已验证的数据模式：

```text
00..FF repeated
raw binary
总长度 1280 bytes
```

至少证明：

```text
APP Communication enters RUNNING
UART Service enters RUNNING
1280 bytes all consumed by APP
processedByteCount = 1280
service rxBytesReceived = 1280
service rxBytesDropped = 0
no mismatch in temporary board verification hook
no ERROR
no DATA_LOSS
```

为了验证数据内容，可临时加入集中、可恢复的 board-test compare hook；验证完成后必须删除临时逻辑，再执行一次 Keil Full Rebuild。

本阶段不要求为了制造异常而在真实板上强行触发 UART ERROR / RingBuffer Overflow；这些恢复逻辑由 Host Test 覆盖。

---

## 27. Scope Guard

APP Phase 1 实施过程中如果发现必须修改以下已冻结合同：

```text
Platform UART public API
Platform BSP UART public API
UART Service public API
UART Service event semantics
RingBuffer SPSC contract
Platform Notify contract
Platform Thread public API
```

则：

```text
STOP / BLOCKED
```

返回设计评审。

不得以“APP 使用方便”为理由把：

```text
HAL handle
DMA handle
USART1
CMSIS thread handle
FreeRTOS TaskHandle_t
```

暴露给 APP。

---

## 28. Phase 1 完成条件

APP Phase 1 只有在以下全部满足后才能标记 COMPLETED：

```text
[ ] APP / Service / Platform / Impl 依赖边界正确
[ ] app_system 正式成为产品 Composition Root
[ ] Communication Task 由 Platform Thread 创建
[ ] Pre-Scheduler composition 时序验证
[ ] Post-Scheduler UART / Service start 时序验证
[ ] RX_AVAILABLE drain 逻辑 Host Test PASS
[ ] Combined Event Host Test PASS
[ ] DATA_LOSS recovery Host Test PASS
[ ] UART ERROR recovery Host Test PASS
[ ] unexpected STOPPED / timeout Host Test PASS
[ ] Existing UART Service regression PASS
[ ] Existing Platform UART / OS regression PASS
[ ] Coding Standard Review PASS
[ ] Keil Full Rebuild 0 Error(s)
[ ] 正式 APP RX board test PASS
[ ] 临时 board-test hook 清理后 Rebuild PASS
[ ] handoff 更新真实验证结果
```

---

## 29. 设计结论

APP Phase 1 的核心不是增加新的通信算法，而是把已经验证的底层能力按正确所有权和生命周期正式装配起来：

```text
CubeMX startup
    ↓
app_system
    ├── Platform BSP Communication UART
    ├── Platform Thread
    ├── UART Service
    └── caller-owned storage
           ↓
app_communication task
    ↓
UART Service event / RingBuffer
    ↓
APP byte-stream consumer
```

Phase 1 完成后，项目第一次拥有真正从硬件 ISR 一直延伸到 `01_APP/` 的生产垂直链路。

下一阶段才根据真实需求接入 Protocol Parser / command processing / async TX 等上层能力。
