# 工程长期记忆与交接说明

更新时间：2026-09-01

> 本文件是 AI Agent 与人工开发者恢复工程上下文时的长期入口。
> 只保存长期目标、稳定架构合同、已验证能力、当前边界、技术债和下一步。
> 已完成阶段的详细设计、执行步骤、临时板测代码和历史验证过程，以专项设计文档、Tests 与 Git history 为准。

---

# 1. 项目定位与长期目标

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
Toolchain  : Keil MDK-ARM + STM32CubeMX
Debug Log  : EasyLogger + SEGGER RTT
```

项目最初用于学习：

```text
UART 不定长接收 + DMA + RingBuffer + FreeRTOS
```

当前同时承担两个目标：

1. 完成结构清晰、行为明确、可验证的嵌入式通信与基础 Platform 能力；
2. 验证 Requirements -> Design -> Plan -> AI Implementation -> Test -> Handoff 的 AI 辅助嵌入式开发流程。

固定总体链路：

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

第一阶段优先级：

```text
Correctness
    > clear boundaries
    > verifiability
    > maintainability
    > performance optimization
    > abstraction elegance
```

完整链路跑通后允许有明确收益的小范围技术债重构；禁止仅为理论上的“更优抽象”反复重写已验证模块。

---

# 2. 当前工程状态

仓库基线：

```text
main
```

当前已完成：

```text
Platform BSP UART Binding Phase 1   COMPLETED
Communication UART Binding          VERIFIED
UART Service Phase 1                COMPLETED
Base RX Vertical Slice              VERIFIED
APP Phase 1                         COMPLETED / VERIFIED
Service Log Phase 1                 COMPLETED / HOST + MANUAL RTT VERIFIED
Platform Log Naming Refactor        COMPLETED / KEIL + RTT VERIFIED
RingBuffer SPSC Review              REVIEWED / NO STRUCTURAL REFACTOR
```

当前准备阶段：

```text
GPIO Platform Phase 1 Design        FROZEN
GPIO Platform Phase 1 Plan          COMPLETED
GPIO Platform Implementation        COMPLETED / HOST VERIFIED
GPIO Platform Host Tests             PASS
Header Isolation                     PASS
Platform Dependency Boundary         PASS
Coding Standard Review               PASS
No STM32 HAL Dependency              PASS
GPIO STM32 Impl                     NOT STARTED
Target Board GPIO                   NOT YET VERIFIED
```

当前专项设计：

```text
00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md
```

当前施工方案：

```text
00_Doc/04_Agent/implementation_plan.md
```

下一位 Agent 不得把 GPIO STM32 Impl、CubeMX 具体 Pin 配置、EXTI、LED 或 KEY 实现并入当前 GPIO Platform Phase 1。

---

# 3. 已验证 RX 垂直链路

当前真实 RX 链路：

```text
USART1
  ↓
DMA Circular + IDLE / HT / TC
  ↓
STM32 UART Impl
  ↓
Platform UART RX_DATA / ERROR / CANCELED
  ↓
UART Service
  ↓
SPSC RingBuffer
  ↓
Platform Notify From ISR
  ↓
APP Communication Task
  ↓
Application-level byte-stream handling
```

APP Phase 1 已正式接管系统级对象装配、Communication Task、UART Service 生命周期和静态 Buffer 所有权。

现有 UART / APP / Service 垂直链路已经通过 Host、Keil 和目标板数据完整性验证，后续 GPIO 阶段不得为了实现方便破坏这些冻结合同。

---

# 4. 稳定架构合同

固定依赖方向：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

## 4.1 APP

负责：

- 产品级流程；
- Task lifecycle 与系统启动 / 关闭编排；
- Service 的使用与错误决策；
- 协议解析和业务状态机；
- 必要的静态资源 / backing storage 所有权。

不得直接依赖 STM32 HAL、DMA Handle、USART Instance、CMSIS-RTOS2 / FreeRTOS concrete handle、Vendor private API、Impl private API。

## 4.2 Service

负责：

- 数据流组织；
- 软件状态和统计；
- RingBuffer 等通用数据结构组合；
- 将 Platform 事件转换为 Task 可消费的软件语义。

不得直接访问 HAL / DMA / USART 寄存器。

## 4.3 Platform

Platform 定义“上层需要什么能力”，而不是直接镜像 STM32 HAL API。

公共 Platform Header 不暴露 HAL / RTOS Handle。

当前已验证的重要 Platform 能力：

```text
Platform Common
Platform UART
Platform OS
Platform Log
```

当前正在扩展：

```text
Platform GPIO Phase 1
```

## 4.4 Impl

负责 Platform 在 STM32F411 + FreeRTOS + EasyLogger / RTT 环境上的具体实现。

可以依赖 STM32 HAL、CMSIS、FreeRTOS、CubeMX Handle、DMA / IRQ、EasyLogger、SEGGER RTT。

## 4.5 CubeMX 生成文件

`main.c`、`freertos.c`、`usart.c`、`gpio.c`、`stm32f4xx_it.c` 等只作为初始化、Scheduler、IRQ / HAL Callback 和薄适配入口。

长期业务逻辑不得堆积到 CubeMX 自动生成文件；必须修改时优先限制在 `USER CODE` 区域。

---

# 5. 数据、并发与内存合同

## 5.1 DMA Buffer 与 RingBuffer

```text
DMA RX Buffer = hardware-facing temporary storage
RingBuffer     = software-facing unread byte-stream storage
```

RingBuffer 不知道 UART、DMA、HAL、RTOS 或协议。

## 5.2 RX 所有权

```text
platform_uart_t storage       -> APP / Caller owns
service_uart_t storage        -> APP / Caller owns
DMA RX backing storage        -> APP / Caller owns
RingBuffer backing storage    -> APP / Caller owns
Consumer Task lifecycle       -> APP owns

