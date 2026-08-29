# Current Implementation Plan

## Metadata

- Status: READY_FOR_IMPLEMENTATION
- Phase: Phase 1
- Scope: STM32 UART Platform Blocking Impl
- Architecture Version: 1
- Target: STM32F411CEU6 / USART1
- Updated: 2026-08-29

---

## 1. Objective

将已经完成并冻结的 Platform UART 抽象接入 STM32F411 USART1，建立第一条真实的：

```text
Platform UART
    ↓
STM32 UART Impl
    ↓
STM32 HAL UART
    ↓
USART1
```

本阶段只验证 Platform → Impl → HAL 的阻塞式调用链，不提前实现完整 UART DMA 数据链路。

### 本阶段必须完成

- STM32 UART Impl Context。
- USART1 与 `platform_uart_t` 的绑定。
- Platform UART 构造入口。
- Lifecycle 的 STM32 实现。
- Blocking Write。
- Blocking Read。
- Platform Error 与 HAL 状态映射。
- Keil 工程接入。
- 最小板上 TX/RX Smoke Test。

### 本阶段明确不实现

- UART DMA。
- UART IDLE 检测。
- DMA Circular Buffer。
- UART IRQ / DMA IRQ 数据链路。
- Platform UART Async TX/RX 的 STM32 实现。
- Ring Buffer。
- UART Service。
- FreeRTOS Notification 数据链路。
- 协议解析。
- APP 通信业务。
- 与 UART Phase 1 无关的日志重构。

如实施过程中发现必须修改上述非本阶段内容才能继续，应停止扩大范围并在 `handoff.md` 标记 `BLOCKED`。

---

## 2. Current State

### Completed

- Platform common object model。
- Platform Device 基础对象。
- Platform Lifecycle 接口。
- Platform Error 类型。
- Platform UART types。
- Platform UART public API。
- Platform UART blocking API。
- Platform UART async API 契约。
- Platform UART host unit tests。
- Agent `requirements.md` 与 `architecture.md` 已建立。

### Not Implemented

- `04_Impl/impl_mcu/impl_platform_uart.c` 当前为空。
- `04_Impl/impl_mcu/impl_dma.c/.h` 当前为空。
- Keil 主工程尚未正式接入新的 Platform UART / Impl UART 文件。
- USART1 DMA 尚未在 CubeMX 配置。
- UART Service / Ring Buffer / APP 尚未实现。

### Current CubeMX Resource State

当前 `.ioc` 已配置：

- USART1 Asynchronous。
- PA9 = USART1_TX。
- PA10 = USART1_RX。
- USART1 基准配置为 115200 / 8N1 / No Flow Control。
- CMSIS-RTOS2 / FreeRTOS。

当前没有 USART1 DMA 映射；这不阻塞 Phase 1 Blocking Impl。

---

## 3. Frozen Architecture Decisions

本阶段不得重新设计以下内容。

### 3.1 Dependency Direction

```text
APP -> Service -> Platform -> Impl -> HAL / RTOS / Hardware
```

Platform 公共接口不得新增 HAL、DMA、FreeRTOS 类型依赖。

### 3.2 Existing Platform UART Contract

继续使用现有：

- `platform_uart_t`
- `platform_uart_config_t`
- `platform_uart_ops_t`
- `platform_lifecycle_ops_t`
- `platform_uart_init()`
- `platform_uart_write()`
- `platform_uart_read()`
- 已定义的异步接口和事件类型

不得为了方便 STM32 Impl 修改上述公共 API 语义。

### 3.3 Object Storage Ownership

- `platform_uart_t` 对象存储由调用方提供。
- Impl 不动态分配 `platform_uart_t`。
- 对象首次构造前必须使用 `PLATFORM_UART_INITIALIZER` 零初始化。
- Impl 通过构造/绑定函数向 `platform_uart_init()` 注入 lifecycle、ops 和 private context。

### 3.4 Phase 1 Instance Policy

- 当前只支持工程实际使用的 USART1。
- 不为“未来可能存在的多 UART”提前设计复杂注册中心或设备管理器。
- Impl 对外提供 USART1 的明确绑定入口，不向上层暴露 `UART_HandleTypeDef *`。

---

## 4. Files In Scope

### 4.1 Implement / Create

