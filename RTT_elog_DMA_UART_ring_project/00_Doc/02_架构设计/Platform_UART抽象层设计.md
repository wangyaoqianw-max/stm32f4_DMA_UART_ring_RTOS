# Platform UART 抽象层设计

## 1. 目标

在 `03_Platform` 建立与 STM32 HAL、具体 UART 实例、DMA 及 FreeRTOS 无关的 UART 抽象层，为上层
Service 提供稳定的阻塞式和异步式收发能力。

设计遵循工程依赖方向：

```text
APP -> Service -> Platform UART -> Impl -> HAL / DMA / Hardware
```

## 2. 范围

本次实现范围：

- UART 公共配置、事件和方向类型。
- `platform_uart_t` UART 设备对象。
- `platform_uart_ops_t` UART 数据操作表。
- UART 抽象对象初始化、阻塞收发、异步收发、取消和事件通知入口。
- 不依赖 HAL 的平台层单元测试。

本次不实现：

- STM32 USART1 具体实现。
- DMA、UART IDLE 中断或 HAL Callback 适配。
- Ring Buffer、协议解析或完整帧判断。
- FreeRTOS 互斥量、信号量、队列或任务分发。

## 3. 文件变更

| 文件 | 变更 |
| --- | --- |
| `03_Platform/platform_common/platform_device.h` | 在已有设备类别末尾增加 UART 类别 |
| `03_Platform/platform_common/platform_types.h` | 增加 `platform_size_t` |
| `03_Platform/platform_mcu/uart/platform_uart_types.h` | 定义 UART 配置、事件和传输方向类型 |
| `03_Platform/platform_mcu/uart/platform_uart.h` | 定义 UART 对象、Ops 表和公共 API |
| `03_Platform/platform_mcu/uart/platform_uart.c` | 实现参数校验、状态检查、Ops 转发和事件通知 |
| `Tests/platform_uart/test_platform_uart.c` | 使用假 Ops 验证抽象层行为 |

`platform_size_t` 在当前 32 位 STM32 平台上定义为 `uint32`。后续 Impl 如需缩窄为 HAL 长度类型，必须先做
边界检查。

## 4. 对象模型

`platform_uart_t` 使用 C 语言组合方式继承平台设备基类。`platform_device_t` 必须是第一个字段，以便
将 UART 对象安全地视为通用设备对象。

```text
platform_uart_t
├── platform_device_t device
├── platform_uart_config_t config
├── const platform_uart_ops_t *ops
├── void *implContext
├── platform_uart_callback_t callback
└── void *callbackContext
```

各字段职责：

- `device`：名称、设备类别、能力、电源状态和生命周期。
- `config`：在 `platform_uart_init()` 中复制，不保存外部配置指针。
- `ops`：Impl 注入的 UART 数据操作，不重复基类生命周期。
- `implContext`：Impl 私有上下文，Platform 不解析其内容。
- `callback`：异步收发事件回调。
- `callbackContext`：由调用者提供并在回调时原样返回。

`lifecycle` 和 `ops` 建议定义为 `static const`；它们以及 `implContext`、`callbackContext`
指向的对象必须至少有效至 `deinit` 完成。同一 UART 对象只允许调用一次
`platform_uart_init()`，重复构造返回 `PLATFORM_ERR_ALREADY_INITIALIZED`。首次构造前必须使用
`PLATFORM_UART_INITIALIZER` 将对象存储清零，避免读取未初始化的对象标识。

## 5. 公共类型

### 5.1 UART 配置

`platform_uart_config_t` 包含：

- `baudRate`：波特率。
- `dataBits`：数据位。
- `stopBits`：停止位。
- `parity`：校验方式。
- `flowControl`：流控方式。
- `defaultTimeoutMs`：阻塞接口的默认超时时间。

配置枚举的公共选项为：

```text
dataBits    : 7 / 8 / 9
stopBits    : 1 / 2
parity      : NONE / EVEN / ODD
flowControl : NONE / RTS / CTS / RTS_CTS
```

配置只描述上层可见行为，不包含 DMA Channel、IRQn、HAL Handle 或 RTOS Handle。

阻塞接口的 `timeoutMs` 为 `PLATFORM_UART_TIMEOUT_USE_DEFAULT` 时，Platform 将向 Ops 传入
`config.defaultTimeoutMs`。其他值均按调用者指定的毫秒超时传递，其中 `0U` 表示不等待，
`PLATFORM_UART_WAIT_FOREVER` 表示无限等待。

```c
#define PLATFORM_UART_TIMEOUT_USE_DEFAULT (0xFFFFFFFEU)
#define PLATFORM_UART_WAIT_FOREVER        (0xFFFFFFFFU)
```

