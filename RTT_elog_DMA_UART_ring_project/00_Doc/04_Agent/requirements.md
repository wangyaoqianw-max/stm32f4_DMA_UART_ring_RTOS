# UART DMA RingBuffer RTOS Project Requirements

> 文档类型：Project Requirements
> 状态：Baseline
> 版本：V1.0
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

------

# 1. 项目背景

本项目最初用于学习 STM32 UART 不定长数据接收、DMA 和 Ring Buffer 等嵌入式通信技术。

在基础实验目标之上，引入分层架构、FreeRTOS、统一 Platform 接口、错误处理、日志、测试和文档管理，使项目从单一功能实验逐步演化为一个小型工程化嵌入式软件项目。

项目重点不是单纯实现“串口能收发数据”，而是通过 UART 通信链路练习：

- UART 不定长数据接收
- DMA 数据搬运
- Ring Buffer 数据缓存
- 中断与任务协作
- FreeRTOS 并发设计
- 模块职责划分
- Platform 硬件抽象
- 错误处理与恢复
- 可测试的软件接口
- 工程化开发流程

------

# 2. 项目目标

项目最终应形成一条完整且可验证的数据链路：

```text
UART Hardware
      ↓
HAL / DMA / IRQ
      ↓
Impl
      ↓
Platform UART
      ↓
UART Service
      ↓
Ring Buffer
      ↓
Application Task
```

系统应能够持续接收外部 UART 输入的不定长数据，并在 DMA、Ring Buffer 和 FreeRTOS 配合下将数据安全地传递到任务上下文处理。

同时建立清晰的：

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

软件依赖关系。

------

# 3. 非目标

本项目当前不要求：

- 实现完整的工业通信协议。
- 实现 Modbus、CANopen 等具体协议栈。
- 构建通用 RTOS 或通用 HAL 框架。
- 支持运行时动态加载设备驱动。
- 支持任意 MCU 无修改直接运行。
- 实现复杂的动态内存管理。
- 追求极端 Zero-Copy。
- 实现完整 Bootloader / OTA 功能。
- 建立大型设备注册中心或复杂 IoC 框架。
- 为简单功能增加与当前需求无关的抽象层。
- 达到功能安全认证或商业量产认证标准。

本项目首先保证：

> 架构清晰、行为明确、并发安全、可测试、可理解。

------

# 4. 硬件运行环境

当前目标硬件平台：

```text
MCU        : STM32F411CEU6
Architecture: ARM Cortex-M4F
Flash      : 512 KB
RAM        : 128 KB
UART       : USART1
```

当前 UART 基准配置：

```text
Baud Rate  : 115200
Data Bits  : 8
Parity     : None
Stop Bits  : 1
Flow Ctrl  : None
```

后续允许调整波特率进行压力测试，但系统至少必须稳定支持 115200 baud。

------

# 5. 软件运行环境

当前软件环境：

```text
STM32 HAL
FreeRTOS
CMSIS-RTOS V2
Keil MDK-ARM
STM32CubeMX
SEGGER RTT
EasyLogger
```

CubeMX 负责 MCU 外设基础配置和初始化代码生成。

业务代码不得长期堆积在 CubeMX 自动生成文件中。

------

# 6. UART 基础功能需求

## 6.1 UART 初始化

系统必须能够完成目标 UART 的初始化，并建立 Platform UART 对象与实际 STM32 UART 实例之间的绑定关系。

初始化失败时：

- 必须返回明确错误。
- 不得继续进入正常运行状态。
- 不得留下半初始化的软件状态。

------

## 6.2 UART 接收

UART 必须支持：

- 不定长数据输入。
- 连续数据输入。
- 多次分段输入。
- 长度未知的数据流。
- 小于 DMA Buffer 的数据。
- 超过一次 DMA Buffer 容量的数据流。

上层不得依赖：

> 一次 UART 接收事件等于一个完整业务数据包。

UART 层只负责提供字节流。

------

## 6.3 UART 发送

系统至少需要支持可靠的 UART 数据发送。

发送接口必须能够明确区分：

- 成功
- Busy
- Timeout
- I/O Error
- Invalid Parameter

后续如使用异步发送，应能够获得：

- TX Complete
- Error
- Canceled

等完成事件。

------

# 7. DMA 接收需求

UART 接收的数据搬运必须使用 DMA 完成。

DMA 的主要职责是：

```text
UART Peripheral
        ↓
DMA
        ↓
DMA Receive Buffer
```

CPU 不应针对每一个收到的字节执行一次普通接收处理。

系统必须能够确定：

> DMA 自上次处理以来新增了多少有效数据。

DMA Buffer 到达边界时不得导致：

- 数据顺序错误
- 重复数据
- 越界访问
- 未定义内存访问

