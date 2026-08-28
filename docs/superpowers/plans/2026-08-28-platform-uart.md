# Platform UART Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现一个不泄漏 HAL、DMA 或 RTOS 类型的 Platform UART 抽象，同时支持阻塞收发、异步收发和事件回调。

**Architecture:** `platform_uart_t` 以 `platform_device_t` 为首字段，通过 `platform_uart_ops_t` 把数据面操作注入 Impl，通过已有 `platform_lifecycle_ops_t` 管理生命周期。Platform 包装函数只负责参数、对象和状态校验，然后转发 Ops 并原样传播 `platform_error_t`。

**Tech Stack:** C11、MinGW GCC、工程现有 `platform_common` 对象模型、无第三方测试框架的单文件测试程序。

**Spec:** `RTT_elog_DMA_UART_ring_project/00_Doc/02_架构设计/Platform_UART抽象层设计.md`

## Global Constraints

- 只实现 Platform 抽象，不修改 `04_Impl`、CubeMX 生成文件、HAL Callback 或 FreeRTOS 配置。
- `platform_uart.h` 和 `platform_uart_types.h` 不得引入 HAL、CMSIS-RTOS 或 FreeRTOS 头文件。
- `platform_device_t` 必须是 `platform_uart_t` 的首字段。
- UART Ops 不重复 `init/start/process/stop/deinit`；生命周期继续使用 `platform_lifecycle_ops_t`。
- 不使用动态内存，Platform 不复制传输 Buffer，不创建 RTOS 同步对象。
- `RX_DATA` 仅表示收到字节，不表示完整协议帧。
- 保留用户已有未提交改动，Git 操作只指定本计划的文件。

---

### Task 1: 建立 UART 公共类型

**Files:**
- Modify: `RTT_elog_DMA_UART_ring_project/03_Platform/platform_common/platform_types.h:42-44`
- Modify: `RTT_elog_DMA_UART_ring_project/03_Platform/platform_common/platform_device.h:35-48`
- Create: `RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart_types.h`
- Create: `RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart_types.c`

**Interfaces:**
- Consumes: `platform_error_t`、`platform_device_class_t`、工程现有 `uint8_t/uint32_t`。
- Produces: `platform_size_t`、`PLATFORM_DEVICE_CLASS_UART`、UART 配置枚举、`platform_uart_config_t`、`platform_uart_event_t`、`platform_uart_callback_t`。

- [ ] **Step 1: 写入类型约束的失败测试**

```c
#include "platform_uart_types.h"
#include "platform_device.h"

typedef char assert_platform_size_is_32_bit[(sizeof(platform_size_t) == 4U) ? 1 : -1];
typedef char assert_uart_class_appended[
    (PLATFORM_DEVICE_CLASS_UART == (PLATFORM_DEVICE_CLASS_POWER + 1)) ? 1 : -1];

int main(void)
{
    platform_uart_config_t config = {
        115200U,
        PLATFORM_UART_DATA_BITS_8,
        PLATFORM_UART_STOP_BITS_1,
        PLATFORM_UART_PARITY_NONE,
        PLATFORM_UART_FLOW_CONTROL_NONE,
        100U
    };

    return (115200U == config.baudRate) ? 0 : 1;
}
```

- [ ] **Step 2: 编译并确认测试因 UART 类型缺失而失败**

```powershell
gcc -std=c11 -Wall -Wextra -Werror `
  -I RTT_elog_DMA_UART_ring_project/04_Impl/impl_board `
  -I RTT_elog_DMA_UART_ring_project/03_Platform/platform_common `
  -I RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart `
  RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart_types.c `
  -o RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart_types.exe
```

Expected: FAIL，错误指向 UART 类型或枚举尚未定义。

- [ ] **Step 3: 实现公共类型**

Add to `platform_types.h`:

```c
typedef uint32 platform_size_t;
```

Append before `PLATFORM_DEVICE_CLASS_MAX`:

```c
PLATFORM_DEVICE_CLASS_UART,
```

Define `platform_uart_types.h` with these public declarations:

```c
#include "platform_types.h"
#include "platform_error.h"

#define PLATFORM_UART_TIMEOUT_USE_DEFAULT (0xFFFFFFFEU)
#define PLATFORM_UART_WAIT_FOREVER        (0xFFFFFFFFU)

typedef struct platform_uart platform_uart_t;

