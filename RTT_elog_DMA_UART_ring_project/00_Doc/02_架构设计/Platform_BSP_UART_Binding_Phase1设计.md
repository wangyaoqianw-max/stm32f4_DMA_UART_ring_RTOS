# Platform BSP UART Binding Phase 1 设计

> 文档类型：专项设计 / Frozen Design Contract  
> 状态：FROZEN  
> 版本：V1.0  
> 日期：2026-08-30

---

## 1. 背景

UART Service Phase 1 已完成并通过 Host、Keil 与真实板级验证。当前下一目标是将已经验证的 UART Service 垂直链路正式接入 `01_APP/`。

APP Phase 1 设计前存在一个明确的组合缺口：当前 STM32 USART1 的 Platform UART 构造入口位于 Impl 层：

```c
impl_platform_uart_usart1_construct(...);
```

如果 APP 直接调用该函数，将形成：

```text
APP -> Impl
```

这违反当前确认的架构边界。

本项目进一步明确依赖规则为：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

因此，在 APP Phase 1 之前增加一个很小的 Platform BSP UART Binding 阶段，用于把产品级逻辑设备 `Communication UART` 绑定到当前目标板的具体 USART1 实现。

---

## 2. Phase 1 目标

本阶段只解决一个问题：

> APP 如何在完全不知道 STM32 USART1、HAL Handle、DMA 资源和 Impl 构造函数的情况下，构造产品所需的 Communication UART Platform 对象。

目标调用关系：

```text
APP
  |
  | platform_bsp_uart_construct_communication()
  v
Platform BSP UART Contract
  |
  | current-board binding
  v
Impl BSP
  |
  | impl_platform_uart_usart1_construct()
  v
STM32 UART Impl
  |
  v
USART1 / HAL / DMA / Hardware
```

本阶段完成后，APP 只需要知道：

```text
Communication UART
Platform UART configuration
Platform UART lifecycle
```

APP 不需要知道：

```text
USART1
huart1
DMA2_Stream2
DMA_CHANNEL_4
HAL_UART_*
impl_platform_uart_usart1_construct()
g_usart1Context
```

---

## 3. 架构定位

### 3.1 APP 可直接依赖 Platform

本项目的五层关系不是“APP 只能调用 Service”的严格线性调用链。

正式依赖关系允许：

```text
                APP
              /     \
             v       v
         Service   Platform
             \       /
              v     v
             Platform
                |
                v
               Impl
                |
                v
             Vendor
```

APP 可以直接使用 Platform 提供的系统级抽象，例如：

- Platform OS；
- Platform Log；
- Platform Device lifecycle；
- Platform BSP 逻辑设备绑定。

但 APP 不得直接包含或调用 Impl / HAL / Vendor 私有接口。

### 3.2 Platform BSP 的职责

Platform BSP 表达：

> 当前产品有哪些逻辑硬件能力可以供上层使用。

Impl BSP 表达：

> 这些逻辑能力在当前具体板卡上绑定到什么硬件实现。

因此本阶段：

```text
Communication UART
        |
        v
STM32 USART1
```

这个映射属于 BSP / Impl 边界，而不是 APP 业务知识。

---

## 4. 文件布局

本阶段新增：

```text
03_Platform/
└── platform_bsp/
    └── platform_bsp_uart.h

04_Impl/
└── impl_bsp/
    └── impl_platform_bsp_uart.c

Tests/
└── platform_bsp_uart/
    └── test_platform_bsp_uart.c
```

不新增：

```text
platform_bsp_uart.c
Device Registry
Device Factory
IoC Container
动态设备表
```

`platform_bsp_uart.h` 定义上层可见的 Platform Contract；其当前板级实现直接位于 `impl_bsp`。

---

## 5. 公共接口

Phase 1 只暴露一个产品级 UART 构造入口：

```c
platform_error_t platform_bsp_uart_construct_communication(
    platform_uart_t *uart,
    const platform_uart_config_t *config);
```

建议公共头文件：

```text
03_Platform/platform_bsp/platform_bsp_uart.h
```

公共 Header 只允许依赖 Platform 公共类型，例如：

```c
#include "platform_uart.h"
```

禁止包含：

```text
impl_platform_uart.h
usart.h
stm32f4xx_hal.h
cmsis_os2.h
FreeRTOS.h
```

---

## 6. API 语义

### 6.1 输入

`uart`：

- 由 APP / Caller 提供实际 `platform_uart_t` 存储；
- 首次构造前必须满足现有 Platform UART 的初始化合同；
- BSP 不拥有该对象存储。

`config`：

