# Embedded Firmware Architecture Contract

> 文档类型：Architecture Contract
> 状态：Baseline
> 版本：V1.0
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

------

# 1. 架构目标

本工程采用分层式嵌入式软件架构：

```text
APP
 ↓
Service
 ↓
Platform
 ↓
Impl
 ↓
Vendor / HAL / Hardware
```

目标是：

- 降低业务逻辑与具体 MCU、HAL、RTOS 的耦合。
- 明确模块职责和数据所有权。
- 控制中断、任务和共享资源之间的并发复杂度。
- 提高代码可测试性、可维护性和可移植性。
- 限制底层实现变化向上层扩散。

本架构服务于工程，不追求为简单功能增加不必要的抽象。

------

# 2. 总体依赖规则

推荐依赖方向：

```text
APP
 ↓
Service
 ↓
Platform
 ↓
Impl
 ↓
Vendor / HAL / Hardware
```

必须遵守：

- 上层依赖下层提供的抽象能力。
- 下层不得依赖具体业务逻辑。
- APP 不直接依赖 HAL。
- Service 不直接依赖 STM32 HAL、UART Handle、DMA Handle。
- Platform 公共接口不依赖具体 MCU、HAL 或 FreeRTOS 类型。
- Impl 可以依赖 HAL、CMSIS、FreeRTOS 和 CubeMX Handle。
- Vendor 不依赖项目业务代码。
- 禁止为了快速解决问题长期保留跨层调用。

允许底层事件向上传递，但必须通过：

- Callback
- Event
- Notification
- 明确定义的接口

完成，不得由下层直接调用 APP 业务函数。

------

# 3. APP 层

目录：

```text
01_APP/
```

职责：

- 产品级业务流程。
- 应用状态机。
- 应用任务组织。
- 调用 Service。
- 根据 Service 数据执行业务行为。

APP 可以知道：

- 业务数据结构。
- Service 接口。
- 产品运行状态。

APP 不应知道：

- `UART_HandleTypeDef`
- `DMA_HandleTypeDef`
- USART1
- DMA Stream / Channel
- HAL UART API
- Ring Buffer 内部读写指针
- Vendor 具体实现

APP 原则上：

```text
APP → Service
```

而不是：

```text
APP → Platform
APP → HAL
```

若简单测试代码临时绕过 Service，必须明确标记为测试或过渡代码。

------

# 4. Service 层

目录：

```text
02_Service/
```

职责：

- 提供可复用软件服务。
- 管理数据流和运行上下文。
- 管理 Ring Buffer。
- 处理通信数据。
- 协议解析。
- 数据转换。
- 状态与统计。
- 将中断产生的数据转换为任务可消费的数据。

典型模块：

```text
UART Service
Ring Buffer
Log Service
Storage Service
Sensor Service
Protocol Parser
Firmware Update Service
```

Service 可以依赖：

```text
Platform
Service 内部通用模块
```

Service 不应直接依赖：

```text
STM32 HAL
USART1
DMA Stream
CubeMX Handle
Vendor 私有接口
```

Service 是：

> 硬件能力与业务逻辑之间的主要数据处理层。

------

# 5. Platform 层

目录：

```text
03_Platform/
```

Platform 定义：

> 上层需要什么能力。

而不是：

> 当前 STM32 如何实现这种能力。

Platform 主要子目录：

```text
platform_common/
platform_mcu/
platform_os/
platform_middleware/
platform_bsp/
```

Platform 可以定义：

- 公共数据类型。
- Error。
- Object。
- Device。
- Lifecycle。
- UART API。
- OS 抽象 API。
- Log 抽象 API。
- Storage 抽象 API。
- 其他硬件或系统能力接口。

Platform 公共头文件不得暴露：

```text
UART_HandleTypeDef
DMA_HandleTypeDef
TIM_HandleTypeDef
osThreadId_t
osMutexId_t
FreeRTOS TaskHandle_t
HAL_StatusTypeDef
USART_TypeDef
```