```text
04_Impl/impl_mcu/impl_platform_uart.c
04_Impl/impl_mcu/impl_platform_uart.h
```

`impl_platform_uart.h` 仅暴露当前 Platform UART 与 USART1 的构造/绑定入口，不暴露 HAL Handle。

### 4.2 Required Existing Platform Sources for Keil Integration

根据依赖加入 Keil 工程：

```text
03_Platform/platform_common/platform_object.c
03_Platform/platform_common/platform_device.c
03_Platform/platform_mcu/uart/platform_uart.c
04_Impl/impl_mcu/impl_platform_uart.c
```

并加入所需 Include Path，至少覆盖：

```text
03_Platform/platform_common
03_Platform/platform_mcu/uart
04_Impl/impl_mcu
04_Impl/impl_board
```

只添加真实编译依赖，不因为目录存在就批量加入无关模块。

### 4.3 Possibly Modify

仅在确有需要时：

- `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx`
- `Core/Src/freertos.c` 的 USER CODE 区域，用于最小板上 Smoke Test / 临时 Composition Root。

### 4.4 Do Not Modify

- `03_Platform/platform_mcu/uart/platform_uart.h`
- `03_Platform/platform_mcu/uart/platform_uart.c`
- `03_Platform/platform_mcu/uart/platform_uart_types.h`
- Platform Object / Device 公共语义。
- `05_Vendors/`。
- STM32 HAL Vendor 源码。
- FreeRTOS Vendor 源码。
- `impl_dma.c/.h`。
- Ring Buffer / Service / APP。
- 与本阶段无关的 Log 技术债。

若现有 Platform API 被证明无法正确实现本阶段目标，不得自行修改冻结接口；在 `handoff.md` 中记录原因并标记 `BLOCKED`。

---

## 5. STM32 UART Impl Design

## 5.1 Private Context

Impl 内部定义私有 Context，最小信息为：

```text
STM32 UART Impl Context
└── UART_HandleTypeDef *halUart
```

要求：

- Context 定义留在 Impl 层。
- Platform Header 不得看到 `UART_HandleTypeDef`。
- 当前 USART1 Context 静态分配，不使用 `malloc/free`。
- 不加入 DMA、RingBuffer、RTOS Handle 等 Phase 2/3 状态。

---

## 5.2 USART1 Binding Entry

`impl_platform_uart.h` 提供一个面向当前 USART1 的构造/绑定入口。

该入口负责：

1. 接收调用方提供的 `platform_uart_t` 对象存储。
2. 接收 Platform UART 静态配置。
3. 可选接收 callback / callbackContext；Phase 1 阻塞模式允许 callback 为 `NULL`。
4. 内部绑定 CubeMX 已生成的 `huart1`。
5. 注入 static const Lifecycle Ops。
6. 注入 static const UART Ops。
7. 注入静态 STM32 UART Impl Context。
8. 调用现有 `platform_uart_init()` 完成对象构造。

要求：

- 上层不传入 `UART_HandleTypeDef *`。
- 不创建通用动态 UART Registry。
- 不动态分配对象或 Context。
- 不直接启动硬件；硬件初始化仍由 lifecycle `init()` 完成。

---

## 5.3 Configuration Ownership

本阶段采用以下规则：

> Platform `platform_uart_config_t` 是 UART 对外语义配置；Impl 负责把该配置翻译为 STM32 HAL UART 配置。

CubeMX 继续负责：

- USART1 Handle 生成。
- GPIO Alternate Function。
- Peripheral Clock / MSP 基础资源。
- 工程生成框架。

Impl lifecycle `init()` 负责：

```text
platform_uart_config_t
        ↓
STM32 HAL UART config translation
        ↓
huart1.Init
        ↓
HAL_UART_Init()
```

`MX_USART1_UART_Init()` 当前可以继续保留作为 CubeMX 资源初始化入口；Impl `init()` 必须确保 Platform Config 最终真正作用于 USART1，而不是只保存配置但硬件仍使用另一套值。

### Phase 1 Required Configuration

至少保证当前基准配置：

```text
115200 baud
8 data bits
No parity
1 stop bit
No flow control
TX + RX
Oversampling 16
```