typedef enum {
    PLATFORM_UART_DATA_BITS_7 = 7,
    PLATFORM_UART_DATA_BITS_8 = 8,
    PLATFORM_UART_DATA_BITS_9 = 9
} platform_uart_data_bits_t;

typedef enum {
    PLATFORM_UART_STOP_BITS_1 = 0,
    PLATFORM_UART_STOP_BITS_2,
    PLATFORM_UART_STOP_BITS_MAX
} platform_uart_stop_bits_t;

typedef enum {
    PLATFORM_UART_PARITY_NONE = 0,
    PLATFORM_UART_PARITY_EVEN,
    PLATFORM_UART_PARITY_ODD,
    PLATFORM_UART_PARITY_MAX
} platform_uart_parity_t;

typedef enum {
    PLATFORM_UART_FLOW_CONTROL_NONE = 0,
    PLATFORM_UART_FLOW_CONTROL_RTS,
    PLATFORM_UART_FLOW_CONTROL_CTS,
    PLATFORM_UART_FLOW_CONTROL_RTS_CTS,
    PLATFORM_UART_FLOW_CONTROL_MAX
} platform_uart_flow_control_t;

typedef enum {
    PLATFORM_UART_DIRECTION_TX = 0,
    PLATFORM_UART_DIRECTION_RX,
    PLATFORM_UART_DIRECTION_BOTH,
    PLATFORM_UART_DIRECTION_MAX
} platform_uart_direction_t;

typedef enum {
    PLATFORM_UART_EVENT_TX_COMPLETE = 0,
    PLATFORM_UART_EVENT_RX_DATA,
    PLATFORM_UART_EVENT_ERROR,
    PLATFORM_UART_EVENT_CANCELED,
    PLATFORM_UART_EVENT_MAX
} platform_uart_event_type_t;

typedef struct {
    uint32_t baudRate;
    platform_uart_data_bits_t dataBits;
    platform_uart_stop_bits_t stopBits;
    platform_uart_parity_t parity;
    platform_uart_flow_control_t flowControl;
    uint32_t defaultTimeoutMs;
} platform_uart_config_t;

typedef struct {
    platform_uart_event_type_t type;
    platform_uart_direction_t direction;
    const uint8_t *data;
    platform_size_t dataLength;
    platform_error_t error;
} platform_uart_event_t;

typedef void (*platform_uart_callback_t)(platform_uart_t *uart,
                                         const platform_uart_event_t *event,
                                         void *callbackContext);
```

- [ ] **Step 4: 重新编译并运行类型测试**

Run the Step 2 compile command, then:

```powershell
& RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart_types.exe
```

Expected: compile succeeds without warnings; process exits with code `0`.

- [ ] **Step 5: 提交类型层**

```powershell
git add -- `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_common/platform_types.h `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_common/platform_device.h `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart_types.h `
  RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart_types.c
git commit -m "feat: add platform UART public types"
```

### Task 2: 实现 UART 对象构造与阻塞收发

**Files:**
- Create: `RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.h`
- Create: `RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.c`
- Create: `RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart.c`

**Interfaces:**
- Consumes: Task 1 的 UART 公共类型、`platform_device_init()` 和 `platform_object_is_valid()`。
- Produces: `platform_uart_t`、`platform_uart_ops_t`、`platform_uart_init_params_t`、`platform_uart_init()`、`platform_uart_write()`、`platform_uart_read()`。

- [ ] **Step 1: 写入对象构造、状态拒绝和阻塞 Ops 转发的失败测试**

Use a fake Ops table that records UART object, Buffer, length and timeout. The assertions include:

```c
TEST_ASSERT(PLATFORM_ERR_OK == platform_uart_init(&uart, &params));
TEST_ASSERT(PLATFORM_DEVICE_CLASS_UART == uart.device.dev_class);
TEST_ASSERT(115200U == uart.config.baudRate);
TEST_ASSERT(&fakeContext == uart.implContext);

TEST_ASSERT(PLATFORM_ERR_INVALID_STATE ==
            platform_uart_write(&uart, txData, 2U, 10U, &writtenLength));

uart.device.object.state = PLATFORM_OBJECT_STARTED;
TEST_ASSERT(PLATFORM_ERR_OK ==
            platform_uart_write(&uart, txData, 2U,
                                PLATFORM_UART_TIMEOUT_USE_DEFAULT,
                                &writtenLength));