- 由 APP / Caller 提供产品级 UART 行为配置；
- 包括 baud rate、data bits、stop bits、parity、flow control、default timeout；
- BSP 不把配置固定死在 USART1 绑定实现中；
- 最终由现有 Platform UART 构造逻辑复制进入 `platform_uart_t.config`。

### 6.2 成功结果

成功后只完成：

```text
Communication UART logical role
        +
current-board USART1 implementation binding
        +
Platform UART object construction
```

对象应处于现有 Platform UART 构造后的 `CREATED` 语义。

### 6.3 不执行生命周期

本函数不得调用：

```text
UART lifecycle init
UART lifecycle start
UART lifecycle stop
UART lifecycle deinit
HAL_UART_Init
DMA RX start
service_uart_init
service_uart_start
```

UART lifecycle 继续由后续 APP Composition Root 按既有 Platform 合同编排。

### 6.4 Callback

BSP 构造时固定：

```text
callback        = NULL
callbackContext = NULL
```

原因：UART Service Phase 1 已冻结由 `service_uart_init()` 通过 `platform_uart_set_callback()` 接管异步 RX callback 的设计。

BSP 不参与 Service callback ownership。

### 6.5 Error

BSP 必须至少检查：

```text
uart == NULL
config == NULL
```

非法参数返回：

```text
PLATFORM_ERR_INVALID_PARAM
```

底层 `impl_platform_uart_usart1_construct()` 返回的其他标准 `platform_error_t` 原样传播，不重新映射，不吞掉根因。

重复构造、非法 Platform 配置等语义继续由现有 Platform UART / Impl 构造合同决定，本层不复制第二套状态机。

---

## 7. 当前板级绑定

当前唯一绑定固定为：

```text
Logical role       : Communication UART
Platform object    : caller-owned platform_uart_t
Concrete Impl      : STM32 Platform UART Impl
Physical resource  : USART1
UART config owner  : APP / Caller
```

Impl BSP 内部通过：

```c
impl_platform_uart_usart1_construct(
    uart,
    "communication_uart",
    PLATFORM_DEVICE_CAP_NONE,
    config,
    NULL,
    NULL);
```

完成当前板级绑定。

这里的字符串名称和 `PLATFORM_DEVICE_CAP_NONE` 属于当前产品/板级装配策略，不要求 APP 传入。

---

## 8. 所有权合同

本阶段不改变 UART Service Phase 1 已冻结的所有权。

```text
platform_uart_t storage       -> APP / Caller owns
platform_uart_config_t        -> APP / Caller owns source config
service_uart_t storage        -> APP / Caller owns
DMA RX backing storage        -> APP / Caller owns
RingBuffer backing storage    -> APP / Caller owns
Communication Task lifecycle  -> APP owns

STM32 UART Impl context       -> Impl owns
UART HAL / DMA handle         -> CubeMX / Impl owns
Active RX Session             -> UART Service owns
Platform UART callback        -> UART Service binds after service init
```

Platform BSP：

```text
不 malloc
不 free
不创建 platform_uart_t singleton
不接管 caller storage
不创建 Task
不保存 APP callback
```

---

## 9. 配置与硬件映射分离

本阶段明确区分：

```text
Product behavior configuration
        !=
Board physical resource mapping
```

APP 决定：

```text
baudRate
word/data bits
stop bits
parity
flow control
default timeout
```

Impl BSP / Impl 决定：

```text
Communication UART -> USART1
USART1 -> huart1
USART1 RX -> DMA2 Stream2 / Channel 4
IRQ / HAL callback / DMA implementation
```

因此未来：

```text
115200 -> 921600
```

属于 APP 配置变化；

而：

```text
USART1 -> USART2
```

属于 BSP / Impl 映射变化，原则上不应修改 APP。

---

## 10. 单实例组合合同

Phase 1 只有一个产品级 Communication UART。

固定组合模型：

```text
1 Communication UART role
        ->
1 caller-owned platform_uart_t
        ->
1 USART1 Impl context
```

本阶段规定：

> `platform_bsp_uart_construct_communication()` 在系统 Composition 阶段只调用一次。

当前 `impl_platform_uart.c` 使用单个静态 `g_usart1Context`。本阶段不为了防御错误的二次组合而新增：

```text
Device Registry
Resource Manager
reference count
全局设备锁
动态设备占用表
```

如果未来真实出现多 UART / 动态设备装配需求，再单独重新设计资源管理模型。

---

## 11. 与 APP Phase 1 的接口

本阶段完成后，APP Composition Root 可以按如下方向设计：

