# APP Phase 1 Config 补充设计

> 文档类型：专项设计补充 / Frozen Design Addendum  
> 状态：FROZEN  
> 版本：V1.0  
> 日期：2026-08-30  
> 主设计：`00_Doc/02_架构设计/APP_Phase1设计.md`

---

## 1. 目的

APP Phase 1 主设计已经冻结，但其中 UART 参数、Buffer 大小、Task 参数和等待时间等产品级静态设置原本计划直接定义在 APP 源文件中。

本补充设计将这类“编译期确定、修改后重新编译生效”的产品级静态参数统一收敛到：

```text
00_Config/project_config.h
```

该调整只改变静态参数的归属位置，不改变 APP / Service / Platform / Impl 的依赖边界、对象所有权、生命周期和运行时行为。

---

## 2. 00_Config 的职责

`00_Config/` 定义为：

> 当前产品的软件静态配置中心，保存编译期确定、产品级可调整、通常修改后需要重新编译生效的参数。

Phase 1 使用：

```text
00_Config/
├── README.md
└── project_config.h
```

当前规模不拆分多个 config header。只有真实配置量明显增长后，才考虑按 communication / sensor / display 等领域拆分。

---

## 3. 可以放入 00_Config 的内容

Phase 1 固定收敛以下参数：

```text
Communication UART
    baudRate
    dataBits
    stopBits
    parity
    flowControl
    defaultTimeoutMs

Communication buffers
    DMA RX buffer size
    RingBuffer storage size
    APP read buffer size

Communication Task
    stack size bytes
    priority

Communication runtime policy
    wait_event timeout
    fatal error idle delay
```

当前冻结值：

```text
UART baudRate                 115200
UART dataBits                 PLATFORM_UART_DATA_BITS_8
UART stopBits                 PLATFORM_UART_STOP_BITS_1
UART parity                   PLATFORM_UART_PARITY_NONE
UART flowControl              PLATFORM_UART_FLOW_CONTROL_NONE
UART defaultTimeoutMs         1000 ms
DMA RX buffer                 128 bytes
RingBuffer storage            512 bytes
APP read buffer               128 bytes
Communication Task stack      1024 bytes
Communication Task priority   PLATFORM_THREAD_PRIORITY_NORMAL
wait_event timeout            1000 ms
fatal error idle delay        1000 ms
```

---

## 4. project_config.h 公共依赖边界

`project_config.h` 允许引用产品配置所需的公开 Platform 类型 / 枚举，例如：

```text
platform_uart_types.h
platform_thread.h
```

但禁止依赖：

```text
Impl header
STM32 HAL
CMSIS-RTOS2 concrete API
FreeRTOS concrete API
usart.h
DMA handle
USART instance
TaskHandle_t
```

因此允许：

```c
#define PROJECT_COMM_UART_DATA_BITS \
    PLATFORM_UART_DATA_BITS_8

#define PROJECT_COMM_TASK_PRIORITY \
    PLATFORM_THREAD_PRIORITY_NORMAL
```

而不允许在配置头中出现：

```c
&huart1
DMA2_Stream2
impl_platform_uart_usart1_construct
osPriorityNormal
TaskHandle_t
```

---

## 5. 推荐宏命名

Phase 1 使用统一 `PROJECT_` 前缀：