具体 DMA 工作模式由技术设计阶段决定。

------

# 8. 不定长数据检测需求

系统必须具备识别“当前已经收到一批可处理数据”的机制。

该机制必须支持：

- 短数据
- 非固定长度数据
- 连续数据
- DMA Buffer 未填满时的数据处理

数据到达事件仅表示：

> 当前存在新的 UART 字节数据。

不得在 Impl 或 Platform 层判断业务协议帧是否完整。

------

# 9. Ring Buffer 需求

系统必须使用 Ring Buffer 作为 UART 接收数据的长期缓存。

数据路径：

```text
DMA Buffer
    ↓
Ring Buffer
    ↓
Service / Task
```

DMA Buffer 与 Ring Buffer 必须具有不同职责：

```text
DMA Buffer
临时保存硬件刚接收到的数据。

Ring Buffer
保存尚未被上层消费的数据流。
```

Ring Buffer 必须至少支持：

- 初始化
- 写入
- 读取
- 查询可读数据长度
- 查询剩余容量
- Wrap Around
- Buffer Full 检测

Ring Buffer 不负责：

- UART 硬件控制
- DMA 控制
- 协议解析
- 日志输出
- FreeRTOS Task 创建

------

# 10. Ring Buffer 满处理

Ring Buffer 不允许发生静默覆盖。

当剩余空间不足以保存新数据时，系统必须能够检测到 Overflow。

Overflow 策略必须在设计阶段明确，并至少满足：

- 不发生非法内存写入。
- 不将已损坏的数据伪装成正常数据。
- 能够记录 Overflow 状态或统计信息。
- 上层能够得知发生过数据丢失。

默认优先策略：

> 保留已经进入 Ring Buffer 的旧数据，拒绝无法容纳的新数据，并报告 Overflow。

如后续业务要求采用其他策略，应重新评审。

------

# 11. Service 层需求

UART Service 负责连接 Platform UART 与上层应用。

主要职责包括：

- 管理 UART RX 数据流。
- 管理 Ring Buffer。
- 接收 Platform UART 的数据事件。
- 将新数据写入 Ring Buffer。
- 向任务上下文通知存在新数据。
- 为 APP 提供读取 UART 数据的接口。
- 管理通信相关运行状态和统计信息。

UART Service 不应：

- 直接访问 `UART_HandleTypeDef`。
- 直接调用 STM32 HAL UART API。
- 保存具体 DMA Stream / Channel。
- 直接操作 UART 寄存器。

------

# 12. APP 层需求

APP 层负责产品级数据处理流程。

APP 可以：

- 等待 UART 数据事件。
- 从 UART Service 获取数据。
- 执行业务数据解析。
- 执行协议状态机。
- 根据数据触发业务行为。

APP 不得：

- 直接访问 HAL UART。
- 直接控制 DMA。
- 直接操作 DMA Receive Buffer。
- 直接依赖 STM32 USART 实例。

------

# 13. FreeRTOS 任务需求

系统必须将：

```text
硬件实时处理
```

与：

```text
业务数据处理
```

分离。

推荐执行关系：

```text
UART / DMA ISR
      ↓
最小数据处理
      ↓
通知任务
      ↓
UART / Communication Task
      ↓
读取 Ring Buffer
      ↓
业务处理
```

任务上下文负责相对耗时的工作，包括：

- 数据消费
- 协议解析
- 日志
- 状态机
- 复杂业务逻辑

------

# 14. ISR 并发约束

UART/DMA 中断上下文必须遵守：

允许：

- 获取硬件状态。
- 获取 DMA 当前接收位置。
- 更新必要的轻量状态。
- 搬运必要数据。
- 调用明确声明为 ISR-safe 的接口。
- 使用 ISR-safe RTOS 通知机制。

禁止：

- 阻塞等待。
- 使用普通 Mutex。
- 长时间循环。
- 动态内存分配。
- 完整协议解析。
- 大量格式化日志。
- 长时间 UART 打印。
- 调用非 ISR-safe RTOS API。

ISR 设计目标：

> 尽快完成必要工作并退出中断。

------

# 15. 数据所有权需求

所有异步 Buffer 必须明确所有者。

至少区分：

```text
DMA Receive Buffer
Ring Buffer Storage
APP / Service Temporary Buffer
UART TX Buffer
```

任何 Buffer 在被 DMA 或异步 UART 使用期间：

- 不得提前释放。
- 不得非法复用。
- 不得被其他上下文无保护修改。

Platform UART 必须定义异步 Buffer 生命周期。

------

# 16. Platform 层需求

Platform UART 必须对上层隐藏：

- `UART_HandleTypeDef`
- `DMA_HandleTypeDef`
- USART1
- DMA Stream / Channel
- HAL UART API
- FreeRTOS Handle