Platform 应尽可能保持：

```text
MCU independent
RTOS independent
Vendor independent
```

------

# 6. Platform Common 对象模型

当前工程采用统一基础对象模型。

基础关系：

```text
platform_object_t
        │
        ├── platform_device_t
        │       │
        │       └── platform_uart_t
        │
        └── platform_service_t
```

`platform_object_t` 用于保存：

- 对象身份。
- 对象类型。
- 名称。
- 生命周期状态。
- 父对象。
- 扩展上下文。
- Flags。

`platform_device_t` 在此基础上增加：

- 设备类别。
- 能力。
- 电源状态。
- 生命周期接口。

具体设备对象可以通过 C 结构体组合方式扩展 `platform_device_t`。

要求：

> 基础对象模型只负责所有对象共有的属性，不得逐渐堆积 UART、Sensor、Storage 等具体功能。

------

# 7. 生命周期模型

需要生命周期管理的对象统一采用：

```text
CREATED
   ↓
INITIALIZED
   ↓
STARTED
   ↓
STOPPED
   ↓
DEINIT
```

公共生命周期操作：

```text
init
start
process
stop
deinit
```

生命周期与数据操作必须区分。

例如 UART：

```text
Lifecycle:
init()
start()
stop()
deinit()

Data Ops:
write()
read()
writeAsync()
readAsync()
cancel()
```

禁止因为某个设备需要特殊操作就无限扩展公共生命周期接口。

------

# 8. UART Platform 架构

UART 当前采用：

```text
platform_uart_t
├── platform_device_t device
├── config
├── ops
├── implContext
├── callback
└── callbackContext
```

Platform UART 负责：

- UART 公共配置。
- UART 公共参数检查。
- UART 对象状态检查。
- 阻塞接口。
- 异步接口。
- 事件通知。
- Error 传播。
- Buffer 生命周期契约。

Platform UART 不负责：

- DMA Channel 选择。
- IDLE IRQ。
- HAL Callback。
- Ring Buffer。
- Protocol Parser。
- RTOS Task。
- Mutex 创建。
- USART1 资源分配。

UART 数据操作通过：

```text
platform_uart_ops_t
```

由 Impl 注入。

现有 Platform UART 公共接口视为当前阶段冻结接口。

除非经过架构评审，不得为了适配 Impl 随意修改 Platform UART API。

------

# 9. Impl 层

目录：

```text
04_Impl/
```

Impl 回答：

> Platform 定义的能力在当前目标平台上如何实现。

主要子目录：

```text
impl_board/
impl_mcu/
impl_os/
impl_middleware/
impl_bsp/
```

Impl 可以直接使用：

- STM32 HAL。
- CMSIS。
- FreeRTOS。
- CubeMX Handle。
- UART。
- DMA。
- IRQ。
- GPIO。
- TIM。
- EasyLogger。
- RTT。

Impl 负责：

- Platform Ops 实现。
- HAL Error 到 Platform Error 的转换。
- MCU Handle 绑定。
- DMA 状态维护。
- IRQ / HAL Callback 适配。
- RTOS API 适配。
- Vendor Port。

Impl 不负责：

- 产品业务。
- 完整协议解析。
- APP 状态机。
- 与硬件无关的通用数据处理。

------

# 10. Vendor 层

目录：

```text
05_Vendors/
Drivers/
Middlewares/
```

包括：

- STM32 HAL。
- CMSIS。
- FreeRTOS。
- EasyLogger。
- SEGGER RTT。
- 第三方协议或算法库。

原则：

> 尽量不修改第三方源码。

如果需要适配，应优先：

```text
Vendor
  ↑
Impl Adapter
  ↑
Platform
```

而不是：

```text
APP / Service
    ↓
Vendor API
```

------

# 11. CubeMX 生成代码边界