Platform 定义但当前 STM32/CubeMX 资源无法安全支持的配置组合，应由 Impl 返回 `PLATFORM_ERR_NOT_SUPPORTED`，不得修改 Platform 公共枚举来规避。

当前未配置 RTS/CTS GPIO，因此 Phase 1 不要求硬件流控。

---

## 5.4 Lifecycle State Machine

Phase 1 采用：

```text
platform_uart_init()
        ↓
CREATED / POWER_OFF
        ↓ lifecycle.init()
INITIALIZED / POWER_IDLE
        ↓ lifecycle.start()
STARTED / POWER_ACTIVE
        ↓ lifecycle.stop()
STOPPED / POWER_IDLE
        ├──── lifecycle.start() ────> STARTED
        ↓ lifecycle.deinit()
CREATED / POWER_OFF
```

这里 `deinit()` 表示释放当前硬件运行资源并使已经构造的 Platform 对象回到可重新 `init()` 的 CREATED 状态，不再次调用 `platform_uart_init()`。

### lifecycle.init(self)

- `self` 必须解释为 `platform_uart_t *`。
- 校验对象、Impl Context、HAL Handle 和当前状态。
- 将 Platform Config 转换到 `huart1.Init`。
- 调用 `HAL_UART_Init()`。
- HAL 成功后设置 Object State = `PLATFORM_OBJECT_INITIALIZED`。
- 设置 Device Power State = `PLATFORM_DEVICE_POWER_IDLE`。
- 失败时不得伪装成 INITIALIZED。

### lifecycle.start(self)

- 允许从 INITIALIZED 或 STOPPED 进入 STARTED。
- Phase 1 Blocking UART 无需启动 DMA / IRQ 数据流。
- 设置 Object State = `PLATFORM_OBJECT_STARTED`。
- 设置 Device Power State = `PLATFORM_DEVICE_POWER_ACTIVE`。

### lifecycle.process(self)

- Phase 1 不需要周期性 UART 处理。
- 实现为轻量 no-op，并保持确定返回值。
- 不在此处轮询 UART 硬件。

### lifecycle.stop(self)

- 只处理当前 Blocking Phase 所需状态收口。
- 不引入 DMA / Async cancel 逻辑。
- 设置 Object State = `PLATFORM_OBJECT_STOPPED`。
- 设置 Device Power State = `PLATFORM_DEVICE_POWER_IDLE`。
- Phase 1 不支持在另一个 Task 正执行阻塞 read/write 时并发 stop。

### lifecycle.deinit(self)

- 要求对象不处于 STARTED；调用者应先 stop。
- 调用 `HAL_UART_DeInit()`。
- 成功后设置 Device Power State = `PLATFORM_DEVICE_POWER_OFF`。
- Object State 回到 `PLATFORM_OBJECT_CREATED`，允许后续再次 lifecycle.init()。
- 不再次调用 `platform_uart_init()`，不清除 Platform 对象 identity/magic。

---

## 5.5 Blocking Write

Impl `write` 必须：

1. 从 `uart->implContext` 获取 STM32 UART Context。
2. 校验 HAL Handle。
3. 调用 `HAL_UART_Transmit()`。
4. 使用 Platform 已解析后的 timeout。
5. 仅在 `HAL_OK` 时令 `*writtenLength = dataLength`。
6. 失败时完成长度保持 0。
7. 不创建 Mutex。
8. 不调用 `printf()`。
9. 不输出大量日志。

`PLATFORM_UART_WAIT_FOREVER` 映射到 HAL 的永久等待语义；普通毫秒值直接按毫秒传递。

---

## 5.6 Blocking Read

Impl `read` 必须：

1. 从 `uart->implContext` 获取 STM32 UART Context。
2. 校验 HAL Handle。
3. 调用 `HAL_UART_Receive()`。
4. 使用 Platform 已解析后的 timeout。
5. 仅在 `HAL_OK` 时令 `*readLength = bufferSize`。
6. Timeout / Error / Busy 返回失败，完成长度保持 0。
7. Phase 1 不把阻塞 Read 伪装成“不定长接收”。

说明：

> 不定长 UART RX 是后续 DMA + IDLE + Service 阶段的目标；本阶段 Blocking Read 只用于验证 Platform → Impl → HAL 链路。

---

## 5.7 HAL Error Mapping

统一映射：