Platform UART 负责：

- UART 公共配置
- 公共状态检查
- 参数检查
- UART 操作接口
- 异步事件模型
- 错误类型转换或传播

Platform 不负责：

- Ring Buffer
- 协议解析
- RTOS Task
- HAL UART 具体实现

------

# 17. Impl 层需求

Impl 层负责 Platform UART 在当前 STM32F411 平台上的实际实现。

Impl 可以依赖：

- STM32 HAL
- CMSIS
- FreeRTOS
- CubeMX 生成的 Handle
- UART IRQ
- DMA
- MCU 硬件资源

Impl 必须将底层实现细节限制在本层。

更换 UART 实例或 MCU 时，应尽量减少对 Service 和 APP 的影响。

------

# 18. Vendor 边界需求

以下组件视为 Vendor / 外部实现：

- STM32 HAL
- CMSIS
- FreeRTOS
- EasyLogger
- SEGGER RTT

原则：

> 不直接修改 Vendor 源码以实现业务需求。

如需要适配，应优先通过：

```text
Platform
或
Impl
```

建立适配层。

------

# 19. 日志系统需求

日志系统用于：

- 初始化状态记录
- Error 记录
- Buffer Overflow 记录
- UART/DMA 异常记录
- 调试状态记录
- 性能及统计信息观察

正常数据收发过程中不得因为日志输出严重阻塞 UART RX 数据链路。

ISR 内原则上不得输出大量日志。

UART 业务数据链路与 RTT / EasyLogger 调试链路必须保持职责独立。

------

# 20. 错误处理需求

系统必须至少考虑：

## 20.1 UART Error

包括：

- Overrun
- Framing Error
- Noise Error
- Parity Error

发生异常后：

- 不得越界访问数据。
- 必须记录错误状态。
- 必须能够恢复接收或进入明确错误状态。

------

## 20.2 DMA Error

DMA 出现异常时：

- 当前传输状态必须明确。
- 不得继续使用不可信的数据长度。
- 必须能够重新建立接收流程或进入错误状态。

------

## 20.3 Ring Buffer Overflow

必须：

- 检测
- 记录
- 统计
- 向上层反映

不得静默忽略。

------

## 20.4 Timeout

阻塞接口必须定义 Timeout 行为。

Timeout 后：

- 必须返回明确错误。
- 输出长度必须具有确定语义。
- 不得产生未定义 Buffer 状态。

------

# 21. 异常恢复需求

对于可恢复异常，应优先：

```text
Detect
  ↓
Record
  ↓
Stop affected operation
  ↓
Clear error state
  ↓
Restart
```

系统不应因为一次普通 UART 通信异常永久失去通信能力。

无法自动恢复的错误必须进入明确错误状态。

------

# 22. 性能需求

第一阶段基准性能：

```text
UART Baud Rate     : 115200 baud
Data Format        : 8N1
```

在正常工作负载下必须保证：

- 连续接收不存在非预期丢字节。
- 数据顺序保持正确。
- 不发生重复写入 Ring Buffer。
- Ring Buffer Wrap Around 正常。
- ISR 不执行长时间阻塞操作。

后续应测试更高波特率，以观察系统性能边界。

------

# 23. 缓冲区需求

所有主要 Buffer 大小必须：

- 集中配置。
- 使用具名宏或配置项。
- 不得在业务代码中散布 Magic Number。
- 有明确的容量设计依据。

至少需要明确：

```text
DMA RX Buffer Size
Ring Buffer Size
Maximum APP Read Size
TX Buffer Size（如需要）
```

具体数值在设计和测试阶段确定。

------

# 24. 数据完整性需求

在系统不存在 Overflow 或硬件错误的正常条件下：

发送端输入：

```text
A B C D E F ...
```

APP 最终读取的数据必须保持：

```text
A B C D E F ...
```

不得出现：

- Missing Data
- Duplicate Data
- Reordered Data
- Memory Corruption

------

# 25. 状态与统计需求

系统应能够记录必要的运行统计信息，例如：

- RX 总字节数
- TX 总字节数
- RX Event 次数
- Buffer Overflow 次数
- UART Error 次数
- DMA Error 次数
- 丢弃字节数

统计功能不得明显影响实时接收性能。

------

# 26. 静态内存需求

核心 UART 数据路径默认使用静态内存。

包括：

- DMA Buffer
- Ring Buffer
- UART Context
- 必要 Service Context

正常数据收发过程中不得依赖频繁动态内存申请和释放。

------

# 27. CubeMX 代码边界

CubeMX 自动生成文件主要负责：

- Clock 初始化
- GPIO 初始化
- UART 初始化
- DMA 基础配置
- NVIC 配置
- FreeRTOS 基础生成代码
- IRQ / HAL Callback 入口