CubeMX 相关目录主要包括：

```text
Core/
Drivers/
Middlewares/
*.ioc
```

其中：

```text
main.c
freertos.c
usart.c
stm32f4xx_it.c
```

主要作为：

- 初始化入口。
- IRQ 入口。
- HAL Callback 入口。
- Scheduler 启动入口。
- 与自定义架构的薄适配入口。

长期业务逻辑不得堆积在这些文件中。

必须修改 CubeMX 文件时：

- 优先使用 `USER CODE` 区域。
- 修改内容必须尽量短。
- 不在生成文件中建立复杂状态机。
- 不在生成文件中实现完整 Service。
- 不将生成文件作为长期模块实现文件。

------

# 12. UART 数据流

目标 RX 数据流：

```text
USART Hardware
      ↓
DMA / IRQ
      ↓
STM32 UART Impl
      ↓
Platform UART Event
      ↓
UART Service
      ↓
Ring Buffer
      ↓
APP / Communication Task
```

目标 TX 数据流：

```text
APP
 ↓
Service
 ↓
Platform UART
 ↓
STM32 UART Impl
 ↓
HAL / DMA
 ↓
USART
```

每层只处理属于自己的职责。

------

# 13. DMA Buffer 与 Ring Buffer

必须区分：

```text
DMA Buffer
```

和：

```text
Ring Buffer
```

DMA Buffer：

- 属于 UART Impl。
- 面向硬件。
- 保存 DMA 刚接收到的数据。
- 生命周期与 DMA 配置相关。

Ring Buffer：

- 属于 Service 或独立通用软件模块。
- 面向数据流。
- 保存上层尚未消费的数据。
- 不知道 UART、DMA 和 HAL。

禁止让 Ring Buffer 直接访问：

```text
huart
hdma
DMA counter
UART registers
```

------

# 14. 数据所有权

所有跨层数据必须能够明确回答：

```text
Who owns it?
Who may write it?
Who may read it?
How long is it valid?
```

至少区分：

- DMA RX Buffer。
- Ring Buffer Storage。
- Platform Event Data。
- Service Context。
- APP Read Buffer。
- UART TX Buffer。

异步发送期间：

> TX Buffer 在完成、取消或错误事件发生前不得修改或失效。

异步接收事件中的数据：

> 默认只保证在接口约定的有效期内可访问，上层如需长期保存，应复制进入自己的存储区域。

禁止依赖未声明的 Buffer 生命周期。

------

# 15. 中断上下文规则

ISR / HAL Callback 只做最小必要工作。

允许：

- 读取硬件状态。
- 计算 DMA 新数据位置。
- 更新轻量 Context。
- 产生 Platform Event。
- 写入明确支持 ISR 使用的数据结构。
- 发送 ISR-safe Notification。

禁止：

- 阻塞。
- 普通 Mutex。
- 动态内存。
- 完整协议解析。
- 大量日志。
- 长时间循环。
- 长时间 HAL UART 输出。
- 调用非 ISR-safe RTOS API。

原则：

```text
ISR
 ↓
capture / notify
 ↓
exit quickly
```

复杂处理必须转入 Task Context。

------

# 16. Task Context 规则

Task Context 负责：

- Ring Buffer 消费。
- 协议解析。
- 状态机。
- 日志。
- 数据处理。
- 复杂错误恢复。
- 上层业务。

推荐：

```text
ISR
 ↓
Notification
 ↓
Communication Task
 ↓
Service
```

禁止使用 Task 轮询高速硬件状态来代替本应由 IRQ/DMA 完成的实时处理。

------

# 17. 并发原则

共享数据在设计前必须明确：

- ISR 是否访问。
- Task 是否访问。
- 是否单生产者。
- 是否单消费者。
- 是否需要临界区。
- 是否需要原子操作。
- 是否需要 Mutex。
- 是否需要 Semaphore / Notification。