```text
HAL_OK      -> PLATFORM_ERR_OK
HAL_BUSY    -> PLATFORM_ERR_BUSY
HAL_TIMEOUT -> PLATFORM_ERR_TIMEOUT
HAL_ERROR   -> PLATFORM_ERR_IO
other       -> PLATFORM_ERR_UNKNOWN
```

不得为了 HAL 状态新增或修改现有 Platform Error 枚举。

---

## 5.8 Concurrency Boundary

Phase 1 不解决完整多任务 UART 并发。

约束：

- 不在 Impl 内新建 Mutex。
- 不修改当前旧 `usart.c` 中日志相关 Mutex 作为本阶段顺带重构。
- Platform UART Blocking API 的调用方在 Phase 1 负责避免多个 Task 同时操作同一 UART。
- ISR 不参与本阶段 Blocking 数据链路。
- 后续 Async/DMA 设计时重新评审并发和资源仲裁。

---

## 6. Implementation Steps

严格按以下顺序执行。

### Step 1 — Preflight

- 读取四份 Agent 文档。
- 读取 `Platform_UART抽象层设计.md`。
- 读取代码规范。
- 执行 `git status --short`。
- 保存并尊重所有与当前任务无关的用户改动。
- 确认当前 Keil 工程文件和 USART1 CubeMX 配置。

### Step 2 — Impl Header

创建 `impl_platform_uart.h`：

- 只暴露 Platform 类型和 USART1 构造/绑定入口。
- 不暴露 HAL Header / HAL Handle。

### Step 3 — Private Context + Static Ops

在 `impl_platform_uart.c`：

- 定义 STM32 private context。
- 绑定 `huart1`。
- 定义 static const lifecycle ops。
- 定义 static const UART ops。
- Async Ops 暂不实现，对应函数指针保持 `NULL`。

### Step 4 — Config Translation

实现 Platform UART Config → HAL UART Init 的转换与支持范围检查。

不得修改 Platform UART Config 类型。

### Step 5 — Lifecycle

按第 5.4 节实现：

- init
- start
- process
- stop
- deinit

所有状态更新必须发生在底层操作成功之后。

### Step 6 — Blocking Write / Read

实现：

- HAL blocking TX。
- HAL blocking RX。
- timeout mapping。
- HAL error mapping。
- completion length 契约。

### Step 7 — Keil Integration

仅加入本阶段真实依赖：

- Platform common sources。
- Platform UART source。
- STM32 UART Impl source。
- 对应 Include Path。

不得把空 Service、DMA 或其他无关目录批量加入编译。

### Step 8 — Build

执行 Keil 全工程构建。

要求：

- 记录构建命令。
- 记录 Error / Warning 数量。
- 修复由本次修改直接引入的编译 Error。
- 不通过改架构、屏蔽文件或修改 Vendor 来“消除”错误。

### Step 9 — Board Smoke Test

验证最小链路：

```text
platform_uart_t
    ↓
construct/bind USART1
    ↓
lifecycle.init
    ↓
lifecycle.start
    ↓
platform_uart_write/read
    ↓
Impl
    ↓
HAL UART
    ↓
USART1
```

可在 `freertos.c` USER CODE 区放置临时 Smoke Test Composition Root，但必须：

- 明确标记为测试/过渡代码。
- 不加入 Service / RingBuffer / 协议逻辑。
- 不把 `UART_HandleTypeDef` 传入 APP / Service / Platform。

### Step 10 — Handoff

执行完成后更新 `00_Doc/04_Agent/handoff.md`，至少记录：

- 实际完成项。
- 实际修改文件。
- Build 结果。
- Board Test 结果。
- 与计划的偏差。
- 已知问题。
- Blocker。
- 下一阶段建议。

---

## 7. Constraints

### Architecture

- 不修改五层依赖方向。
- 不新增 Platform → HAL / RTOS 依赖。
- 不修改冻结 Platform UART API。
- HAL 类型只能存在于 Impl / CubeMX / Vendor 区域。
- 不增加与当前单 USART1 需求无关的抽象框架。

### Memory

- UART 对象与 Impl Context 使用静态/调用方静态存储。
- 不使用动态内存。

### CubeMX

- 不修改 `.ioc` 增加 DMA；Phase 2 再处理。
- 必须修改生成文件时仅限 USER CODE 区域，且保持薄适配。