业务逻辑不得长期堆积在：

```text
main.c
freertos.c
usart.c
stm32f4xx_it.c
```

必须修改生成代码时，应优先限制在：

```c
/* USER CODE BEGIN */

/* USER CODE END */
```

区域。

------

# 28. 软件架构需求

项目必须保持依赖方向：

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

禁止形成：

```text
APP → HAL
Service → HAL
Service → STM32 Handle
Platform → STM32 HAL
Vendor → Business
```

等长期跨层依赖。

必要的底层事件可以向上传递，但应通过定义明确的 Callback / Event / Notification 机制完成。

------

# 29. 可测试性需求

核心软件模块应尽量能够脱离真实硬件进行验证。

重点包括：

- Platform UART 参数检查
- Platform UART Ops 转发
- Ring Buffer
- Buffer Wrap Around
- Buffer Overflow
- UART Service 状态处理
- 错误路径
- 数据顺序

Platform 层测试不得强制依赖真实 STM32 HAL。

------

# 30. 基础测试场景

系统至少需要覆盖以下测试。

## 30.1 普通接收

发送：

```text
Hello
```

APP 应完整获得相同数据。

------

## 30.2 不定长接收

连续发送不同长度数据：

```text
1 Byte
10 Bytes
100 Bytes
...
```

均应正确进入 Ring Buffer。

------

## 30.3 分段接收

一段业务数据被 UART 分多次到达时：

系统不得假定每次 RX Event 都是完整业务帧。

------

## 30.4 连续数据

持续发送数据，验证：

- DMA
- Ring Buffer
- Task

能够持续工作。

------

## 30.5 Ring Buffer Wrap Around

人为制造读写指针跨 Buffer 尾部的情况。

必须保持数据顺序正确。

------

## 30.6 Ring Buffer Overflow

停止消费者，使生产速度超过消费速度。

必须：

- 检测 Overflow
- 不越界
- 不崩溃
- 能记录数据丢失

------

## 30.7 UART Error

人为触发或模拟 UART 异常。

系统不得永久卡死。

------

## 30.8 高频连续输入

提高数据输入速率，观察：

- ISR 占用
- Task 调度
- Ring Buffer 使用率
- 数据丢失情况

------

# 31. 工程验收条件

项目达到当前目标至少需要满足：

1. UART 使用 DMA 接收。
2. 支持不定长 UART 字节流。
3. DMA 数据能够正确进入 Ring Buffer。
4. Ring Buffer 支持 Wrap Around。
5. Ring Buffer Overflow 能被检测。
6. FreeRTOS Task 能够正确消费 UART 数据。
7. ISR 中不存在明显阻塞操作。
8. APP 不直接依赖 STM32 HAL UART。
9. Service 不直接依赖 STM32 UART Handle。
10. Platform UART 不暴露 HAL / DMA / RTOS Handle。
11. Impl 封装 STM32 UART、DMA 和 IRQ 细节。
12. 正常连续通信不存在非预期丢字节。
13. 不出现重复数据和乱序数据。
14. UART/DMA 普通异常具有恢复机制。
15. 核心错误具有日志或统计记录。
16. Keil 工程能够正常编译。
17. 核心 Ring Buffer 和 Platform UART 行为具有测试。
18. 架构文档与实际代码基本一致。

------

# 32. 工程完成后的目标数据链路

最终期望形成：

```text
                         APP
                          │
                    Business Logic
                          │
                          ▼
                    UART Service
                   ┌──────┴──────┐
                   │             │
              Ring Buffer      TX Flow
                   │             │
                   └──────┬──────┘
                          ▼
                    Platform UART
                          │
                          ▼
                   STM32 UART Impl
                   ┌──────┴──────┐
                   │             │
                  DMA           IRQ
                   │             │
                   └──────┬──────┘
                          ▼
                         HAL
                          │
                          ▼
                       USART1
```

数据流与依赖关系必须保持清晰，任何模块均应能够明确回答：

1. 本模块负责什么。
2. 本模块不负责什么。
3. 数据从哪里来。
4. 数据到哪里去。
5. 数据由谁拥有。
6. 在什么上下文中执行。
7. 发生错误时由谁处理。

------

# 33. 项目成功定义

本项目的成功标准不是仅仅：

> UART DMA + Ring Buffer 可以运行。

而是完成一个可以解释、测试和维护的嵌入式通信子系统，使开发者能够理解：

```text
Hardware
    ↓
DMA / Interrupt
    ↓
Driver Implementation
    ↓
Hardware Abstraction
    ↓
Software Service
    ↓
RTOS Task
    ↓
Application
```

整个数据路径中的职责、数据所有权、并发关系、异常处理和模块边界。