TEST_ASSERT(100U == fakeContext.timeoutMs);
TEST_ASSERT(2U == writtenLength);
```

Also assert null pointers, zero lengths, invalid enum values, zero baud rate, missing lifecycle and missing Ops return `PLATFORM_ERR_INVALID_PARAM`; a null blocking Ops member returns `PLATFORM_ERR_NOT_SUPPORTED`.

- [ ] **Step 2: 编译并确认链接因 Platform UART 函数缺失而失败**

```powershell
gcc -std=c11 -Wall -Wextra -Werror `
  -I RTT_elog_DMA_UART_ring_project/04_Impl/impl_board `
  -I RTT_elog_DMA_UART_ring_project/03_Platform/platform_common `
  -I RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart `
  RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart.c `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_common/platform_object.c `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_common/platform_device.c `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.c `
  -o RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart.exe
```

Expected: FAIL because `platform_uart_init()` and blocking wrappers are absent.

- [ ] **Step 3: 定义 UART 对象、Ops 和初始化参数**

```c
typedef struct {
    platform_error_t (*write)(platform_uart_t *uart, const uint8_t *data,
                              platform_size_t dataLength, uint32_t timeoutMs,
                              platform_size_t *writtenLength);
    platform_error_t (*read)(platform_uart_t *uart, uint8_t *buffer,
                             platform_size_t bufferSize, uint32_t timeoutMs,
                             platform_size_t *readLength);
    platform_error_t (*writeAsync)(platform_uart_t *uart, const uint8_t *data,
                                   platform_size_t dataLength);
    platform_error_t (*readAsync)(platform_uart_t *uart, uint8_t *buffer,
                                  platform_size_t bufferSize);
    platform_error_t (*cancel)(platform_uart_t *uart,
                               platform_uart_direction_t direction);
} platform_uart_ops_t;

struct platform_uart {
    platform_device_t device;
    platform_uart_config_t config;
    const platform_uart_ops_t *ops;
    void *implContext;
    platform_uart_callback_t callback;
    void *callbackContext;
};