### Generated / Vendor Code

- 不修改 STM32 HAL Vendor 源码。
- 不修改 FreeRTOS Vendor 源码。
- 不修改 EasyLogger / RTT Vendor 源码。

### Scope

- 不实现 Async/DMA/IDLE。
- 不实现 RingBuffer。
- 不实现 UART Service。
- 不顺带重构 Log。
- 不顺带处理 `platform_types -> board_types` 技术债。

---

## 8. Verification

必须按层次验证，不得只以“代码写完”作为完成依据。

### 8.1 Static Architecture Check

确认：

- `platform_uart*.h` 没有新增 HAL / FreeRTOS Include。
- `impl_platform_uart.h` 没有向上层暴露 `UART_HandleTypeDef`。
- `impl_platform_uart.c` 是 HAL UART 依赖主要落点。
- 未修改 Vendor 源码。
- 未修改冻结 Platform UART API。

### 8.2 Existing Platform Unit Tests

重新执行现有：

```text
Tests/platform_uart/test_platform_uart_types.c
Tests/platform_uart/test_platform_uart.c
```

已有测试必须继续通过。

### 8.3 Keil Build

必须执行当前 Keil Target 的完整构建，并记录：

```text
Errors:
Warnings:
```

验收目标：

```text
0 Errors
```

本次修改新增 Warning 应修复；已有 Warning 应在 handoff 中明确区分。

### 8.4 Board TX Smoke Test

至少验证一次：

```text
platform_uart_write()
    ↓
USART1 实际输出正确字节
```

### 8.5 Board RX Smoke Test

至少验证一次固定长度 Blocking Read：

```text
PC / UART Sender
    ↓
USART1
    ↓
platform_uart_read()
    ↓
收到期望字节
```

本测试不用于证明 DMA、不定长接收或 RingBuffer 已完成。

### 8.6 Lifecycle Smoke Test

验证：

```text
CREATED
 -> init -> INITIALIZED
 -> start -> STARTED
 -> stop -> STOPPED
 -> start -> STARTED
 -> stop -> STOPPED
 -> deinit -> CREATED
```

确认非 STARTED 状态下 Platform Blocking Data API 仍按原契约拒绝调用。

---

## 9. Completion Criteria

Phase 1 只有在以下条件全部满足时才能标记 `COMPLETED`：

1. `impl_platform_uart.c` 已实现，不再是空占位文件。
2. 已建立不暴露 HAL Handle 的 USART1 构造/绑定入口。
3. Platform UART Config 能实际作用于 STM32 USART1 配置。
4. Lifecycle 按本计划状态机工作。
5. Blocking Write 经 Platform → Impl → HAL 成功到达 USART1。
6. Blocking Read 经 Platform → Impl → HAL 能完成固定长度接收。
7. HAL Error Mapping 符合本计划。
8. Platform UART 公共 API 未被修改。
9. Platform Header 未新增 HAL / RTOS 依赖。
10. 未引入 DMA / IDLE / RingBuffer / Service 范围扩张。
11. Keil 全工程构建达到 0 Error。
12. 现有 Platform UART 单元测试重新验证通过。
13. 板上 TX Smoke Test 已执行并记录结果。
14. 板上 RX Smoke Test 已执行并记录结果。
15. `handoff.md` 已记录实际结果、偏差和下一阶段入口。

如果硬件环境不可用，只能将 Phase 1 标记为：

```text
CODE_COMPLETE_PENDING_HARDWARE_VERIFICATION
```

不得标记为完整 `COMPLETED`。

---

## 10. Next Phase Boundary

Phase 1 结束后再单独设计 Phase 2：

```text
Platform UART Async RX
        ↓
STM32 UART DMA
        ↓
IDLE / DMA Position
        ↓
Platform RX_DATA Event
```

Phase 2 开始前必须重新确定：

- USART1 DMA Stream / Channel。
- DMA Normal / Circular 模式。
- IDLE 处理方案。
- DMA Buffer 大小。
- DMA Wrap Around 位置算法。
- IDLE / HT / TC 是否组合使用。
- Error / Cancel / Stop 的异步语义。
- ISR 与 Task 的并发边界。

不得在 Phase 1 中提前自行决定上述内容。
