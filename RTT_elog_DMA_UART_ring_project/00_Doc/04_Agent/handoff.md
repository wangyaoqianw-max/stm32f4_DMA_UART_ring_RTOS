# 工程交接说明

更新时间：2026-08-29

## 1. 工程快照

- 工程根目录：`RTT_elog_DMA_UART_ring_project/`
- 当前分支：`main`
- 当前阶段：Phase 1 — STM32 UART Platform Blocking Impl
- 当前状态：`READY_FOR_IMPLEMENTATION`
- MCU：STM32F411CEU6，UFQFPN48
- 软件环境：STM32 HAL、CMSIS-RTOS2 / FreeRTOS、Keil、EasyLogger、SEGGER RTT
- CubeMX 当前配置：USART1 Asynchronous，PA9/PA10，CMSIS-RTOS2；尚未配置 USART1 DMA
- 总体目标：实现 UART + DMA + Ring Buffer + FreeRTOS，并通过分层架构隔离业务与硬件

依赖方向固定为：

```text
APP -> Service -> Platform -> Impl -> HAL / RTOS / Hardware
```

本次交接对应的近期目标不是完整 DMA UART，而是先打通：

```text
Platform UART
    ↓
STM32 UART Impl
    ↓
HAL UART
    ↓
USART1
```

---

## 2. 当前完成情况

### 已完成

- 已建立 `APP / Service / Platform / Impl / Vendors` 分层目录和架构说明。
- `platform_common` 已提供对象、设备、服务、生命周期、错误码和公共类型基础。
- Platform UART 抽象层已经完成：
  - `03_Platform/platform_mcu/uart/platform_uart_types.h`
  - `03_Platform/platform_mcu/uart/platform_uart.h`
  - `03_Platform/platform_mcu/uart/platform_uart.c`
  - 支持阻塞读写、异步读写、取消和统一事件回调契约。
  - 不暴露 HAL、DMA 或 RTOS 类型，不使用动态内存。
- Platform UART 设计和测试已经存在：
  - `00_Doc/02_架构设计/Platform_UART抽象层设计.md`
  - `Tests/platform_uart/test_platform_uart_types.c`
  - `Tests/platform_uart/test_platform_uart.c`
- Agent 长期约束文档已建立：
  - `00_Doc/04_Agent/architecture.md`
  - `00_Doc/04_Agent/requirements.md`
- Phase 1 实施计划已补全：
  - `00_Doc/04_Agent/implementation_plan.md`
  - 当前状态为 `READY_FOR_IMPLEMENTATION`
  - 范围限定为 Platform UART → STM32 Impl → HAL Blocking UART。
- EasyLogger、SEGGER RTT 及既有日志适配代码已存在，`defaultTask` 当前会调用日志初始化；该旧日志链路不属于当前 Phase 1 重构范围。

### 尚未完成

- `04_Impl/impl_mcu/impl_platform_uart.c` 仍为空占位文件。
- `04_Impl/impl_mcu/impl_platform_uart.h` 尚未创建。
- `04_Impl/impl_mcu/impl_dma.c/.h` 仍为空占位文件。
- 新的 Platform UART / STM32 UART Impl 尚未正式加入 Keil 主工程。
- 尚未执行 Phase 1 的 Keil 全工程构建。
- 尚未执行 Platform → Impl → HAL 的板上 Blocking TX/RX Smoke Test。
- USART1 DMA / IDLE、HAL Async Callback、FreeRTOS 通知链路尚未实现。
- UART Service、Ring Buffer、APP 通信业务、协议解析尚未实现。

---

## 3. 当前 Phase 1 边界

### 本阶段执行

```text
platform_uart_t
    ↓
USART1 construct / bind
    ↓
lifecycle.init()
    ↓
lifecycle.start()
    ↓
platform_uart_write()/read()
    ↓
STM32 UART Impl
    ↓
HAL_UART_Transmit()/HAL_UART_Receive()
    ↓
USART1
```

必须实现：

- STM32 UART 私有 Context。
- USART1 与 Platform UART 的绑定入口。
- Platform Config → HAL UART Config 转换。
- Lifecycle。
- Blocking Write / Read。
- HAL → Platform Error Mapping。
- Keil 工程接入。
- 最小板上 TX/RX Smoke Test。

### 本阶段不执行

- DMA。
- IDLE。
- Circular Buffer。
- UART/DMA IRQ 数据链路。
- Async TX/RX STM32 实现。
- Ring Buffer。
- UART Service。
- FreeRTOS Notification 通信链路。
- APP 通信逻辑。
- 协议解析。
- 日志架构技术债重构。
- `platform_types -> board_types` 技术债重构。

发现必须扩大到上述范围才能继续时，不得自行扩大任务；在本文件记录 `BLOCKED` 并返回架构设计阶段。

---

## 4. Platform UART 已冻结契约

- `platform_uart_t` 的首字段保持 `platform_device_t`。
- 对象首次构造前使用 `PLATFORM_UART_INITIALIZER` 零初始化；同一对象禁止重复 `platform_uart_init()` 构造。
- 生命周期继续使用 `platform_lifecycle_ops_t`；UART Ops 不重复生命周期接口。
- 数据接口仅允许在 `PLATFORM_OBJECT_STARTED` 状态调用。
- 异步事件只在 `STARTED` 状态接受；这部分在 Phase 2 才接 STM32 DMA/IRQ。
- 同步接口失败时完成长度为 0；成功完成量不得超过请求长度。
- 异步 Buffer 生命周期、Callback ISR 约束和 `RX_DATA` 语义继续保持现有设计。
- 不得为了 STM32 Impl 修改 Platform UART 公共 API。
- Platform Header 不得新增 HAL / DMA / FreeRTOS 类型。