Active RX Session             -> UART Service owns
DMA RX Buffer writer          -> DMA / STM32 Impl only
RX_DATA event data            -> callback-period read-only view
RingBuffer Producer           -> UART Service RX callback
RingBuffer Consumer           -> dedicated APP Communication Task
```

## 5.3 RingBuffer SPSC

冻结目标：STM32F411 单核 Cortex-M4，Single Producer = UART Service RX callback，Single Consumer = APP Communication Task。

发布顺序：

```text
Producer: copy bytes -> publish writeIndex
Consumer: copy bytes -> publish readIndex
```

不得为了消除审查提示直接给当前 ISR Producer 路径加入普通 Mutex。

## 5.4 ISR / Task

ISR / HAL Callback 只允许：capture、copy necessary bytes、update lightweight state、notify、quick exit。

禁止 blocking、普通 Mutex、malloc/free、协议解析、大量格式化日志和非 ISR-safe RTOS API。

## 5.5 内存

核心通信链路和当前 GPIO Platform 均优先使用 caller-owned / static storage，不引入动态内存。

---

# 6. 已验证能力基线

## 6.1 Platform UART / UART Service / APP

已完成并验证：

```text
UART construct / lifecycle
blocking TX / RX
DMA Circular + IDLE / HT / TC RX
Platform callback
UART Service + RingBuffer
APP Communication Task
1280-byte raw binary integrity path
```

`platform_uart_set_callback()` 已属于正式基线；未经专项设计不得修改 callback ownership 或现有公共 UART 语义。

## 6.2 Platform OS

Thread、Mutex、Semaphore、Queue、Thread Notification、Software Timer、Time / Delay 已完成 Host / Keil / Board 验证。

## 6.3 RingBuffer

冻结合同：

```text
SPSC byte stream
caller-owned storage
usable capacity = storageSize - 1
Partial Write on overflow
no malloc/free
no RTOS
no UART/DMA/HAL
no Mutex / Semaphore / Critical Section
```

## 6.4 Service Log

正式链路：

```text
APP / Service
    ↓
service_log
    ↓
Platform Log
    ↓
EasyLogger Adapter
    ↓
EasyLogger / RTT
```

Service Log Phase 1 已完成 Host、Keil 和人工 RTT 验收。

---

# 7. GPIO Platform Phase 1 冻结合同

专项设计：

```text
00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md
```

## 7.1 范围

本阶段只实现：

```text
platform_gpio_init
platform_gpio_configure
platform_gpio_write
platform_gpio_read
platform_gpio_deinit
```

目标目录：

```text
03_Platform/platform_mcu/gpio/
├── platform_gpio_types.h
├── platform_gpio.h
└── platform_gpio.c
```

Host Test：

```text
Tests/platform_gpio/
```

## 7.2 对象模型

`platform_gpio_t` 是轻量 MCU Resource，不继承 `platform_device_t`，不使用 `platform_lifecycle_t`。

只维护：

```text
initialized = Platform object constructed / Ops bound
configured  = hardware configuration successfully applied by Impl
```

`platform_gpio_init()` 只构造抽象对象，不配置具体硬件。

`platform_gpio_deinit()` 只反配置硬件，不销毁 Platform 对象；成功后 `initialized=true`、`configured=false`，允许再次 configure。

## 7.3 公共配置

只抽象：

```text
LEVEL       LOW / HIGH
DIRECTION   INPUT / OUTPUT
PULL        NONE / UP / DOWN
OUTPUT TYPE PUSH_PULL / OPEN_DRAIN
INITIAL LEVEL
```

Phase 1 不公开 GPIO Speed。

## 7.4 GPIO 与 Board / BSP 边界

Platform GPIO 不知道 LED、KEY、LCD RESET、SPI CS 等设备语义。

低电平有效 / 高电平有效等极性转换属于 Board / BSP。

## 7.5 输出初始值

后续 Impl 必须尽可能保证：

```text
prepare initial output level
    -> configure pin as output