### 5.2 异步事件

`platform_uart_event_type_t` 包含：

- `PLATFORM_UART_EVENT_TX_COMPLETE`
- `PLATFORM_UART_EVENT_RX_DATA`
- `PLATFORM_UART_EVENT_ERROR`
- `PLATFORM_UART_EVENT_CANCELED`

传输方向为 `TX`、`RX` 或 `BOTH`。事件和回调原型为：

```c
typedef struct
{
    platform_uart_event_type_t type;
    platform_uart_direction_t direction;
    const uint8_t *data;
    platform_size_t dataLength;
    platform_error_t error;
} platform_uart_event_t;

typedef void (*platform_uart_callback_t)(
    platform_uart_t *uart,
    const platform_uart_event_t *event,
    void *callbackContext);
```

`RX_DATA` 只表示当前收到的字节，不表示一个完整协议帧。

## 6. Ops 操作表

`platform_uart_ops_t` 只包含 UART 数据面操作：

```c
platform_error_t (*write)(platform_uart_t *uart,
                          const uint8_t *data,
                          platform_size_t dataLength,
                          uint32_t timeoutMs,
                          platform_size_t *writtenLength);

platform_error_t (*read)(platform_uart_t *uart,
                         uint8_t *buffer,
                         platform_size_t bufferSize,
                         uint32_t timeoutMs,
                         platform_size_t *readLength);

platform_error_t (*writeAsync)(platform_uart_t *uart,
                               const uint8_t *data,
                               platform_size_t dataLength);

platform_error_t (*readAsync)(platform_uart_t *uart,
                              uint8_t *buffer,
                              platform_size_t bufferSize);

platform_error_t (*cancel)(platform_uart_t *uart,
                           platform_uart_direction_t direction);
```

不在 Ops 中重复 `init/start/process/stop/deinit`；这些操作继续由
`platform_lifecycle_ops_t` 提供，以便通用设备管理器统一管理。

某个 Impl 不支持的 Ops 可以为 `NULL`，Platform 包装函数将返回
`PLATFORM_ERR_NOT_SUPPORTED`。

## 7. 公共 API

```c
platform_error_t platform_uart_init(
    platform_uart_t *uart,
    const platform_uart_init_params_t *params);

platform_error_t platform_uart_write(
    platform_uart_t *uart,
    const uint8_t *data,
    platform_size_t dataLength,
    uint32_t timeoutMs,
    platform_size_t *writtenLength);

platform_error_t platform_uart_read(
    platform_uart_t *uart,
    uint8_t *buffer,
    platform_size_t bufferSize,
    uint32_t timeoutMs,
    platform_size_t *readLength);

platform_error_t platform_uart_write_async(
    platform_uart_t *uart,
    const uint8_t *data,
    platform_size_t dataLength);

platform_error_t platform_uart_read_async(
    platform_uart_t *uart,
    uint8_t *buffer,
    platform_size_t bufferSize);

platform_error_t platform_uart_cancel(
    platform_uart_t *uart,
    platform_uart_direction_t direction);

platform_error_t platform_uart_notify_event(
    platform_uart_t *uart,
    const platform_uart_event_t *event);
```

`platform_uart_notify_event()` 是 Impl 向 Platform 上报异步事件的统一入口。Platform 校验对象和事件后，
调用用户注册的回调。事件仅在对象处于 `STARTED` 状态时接受；Impl 必须在退出 `STARTED`
前关闭事件源并排空尾部事件，防止停止或反初始化后访问失效的回调上下文。

`platform_uart_init_params_t` 定义为：

```c
typedef struct
{
    const char *name;
    uint32_t caps;
    platform_uart_config_t config;
    const platform_lifecycle_ops_t *lifecycle;
    const platform_uart_ops_t *ops;
    void *implContext;
    platform_uart_callback_t callback;
    void *callbackContext;
} platform_uart_init_params_t;
```

事件回调可以为 `NULL`，但此时异步读写接口返回 `PLATFORM_ERR_INVALID_STATE`。

## 8. 生命周期与状态

```text
platform_uart_init()
    -> CREATED + POWER_OFF

lifecycle->init()
    -> INITIALIZED + POWER_IDLE

lifecycle->start()
    -> STARTED + POWER_ACTIVE

lifecycle->stop()
    -> STOPPED + POWER_IDLE

lifecycle->deinit()
    -> STOPPED + POWER_OFF
```

Platform UART 数据收发接口仅在对象处于 `STARTED` 状态时允许调用。实际状态切换由后续
Impl 的生命周期函数负责。

## 9. Buffer 所有权和并发规则

### 9.1 阻塞接口