typedef struct {
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

- [ ] **Step 4: 实现最小构造和阻塞转发逻辑**

Use these private helpers:

```c
static platform_error_t platform_uart_validate_config(
    const platform_uart_config_t *config);
static platform_error_t platform_uart_validate_ready(
    const platform_uart_t *uart);
static uint32_t platform_uart_resolve_timeout(
    const platform_uart_t *uart, uint32_t timeoutMs);
```

`platform_uart_init()` validates `uart`, `params`, `name`, `lifecycle`, `ops` and every config enum, rejects zero baud and rejects `defaultTimeoutMs == PLATFORM_UART_TIMEOUT_USE_DEFAULT`, then calls:

```c
result = platform_device_init(&uart->device,
                              params->name,
                              PLATFORM_DEVICE_CLASS_UART,
                              params->caps,
                              params->lifecycle);
```

On success it copies `config`, `ops`, `implContext`, `callback` and `callbackContext`. Blocking wrappers zero the output length when that pointer is non-null, reject invalid Buffer/length/output pointers, require `PLATFORM_OBJECT_STARTED`, return `PLATFORM_ERR_NOT_SUPPORTED` for a null Ops member, resolve the default-timeout sentinel, and return the Ops result unchanged.

- [ ] **Step 5: 运行对象与阻塞测试**

Run the Step 2 compile command, then:

```powershell
& RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart.exe
```

Expected: all tests exit with code `0`; compiler emits no warnings.

- [ ] **Step 6: 提交对象与阻塞收发**

```powershell
git add -- `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.h `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.c `
  RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart.c
git commit -m "feat: add platform UART blocking API"
```

### Task 3: 实现异步收发、取消和事件通知

**Files:**
- Modify: `RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart.c`
- Modify: `RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.c`

**Interfaces:**
- Consumes: Task 2 的 `platform_uart_t`、Ops 表和 ready-state 校验。
- Produces: `platform_uart_write_async()`、`platform_uart_read_async()`、`platform_uart_cancel()`、`platform_uart_notify_event()`。

- [ ] **Step 1: 先增加异步和事件失败测试**

Add assertions that prove:

```c
TEST_ASSERT(PLATFORM_ERR_OK ==
            platform_uart_write_async(&uart, txData, 2U));
TEST_ASSERT(txData == fakeContext.data);

TEST_ASSERT(PLATFORM_ERR_OK ==
            platform_uart_read_async(&uart, rxData, sizeof(rxData)));
TEST_ASSERT(rxData == fakeContext.buffer);

TEST_ASSERT(PLATFORM_ERR_OK ==
            platform_uart_cancel(&uart, PLATFORM_UART_DIRECTION_BOTH));

TEST_ASSERT(PLATFORM_ERR_OK ==
            platform_uart_notify_event(&uart, &event));
TEST_ASSERT(&uart == callbackRecord.uart);
TEST_ASSERT(&callbackContext == callbackRecord.callbackContext);
TEST_ASSERT(event.dataLength == callbackRecord.event.dataLength);
```

Also cover null/zero Buffer inputs, invalid direction, missing async Ops, null callback, invalid event type, `RX_DATA` with null or zero data, `TX_COMPLETE` with the wrong direction, `ERROR` carrying `PLATFORM_ERR_OK`, and `CANCELED` carrying an error other than `PLATFORM_ERR_CANCELED`.

- [ ] **Step 2: 运行测试并确认因异步函数未实现而失败**

Run the Task 2 Step 2 compile command.

Expected: FAIL at link time for the four asynchronous/event functions.

- [ ] **Step 3: 实现异步包装和事件校验**

Async read/write require a registered callback before calling the corresponding Ops member. `platform_uart_cancel()` validates `direction < PLATFORM_UART_DIRECTION_MAX`. Event validation follows this table:

| Event | Required direction | Required data | Required length | Required error |
| --- | --- | --- | --- | --- |
| `TX_COMPLETE` | `TX` | non-null | greater than zero | `PLATFORM_ERR_OK` |
| `RX_DATA` | `RX` | non-null | greater than zero | `PLATFORM_ERR_OK` |
| `ERROR` | `TX`, `RX`, or `BOTH` | optional | any | not `PLATFORM_ERR_OK` |
| `CANCELED` | `TX`, `RX`, or `BOTH` | optional | any | `PLATFORM_ERR_CANCELED` |

After validation, notify exactly once:

```c
uart->callback(uart, event, uart->callbackContext);
return PLATFORM_ERR_OK;
```

- [ ] **Step 4: 运行异步与事件测试**

Run the Task 2 Step 2 compile command and execute `test_platform_uart.exe`.

Expected: all tests exit with code `0`; compiler emits no warnings.

- [ ] **Step 5: 提交异步能力**

```powershell
git add -- `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.c `
  RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart.c
git commit -m "feat: add platform UART asynchronous API"
```

### Task 4: 全量验证与边界检查

**Files:**
- Verify: `RTT_elog_DMA_UART_ring_project/03_Platform/platform_common/platform_types.h`
- Verify: `RTT_elog_DMA_UART_ring_project/03_Platform/platform_common/platform_device.h`
- Verify: `RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart_types.h`
- Verify: `RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.h`
- Verify: `RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.c`
- Verify: `RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart_types.c`
- Verify: `RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart.c`

**Interfaces:**
- Consumes: Tasks 1-3 的全部产物。
- Produces: 可独立编译、通过单元测试且不泄漏底层依赖的 Platform UART 模块。

- [ ] **Step 1: 运行两个单元测试程序**

Run the compile commands from Task 1 Step 2 and Task 2 Step 2, then execute both generated test programs.

Expected: both processes exit with code `0`; no compiler warnings.

- [ ] **Step 2: 检查 Platform 头文件没有底层依赖**

```powershell
rg -n "stm32|HAL_|UART_HandleTypeDef|DMA_HandleTypeDef|cmsis_os|FreeRTOS" `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart
```

Expected: no matches.

- [ ] **Step 3: 执行格式和变更范围检查**

```powershell
git diff --check
git status --short
```

Expected: `git diff --check` has no output; status contains only the user's pre-existing changes plus files listed in this plan.

- [ ] **Step 4: 检查公共 API 注释与设计一致**

Confirm in `platform_uart.h` that every public function documents state requirements, Buffer ownership, timeout units, output lengths, callback context, and ISR restrictions. Confirm no operation treats `RX_DATA` as a complete frame.

- [ ] **Step 5: 如果验证阶段产生修正，单独提交**

```powershell
git add -- `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_common/platform_types.h `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_common/platform_device.h `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart_types.h `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.h `
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/uart/platform_uart.c `
  RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart_types.c `
  RTT_elog_DMA_UART_ring_project/Tests/platform_uart/test_platform_uart.c
git commit -m "test: verify platform UART abstraction"
```

Skip this commit when Task 4 requires no source or test corrections.