```text
app_system
   |
   +-> platform_bsp_uart_construct_communication()
   |
   +-> Platform OS create Communication Task
   |
   +-> service_uart_init()
   |
   +-> Platform UART lifecycle init / start
   |
   `-> service_uart_start()
```

APP source code 中不得出现：

```text
impl_platform_uart_usart1_construct()
impl_platform_uart.h
usart.h
UART_HandleTypeDef
DMA_HandleTypeDef
HAL UART API
```

本阶段只提供 APP 所需的绑定入口，不提前实现 `app_system` 或 `app_communication`。

---

## 12. ISR / 并发影响

本阶段没有新的 ISR / Task 并发模型。

`platform_bsp_uart_construct_communication()` 只允许在系统初始化 / Composition 阶段调用，不允许：

```text
ISR 中调用
RX Session 运行期间重新调用
多个 Task 并发构造同一逻辑 UART
```

本阶段不改变：

- UART DMA RX；
- Platform UART callback；
- UART Service SPSC RingBuffer；
- Platform Notify；
- Consumer Task 并发合同。

---

## 13. Scope Guard

Platform BSP UART Binding Phase 1 明确不实现：

```text
APP Phase 1 production code
Communication Task
UART Service 修改
RingBuffer 修改
Platform UART API 修改
UART lifecycle redesign
Protocol Parser
Frame Queue
Async TX Service
多 UART role enum
Device Registry
Device Factory
IoC Container
动态资源分配
自动 Error Recovery
CubeMX UART 配置重构
USART1 mutex / fputc 历史代码清理
```

如果实施证明必须修改现有 Platform UART 公共接口或 UART Service 冻结合同：

```text
STOP / BLOCKED
```

返回设计评审，不允许为了完成 BSP 绑定顺手改变下层已验证语义。

---

## 14. Host Test 设计

Host Test 不依赖真实 HAL。

测试 `impl_platform_bsp_uart.c` 时，为：

```c
impl_platform_uart_usart1_construct(...)
```

提供 fake implementation，记录转发参数并控制返回值。

至少验证：

```text
NULL uart
    -> PLATFORM_ERR_INVALID_PARAM
    -> fake constructor not called

NULL config
    -> PLATFORM_ERR_INVALID_PARAM
    -> fake constructor not called

valid uart + config
    -> fake constructor called exactly once
    -> same uart pointer forwarded
    -> same config pointer forwarded
    -> name == "communication_uart"
    -> caps == PLATFORM_DEVICE_CAP_NONE
    -> callback == NULL
    -> callbackContext == NULL
    -> fake OK propagated

fake constructor returns error
    -> exact error propagated
```

Header Isolation Test 必须证明：

```text
platform_bsp_uart.h
```

可以在不引入 HAL、CMSIS-RTOS2、FreeRTOS、`usart.h` 或 Impl Header 的 Host translation unit 中独立编译。

---

## 15. Keil 集成

生产源文件：

```text
04_Impl/impl_bsp/impl_platform_bsp_uart.c
```

必须加入当前 Keil 工程：

```text
MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
```

Platform BSP 公共 Include Path 必须能解析：

```text
03_Platform/platform_bsp/
```

Impl BSP 必须能解析当前已有：

```text
04_Impl/impl_mcu/
03_Platform/platform_mcu/uart/
03_Platform/platform_common/
```

不要求本阶段运行真实 UART 数据流板测，因为本阶段没有改变 UART runtime 行为。

---

## 16. 验收门禁

本阶段完成必须至少证明：

```text
Design Contract                  FROZEN
Host BSP UART Test               PASS
Header Isolation                 PASS
Relevant Regression              PASS
Coding Standard Review           PASS
Keil Full Rebuild                0 Error(s)
Target Board Runtime Test        NOT REQUIRED FOR THIS BINDING-ONLY PHASE
```

另外必须静态确认：

```text
01_APP/ 不新增 Impl include
Platform BSP public header 不暴露 Impl/HAL/Vendor concrete type
现有 Platform UART / UART Service 公共接口未改变
```

---

## 17. 完成后的下一阶段

本阶段完成后进入：

```text
APP Phase 1 Design / Implementation
```

APP Phase 1 的重点仍是：

- APP Composition Root；
- Communication Task lifecycle；
- `wait_event()` + drain RingBuffer；
- RX_AVAILABLE / DATA_LOSS / ERROR / STOPPED 的产品级策略；
- CubeMX `freertos.c` 薄适配；
- 不在第一步引入 Protocol Parser / Async TX。

Platform BSP UART Binding Phase 1 完成后，不应再因为 USART1 具体构造问题让 APP 直接越层依赖 Impl。