不得因为存在 FreeRTOS 就默认所有共享数据都使用 Mutex。

ISR 与 Task 共享数据优先采用：

- SPSC 模型。
- 短临界区。
- ISR-safe Notification。
- 明确读写所有权。

Mutex 主要用于 Task 与 Task 之间的共享资源保护。

------

# 18. Error 处理架构

Error 处理采用分层原则。

Impl：

- 检测硬件和 Vendor Error。
- 转换为 Platform Error。
- 保存必要底层状态。

Platform：

- 校验参数与对象状态。
- 传播标准化 Error。

Service：

- 决定是否重试。
- 决定是否丢弃数据。
- 更新统计。
- 执行软件级恢复。

APP：

- 只处理影响产品行为的错误。

禁止底层发现普通通信错误后直接：

```text
while(1)
```

除非该错误确实属于不可恢复系统故障。

------

# 19. 日志架构

目标依赖：

```text
APP / Service
      ↓
Platform Log
      ↓
Impl Log Adapter
      ↓
EasyLogger / RTT
```

上层不得依赖：

- EasyLogger Header。
- RTT Header。
- Vendor Log API。

当前工程中的：

```text
Platform Log → easylogger_port.h
```

属于待清理技术债，不作为新模块参考模式。

后续新增模块不得复制这种依赖方式。

------

# 20. 基础类型边界

公共 Platform 类型应属于稳定的公共基础设施。

Platform 公共头文件不应长期依赖 Impl 私有类型。

当前：

```text
platform_types.h
    ↓
board_types.h
```

属于需要后续重新确认归属的技术债。

在明确迁移方案前：

- 不扩大这种依赖。
- 不让更多 Platform 接口直接依赖 Impl 私有头文件。

------

# 21. 配置模型

项目配置应按三类信息区分：

```text
Static Configuration
Runtime Context
Runtime Data
```

## Static Configuration

例如：

- Baud Rate。
- Buffer Size。
- Timeout。
- Task Priority。
- Feature Enable。

原则上初始化后很少改变。

## Runtime Context

例如：

- UART 当前状态。
- DMA 上次位置。
- Ring Buffer Index。
- Error Counter。
- Busy State。

用于描述模块当前运行环境。

## Runtime Data

例如：

- 实际接收字节。
- 发送 Payload。
- 协议数据。

不得将这三类数据无边界混合到单一大型结构体中。

------

# 22. Cross-Cutting Concerns

以下属于横切关注点：

- Logging。
- Error。
- Statistics。
- Trace。
- Assertions。
- Timing。
- Diagnostics。

这些能力可以跨模块使用，但不得因此破坏主业务依赖方向。

例如：

```text
Service → Platform Log
```

允许。

但：

```text
EasyLogger → Service
```

不允许。

横切能力只提供辅助观测，不拥有主业务流程。

------

# 23. 内存原则

核心通信链路默认使用：

```text
Static Allocation
```

优先用于：

- DMA Buffer。
- Ring Buffer。
- UART Context。
- Service Context。
- 固定 Task 资源。

正常收发路径禁止频繁：

```text
malloc/free
```

如必须动态分配，必须明确：

- 所有者。
- 释放者。
- 失败行为。
- 并发约束。
- Fragmentation 风险。

------

# 24. 接口设计原则

公共接口应：

- 语义明确。
- 参数方向明确。
- 返回值明确。
- Buffer 所有权明确。
- Error 语义明确。
- Context 明确。
- 支持独立测试。

避免：

- 隐式全局状态。
- 通过函数名无法判断阻塞/异步。
- 上层传入 HAL Handle。
- 接口同时承担配置、运行、协议处理等多种职责。

------

# 25. 文件与模块原则

一个模块至少应能够回答：

1. 本模块负责什么。
2. 本模块不负责什么。
3. 本模块依赖谁。
4. 谁依赖本模块。
5. 本模块的数据由谁拥有。
6. 本模块运行在哪种上下文。
7. 本模块如何测试。