```

以减少 CS / RESET / ENABLE 等信号切换为输出时的毛刺。

## 7.6 当前禁止范围

```text
04_Impl GPIO implementation
HAL_GPIO_xxx
GPIO_TypeDef / GPIO_PIN_x
CubeMX concrete pin configuration
EXTI / NVIC / IRQ callback
Button / LED API
Debounce / long press / short press
GPIO toggle
Alternate Function
Analog Mode
GPIO speed
GPIO group / port batch operations
RTOS synchronization
Dynamic allocation
GPIO registry
```

如果实施必须突破上述范围才能通过 Platform Host Test，执行者必须 STOP / BLOCKED 并返回设计阶段。

---

# 8. GPIO Platform Phase 1 当前验证目标

当前实现已完成并通过 Host 验证。

本阶段完成 Gate：

```text
platform_gpio_types.h          IMPLEMENTED
platform_gpio.h                IMPLEMENTED
platform_gpio.c                IMPLEMENTED
Platform GPIO Host Tests       PASS
Header Isolation               PASS
Platform Dependency Boundary   PASS
Coding Standard Review         PASS
No STM32 HAL Dependency        PASS
```

验证边界：

```text
Host Test                       VERIFIED
Keil Build                      NOT YET VERIFIED
Target Board GPIO               NOT YET VERIFIED
```

完成状态只能标记为：

```text
GPIO Platform Phase 1 = FROZEN + HOST VERIFIED
```

不得标记为 Target Board Verified，因为 STM32 Impl 尚未进入本阶段。

---

# 9. 已知技术债与限制

## 9.1 Platform Types 依赖 Impl 类型

当前存在：

```text
platform_types.h
    ↓
board_types.h
```

需要后续重新确认基础类型归属；正式迁移设计前不在 GPIO 阶段扩大该问题。

## 9.2 CubeMX 遗留胶水代码

仍存在部分历史 `fputc()`、UART printf、默认 Task / 初始化胶水；只随真实需求逐步清理。

## 9.3 USART1 Callback 单实例路由

当前 STM32 UART Impl 使用 USART1 单实例 Context。

状态：

```text
KNOWN CURRENT LIMITATION
DEFERRED
NOT A CURRENT DEFECT
```

出现第二个真实 UART 角色后再设计静态 registry / dispatcher。

## 9.4 architecture.md 文档债

`architecture.md` 存在少量旧 Log 依赖描述或历史路径示例。真实 Agent 文档位置固定为：

```text
00_Doc/04_Agent/
```

该文档债不阻塞 GPIO Platform Phase 1。

## 9.5 README 占位

根目录和部分分层 README 仍为空；项目收尾或形成简历 / 示例工程时统一整理。

---

# 10. 当前 Agent 工作规则

开始当前阶段前必须读取：

```text
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/execution_rules.md
00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md
00_Doc/04_Agent/implementation_plan.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
```

实施过程中：

- 按 `implementation_plan.md` Task 顺序施工；
- 优先 Host Test / TDD；
- 不为了消除编译错误制造跨层依赖；
- 不修改与当前 GPIO Platform Phase 1 无关的模块；
- 不修改 Vendor；
- 不进入 `04_Impl`；
- 不修改 CubeMX concrete GPIO pin configuration；
- 不擅自增加冻结公共接口；
- 新增依赖前检查方向；
- 发现设计与仓库现实存在实质冲突时 STOP / BLOCKED。

验证结论必须区分：

```text
Host Verified
Keil Build Verified
Target Board Verified
Not Yet Verified
```

静态检查不得替代真实 Keil / Board 证据。

---

# 11. 下一阶段入口

当前 GPIO Platform 阶段状态：

```text
GPIO Platform Phase 1           COMPLETED / HOST VERIFIED
```

下一阶段入口：

```text
GPIO STM32 Impl Phase 1 Design
```

下一阶段必须重新建立：

```text
GPIO STM32 Impl Phase 1 Design
        ↓
新的专项设计 + 新的 implementation_plan.md
        ↓
STM32 Impl + simple board GPIO verification
```

后续可使用 LED 作为最简单的板级验证对象，但 LED 逻辑极性必须属于 Board / BSP，而不是 GPIO Platform。

EXTI 应在出现真实按键、IMU INT、Touch IRQ 等需求后再单独扩展，不在当前阶段提前实现。

---

# 12. 历史追溯位置

```text
00_Doc/02_架构设计/              -> 各阶段冻结专项设计
00_Doc/04_Agent/requirements.md -> 项目需求基线
00_Doc/04_Agent/architecture.md -> 完整架构合同
00_Doc/04_Agent/handoff.md      -> 当前长期交接
00_Doc/04_Agent/implementation_plan.md -> 当前阶段施工方案
Git history                     -> 实施过程和阶段提交
Tests/                          -> Host 验证代码
```

原则：

> `handoff.md` 负责快速恢复“工程现在是什么、哪些不能动、下一步是什么”；
> 专项设计负责“为什么这样设计”；
> `implementation_plan.md` 负责“当前阶段怎么执行”；
> Git history 负责“过去具体发生了什么”。