---

## 5. Phase 1 已确定实现决策

以下内容已经在 `implementation_plan.md` 固定，执行 Agent 不再自行选择。

### 5.1 USART1 Binding

- 当前只实现工程实际使用的 USART1。
- 上层提供 `platform_uart_t` 对象存储和 Platform Config。
- Impl 内部静态绑定 CubeMX `huart1`。
- 上层不得传入或看到 `UART_HandleTypeDef *`。
- 不建立通用动态 UART Registry。

### 5.2 Config Source of Truth

Platform `platform_uart_config_t` 作为 UART 对外语义配置。

Impl lifecycle `init()` 负责：

```text
Platform UART Config
        ↓
HAL UART Config translation
        ↓
huart1.Init
        ↓
HAL_UART_Init()
```

CubeMX 继续负责 Handle、GPIO、Clock/MSP 和生成框架。

当前至少验证 115200 / 8N1 / No Flow Control；当前没有 RTS/CTS GPIO，因此 Phase 1 不要求硬件流控。

### 5.3 Lifecycle

```text
CREATED / POWER_OFF
  -> init
INITIALIZED / POWER_IDLE
  -> start
STARTED / POWER_ACTIVE
  -> stop
STOPPED / POWER_IDLE
  -> start 可再次进入 STARTED
  -> deinit
CREATED / POWER_OFF
```

Phase 1 `process()` 为轻量 no-op，不轮询 UART。

### 5.4 HAL Error Mapping

```text
HAL_OK      -> PLATFORM_ERR_OK
HAL_BUSY    -> PLATFORM_ERR_BUSY
HAL_TIMEOUT -> PLATFORM_ERR_TIMEOUT
HAL_ERROR   -> PLATFORM_ERR_IO
other       -> PLATFORM_ERR_UNKNOWN
```

不得修改 Platform Error 枚举。

### 5.5 Concurrency

- Phase 1 不引入新的 UART Mutex。
- 调用方负责避免多个 Task 同时操作同一 Blocking UART。
- ISR 不参与 Phase 1 数据链路。
- Async/DMA 并发模型留到 Phase 2 重新设计。

---

## 6. 当前验证基线

最近一次已有验证记录：

```text
test_platform_uart_types: 0
test_platform_uart:       0
Platform UART 禁止依赖扫描: 0
```

该结果来自 Platform UART 阶段，不代表 Phase 1 STM32 Impl 已验证。

Phase 1 实施后必须重新执行：

1. Platform UART host tests。
2. Architecture dependency check。
3. Keil 全工程构建。
4. Board Blocking TX Smoke Test。
5. Board Blocking RX Smoke Test。
6. Lifecycle Smoke Test。

只有重新验证得到证据后才能更新完成状态。

---

## 7. Git / Workspace 规则

仓库最新文档更新已将 Phase 1 标记为可实施。

执行 Agent 接手时必须先：

```text
git status --short
```

并遵守：

- 不覆盖用户未提交修改。
- 不顺带提交与当前任务无关的变更。
- 修改前先确认文件当前内容和 SHA / 工作区状态。
- 若本地工作区状态与 GitHub `main` 不一致，以本地真实工作区为准，并在本文件记录差异。

当前 GitHub 侧 Agent 文档所在路径为：

```text
RTT_elog_DMA_UART_ring_project/00_Doc/04_Agent/
```

---

## 8. 执行 Agent 必读顺序

1. `00_Doc/04_Agent/handoff.md`
2. `00_Doc/04_Agent/architecture.md`
3. `00_Doc/04_Agent/requirements.md`
4. `00_Doc/04_Agent/implementation_plan.md`
5. `00_Doc/02_架构设计/Platform_UART抽象层设计.md`
6. `00_Doc/02_架构设计/嵌入式项目C代码设计规范.md`
7. 当前相关源码和 Keil / CubeMX 配置

项目自研代码默认使用中文注释。

---

## 9. 当前推荐动作

当前不再等待 DMA / IDLE / Ring Buffer 设计。

下一步直接执行 `implementation_plan.md` 的 Phase 1：

```text
STM32 UART Blocking Impl
```

即：

1. Preflight / git status。
2. 创建 `impl_platform_uart.h`。
3. 实现 `impl_platform_uart.c`。
4. 完成 Config Translation。
5. 完成 Lifecycle。
6. 完成 Blocking Write / Read。
7. 接入 Keil。
8. 构建。
9. 板上 TX/RX Smoke Test。
10. 更新本 handoff。

如果硬件测试尚未执行，但代码、主机测试和 Keil 构建已完成，只能标记：

```text
CODE_COMPLETE_PENDING_HARDWARE_VERIFICATION
```

不得标记完整 `COMPLETED`。

---

## 10. 下一阶段入口

Phase 1 验证通过后，再进入 Phase 2 设计：

```text
Platform UART Async RX
        ↓
STM32 UART DMA
        ↓
IDLE / DMA Position
        ↓
Platform RX_DATA Event
```

Phase 2 开始前重新确定：

- USART1 DMA Stream / Channel。
- DMA Normal / Circular 模式。
- IDLE 策略。
- DMA Buffer Size。
- Wrap Around 算法。
- IDLE / HT / TC 组合。
- Async Error / Cancel / Stop 语义。
- ISR / Task 并发边界。

不得由 Phase 1 执行 Agent提前实现。