文件数量不以多为目标。

如果一个简单模块能够通过：

```text
interface.h
implementation.c
```

清晰表达职责，则无需为了“分层形式完整”增加额外文件。

只有存在明确独立职责时才拆分文件。

------

# 26. 测试边界

优先测试硬件无关部分：

```text
Ring Buffer
Platform UART
Service State
Error Handling
Data Flow
```

Platform Unit Test：

- 不依赖 HAL。
- 不依赖真实 UART。
- 使用 Fake Ops。

Impl Test：

- 可以依赖 STM32。
- 重点验证 HAL / DMA / IRQ 绑定。

Integration Test：

```text
UART → DMA → Platform → Service → RingBuffer → Task
```

用于验证完整数据链路。

------

# 27. Agent 修改规则

AI Agent 在修改工程时必须先读取：

```text
docs/agent/architecture.md
docs/agent/requirements.md
docs/agent/implementation_plan.md
docs/agent/handoff.md
```

执行时必须遵守：

- 不擅自改变架构方向。
- 不擅自修改冻结公共接口。
- 不为了消除编译错误制造跨层依赖。
- 不进行与当前任务无关的重构。
- 不修改 Vendor 源码，除非计划明确要求。
- 不在 CubeMX 文件中增加大量业务逻辑。
- 优先最小范围修改。
- 新增依赖前检查其方向是否合法。
- 发现架构设计问题时，不自行扩大任务范围。

如果实施过程中发现当前方案无法正确实现，应：

```text
STOP ARCHITECTURAL CHANGE
        ↓
记录 handoff.md
        ↓
标记 BLOCKED
        ↓
说明原因
        ↓
提出建议
        ↓
等待重新设计
```

------

# 28. 冻结边界

当前阶段默认冻结：

- 五层依赖方向。
- Platform Object 基础模型。
- Platform Device 基础模型。
- Platform Lifecycle 基本接口。
- Platform UART 公共语义。
- Platform UART 不暴露 HAL/DMA/RTOS Handle。
- DMA Buffer 与 Ring Buffer 职责分离。
- ISR 与 Task 职责分离。

如需修改以上内容，应先进行架构评审，而不是在实现过程中顺手修改。

------

# 29. 当前已知技术债

以下问题已经识别，但不要求在无关任务中顺便重构。

## 29.1 Log 依赖泄漏

当前存在：

```text
Platform Log
    ↓
Impl / EasyLogger Header
```

长期目标应调整为：

```text
Platform Log API
    ↓
Impl Adapter
    ↓
EasyLogger / RTT
```

------

## 29.2 Platform Types 依赖 Impl

当前存在：

```text
platform_types.h
    ↓
board_types.h
```

需要后续明确：

- `board_types.h` 是否真正属于 Impl。
- 或基础类型是否应迁移到更低层的公共区域。

在正式设计前不扩大该依赖。

------

## 29.3 CubeMX 文件中仍存在业务代码

例如：

- UART Mutex。
- `fputc()`。
- Log 初始化。
- 默认 Task 日志逻辑。

这些属于架构迁移前遗留代码。

应随着对应模块正式接入逐步迁移，而不是一次性大规模重构。

------

# 30. 当前架构实施优先级

当前项目下一阶段优先完成第一条完整垂直链路：

```text
APP
 ↓
Service
 ↓
Platform UART
 ↓
STM32 UART Impl
 ↓
DMA / IRQ
 ↓
HAL
```

然后形成完整 RX 数据路径：

```text
UART
 ↓
DMA
 ↓
Impl
 ↓
Platform Event
 ↓
Service
 ↓
Ring Buffer
 ↓
Task
```

在这条链路跑通前，不优先继续扩展大量新的 Platform 基础框架。

原则：

> 优先验证架构能承载真实功能，再继续抽象更多设备。