- Buffer 由调用者创建和持有。
- Buffer 在函数返回前必须保持有效。
- 接口成功时，`writtenLength` 或 `readLength` 返回实际完成量。
- 接口失败时，完成量固定为 0；Impl 返回超过请求长度的完成量时返回 `PLATFORM_ERR_OVERFLOW`。

### 9.2 异步发送

- 发送 Buffer 由调用者持有。
- 在 `TX_COMPLETE`、`ERROR` 或 `CANCELED` 事件到达前，Buffer 必须保持有效且不得修改。

### 9.3 异步接收

- 接收 Buffer 由调用者持有。
- 在 `RX_DATA`、`ERROR` 或 `CANCELED` 事件到达前，Buffer 必须保持有效。
- 事件中的数据指针和长度描述本次实际收到的字节。

### 9.4 并发

- 允许一个异步 TX 和一个异步 RX 同时进行。
- 同一方向的异步传输未结束时，再次提交由 Impl 返回 `PLATFORM_ERR_BUSY`。
- Platform 不隐式分配内存，不复制 Buffer，不创建 RTOS 同步对象。

## 10. 回调上下文

异步事件回调可能由中断上下文调用，因此回调使用者必须按中断安全约束编写：

- 不阻塞。
- 不使用普通 Mutex。
- 不动态分配内存。
- 不进行完整协议解析。
- 不打印大量日志。
- 如需进入任务上下文，由 Service 使用 ISR-safe 通知机制转发。

如某个 Impl 明确保证回调已延后到任务上下文，应在该 Impl 的接口文档中说明，但不改变
Platform 默认约束。

## 11. 错误处理

| 场景 | Platform 错误 |
| --- | --- |
| 空指针、零长度、非法枚举 | `PLATFORM_ERR_INVALID_PARAM` |
| UART 对象未构造 | `PLATFORM_ERR_NOT_INITIALIZED` |
| UART 未处于 `STARTED` | `PLATFORM_ERR_INVALID_STATE` |
| 同方向异步操作正在进行 | `PLATFORM_ERR_BUSY` |
| 对应 Ops 为 `NULL` | `PLATFORM_ERR_NOT_SUPPORTED` |
| 阻塞等待超时 | `PLATFORM_ERR_TIMEOUT` |
| 底层输入输出失败 | `PLATFORM_ERR_IO` |

Impl 返回的有效 `platform_error_t` 由 Platform 原样向上传播，不重复记录或吞掉根因。

## 12. 参数校验

`platform_uart_init()` 校验：

- UART 对象指针。
- 初始化参数指针。
- 设备名称。
- Ops 表。
- 生命周期表。
- 配置枚举范围和非零波特率。
- `defaultTimeoutMs` 不得为 `PLATFORM_UART_TIMEOUT_USE_DEFAULT`。

收发包装函数校验：

- UART 对象的 magic 和设备类别。
- 对象状态。
- Buffer 指针。
- Buffer 长度。
- 输出长度指针。
- 传输方向枚举。
- 对应 Ops 是否可用。

## 13. 测试设计

单元测试使用假 `platform_uart_ops_t`，不引入 HAL、DMA 或 FreeRTOS。

必须覆盖：

1. 成功构造 UART 对象，并正确复制配置、初始化设备基类和保存上下文。
2. 拒绝空指针、非法枚举和零波特率。
3. 在对象未进入 `STARTED` 时拒绝收发。
4. 阻塞收发向假 Ops 传递正确参数并返回实际长度。
5. 异步收发向假 Ops 传递正确的 Buffer 和长度。
6. Ops 缺失时返回 `PLATFORM_ERR_NOT_SUPPORTED`。
7. 假 Ops 返回的错误被原样传播。
8. `platform_uart_notify_event()` 传递正确的 UART 对象、事件和用户上下文。
9. `RX_DATA` 允许小于接收 Buffer 容量的实际长度。

测试按 Red-Green-Refactor 顺序执行：先编写并确认失败测试，再实现最小代码，最后在全部测试
通过后整理结构。

## 14. 验收标准

- Platform UART 公共头文件不引入 HAL、CMSIS-RTOS 或 FreeRTOS 头文件。
- UART 对象正确复用 `platform_device_t` 和 `platform_lifecycle_ops_t`。
- 阻塞和异步收发接口的语义、超时和 Buffer 所有权在头文件中明确。
- 异步回调不将 IDLE 或某次 DMA 数据块解释为完整协议帧。
- 不使用动态内存。
- 新增公共行为均有单元测试，且测试全部通过。
- 未修改 `04_Impl`、CubeMX 生成文件或用户现有 UART 占位文件之外的业务代码。