```c
#define PROJECT_COMM_UART_BAUD_RATE              (115200U)
#define PROJECT_COMM_UART_DATA_BITS              PLATFORM_UART_DATA_BITS_8
#define PROJECT_COMM_UART_STOP_BITS              PLATFORM_UART_STOP_BITS_1
#define PROJECT_COMM_UART_PARITY                 PLATFORM_UART_PARITY_NONE
#define PROJECT_COMM_UART_FLOW_CONTROL           PLATFORM_UART_FLOW_CONTROL_NONE
#define PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS     (1000U)

#define PROJECT_COMM_DMA_RX_BUFFER_SIZE           (128U)
#define PROJECT_COMM_RING_BUFFER_STORAGE_SIZE     (512U)
#define PROJECT_COMM_READ_BUFFER_SIZE             (128U)

#define PROJECT_COMM_TASK_STACK_SIZE_BYTES        (1024U)
#define PROJECT_COMM_TASK_PRIORITY                PLATFORM_THREAD_PRIORITY_NORMAL

#define PROJECT_COMM_WAIT_TIMEOUT_MS              (1000U)
#define PROJECT_COMM_ERROR_IDLE_DELAY_MS          (1000U)
```

宏只表达产品参数，不表达对象身份、运行时地址或实现资源。

---

## 6. Config 与模块 Config struct 的区别

必须区分：

```text
00_Config/project_config.h
    = 产品应该配置成什么样

platform_uart_config_t
service_uart_config_t
app_communication_config_t
    = 模块在当前系统中如何被装配
```

例如 `app_system.c` 仍负责：

```c
static const platform_uart_config_t g_communicationUartConfig = {
    .baudRate = PROJECT_COMM_UART_BAUD_RATE,
    .dataBits = PROJECT_COMM_UART_DATA_BITS,
    .stopBits = PROJECT_COMM_UART_STOP_BITS,
    .parity = PROJECT_COMM_UART_PARITY,
    .flowControl = PROJECT_COMM_UART_FLOW_CONTROL,
    .defaultTimeoutMs = PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS
};
```

同时 `service_uart_config_t` 中的：

```text
uart pointer
DMA buffer pointer
RingBuffer storage pointer
consumerThread pointer
```

继续在 `app_system.c` 中装配，不进入 `00_Config`。

---

## 7. 禁止放入 00_Config 的内容

以下内容不是产品静态设置，不进入 `project_config.h`：

```text
platform_uart_t object / pointer
service_uart_t object / pointer
platform_thread_t object / pointer
app_communication_t object / pointer
DMA buffer address
RingBuffer storage address
runtime state
statistics
lastError
HAL handle
DMA handle
USART instance
Impl context
callback pointer
Platform BSP concrete mapping
```

尤其当前：

```text
Communication UART -> USART1
```

仍由 Platform BSP / Impl 负责，而不是通过 `project_config.h` 暴露给 APP。

---

## 8. 依赖关系

加入 Config 后，产品级依赖关系为：

```text
00_Config
    ↓ public compile-time settings
APP
├── Service
└── Platform
     ↓
    Impl
```

`00_Config` 可以使用公开 Platform 配置枚举，但不得绕过 Platform 直接依赖 Impl / Vendor。

---

## 9. APP Phase 1 文件结构补充

APP Phase 1 实施后预期相关结构：

```text
00_Config/
├── README.md
└── project_config.h

01_APP/
├── app_system.h
├── app_system.c
├── app_communication.h
└── app_communication.c
```

职责：

```text
project_config.h
    -> 产品参数

app_system.c
    -> 参数 + 对象 + storage 的 Composition

app_communication.c
    -> Runtime behavior
```

---

## 10. 验收条件

APP Phase 1 除主设计验收条件外，还必须满足：

```text
[ ] project_config.h 存在并集中保存 Phase 1 产品静态参数
[ ] 00_Config/README.md 明确静态配置边界
[ ] APP 源文件不重复硬编码已集中配置的产品参数
[ ] project_config.h 不包含 Impl / HAL / CMSIS / FreeRTOS concrete dependency
[ ] 对象地址、storage 地址和 runtime state 未进入 project_config.h
[ ] app_system.c 仍负责构造 platform_uart_config_t / service_uart_config_t 等运行时装配对象
[ ] Communication UART -> USART1 映射仍只存在于 Platform BSP / Impl
```

本补充设计与 `APP_Phase1设计.md` 一起构成 APP Phase 1 的冻结设计合同。