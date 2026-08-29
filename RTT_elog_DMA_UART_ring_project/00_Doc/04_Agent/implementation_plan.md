# Current Implementation Plan

## Metadata

- Status: READY_FOR_VERIFICATION
- Phase: UART Phase 1 Final Verification
- Scope: STM32 USART1 Blocking TX / RX / Lifecycle Board Verification
- Architecture Version: 1
- Target: STM32F411CEU6 / USART1 / FreeRTOS / Keil
- Updated: 2026-08-29

---

## 1. Objective

本阶段不再实现新的 UART 功能，只对已经完成的 STM32 UART Platform Blocking Impl 做最终板上验收。

验证链路：

```text
Platform UART
    ↓
STM32 UART Impl
    ↓
STM32 HAL UART
    ↓
USART1 PA9 / PA10
```

完成本阶段后，UART Phase 1 才可以正式标记 `COMPLETED`。

---

## 2. Preconditions

### 2.1 Log Phase 1

Log Phase 1 已完成代码、Host Test、Keil Build 和 RTT Runtime Smoke Test。

人工 RTT 验证已确认：

- `Platform_Log_Init()` 成功。
- INFO 日志正常可见。
- `Platform_Log_SetLevel(WARN)` 后 ERROR / WARN 可见，INFO 被过滤。
- `Platform_Log_EnableOutput(false)` 后日志停止。
- `Platform_Log_EnableOutput(true)` 后日志恢复。

临时日志测试代码已经恢复为正常周期日志代码。

恢复后曾出现 Keil `Invalid argument` 写 `.o` 文件的环境性 I/O Error；重启系统后用户确认构建恢复正常。该问题不是 C 代码、Log API 或 UART API Error。

### 2.2 UART Code Baseline

以下代码已存在并冻结：

```text
03_Platform/platform_common/platform_object.c/.h
03_Platform/platform_common/platform_device.c/.h
03_Platform/platform_mcu/uart/platform_uart.c/.h
03_Platform/platform_mcu/uart/platform_uart_types.h
04_Impl/impl_mcu/impl_platform_uart.c/.h
```

已有：

- USART1 专用构造入口。
- STM32 UART 私有 Context。
- Platform Config → HAL Config。
- Lifecycle init/start/process/stop/deinit。
- Blocking write/read。
- HAL Error Mapping。
- 9-bit byte stream 不兼容保护。
- Platform UART Host Test PASS 记录。
- Impl Config Mapping Host Test PASS 记录。

---

## 3. Frozen Scope

本阶段只允许：

- 在 `Core/Src/freertos.c` 的 USER CODE 区加入最小临时 UART Board Smoke Test。
- 使用 RTT 日志输出测试状态。
- 完成测试后恢复临时测试代码。
- 必要时修复本阶段暴露出的真实 UART Phase 1 Bug。
- 更新 `handoff.md`。

本阶段禁止：

```text
DMA
IDLE
ReceiveToIdle
HT / TC
UART IRQ Data Chain
Async TX / RX
RingBuffer
UART Service
FreeRTOS Notification
Protocol Parser
APP Communication
Log Architecture Refactor
Platform UART API Redesign
Vendor HAL Modification
```

如果板上验证证明必须改变冻结的 Platform UART API 或扩大架构范围，停止并标记 `BLOCKED`。

---

## 4. Hardware Baseline

USART1：

```text
TX: PA9
RX: PA10
Baud: 115200
Data: 8 bits
Parity: None
Stop: 1 bit
Flow Control: None
```

建议工具：

- USB-UART 串口模块或等价串口终端。
- J-Link + RTT Viewer 用于观察测试结果。

连接要求：

```text
MCU PA9  (USART1_TX) -> USB-UART RX
MCU PA10 (USART1_RX) <- USB-UART TX
MCU GND               -> USB-UART GND
```

USB-UART 使用 3.3 V TTL 电平。

---

## 5. Temporary Test Object

测试代码使用一个静态对象：

```c
static platform_uart_t s_uart = PLATFORM_UART_INITIALIZER;
```

配置固定为：

```c
static const platform_uart_config_t s_uartConfig = {
    .baudRate = 115200U,
    .dataBits = PLATFORM_UART_DATA_BITS_8,
    .stopBits = PLATFORM_UART_STOP_BITS_1,
    .parity = PLATFORM_UART_PARITY_NONE,
    .flowControl = PLATFORM_UART_FLOW_CONTROL_NONE,
    .defaultTimeoutMs = 3000U,
};
```

构造：

```c
impl_platform_uart_usart1_construct(
    &s_uart,
    "usart1",
    PLATFORM_DEVICE_CAP_NONE,
    &s_uartConfig,
    NULL,
    NULL);
```

构造不初始化硬件。

---

## 6. Verification Sequence

### Step 1 — Construct

调用 `impl_platform_uart_usart1_construct()`。

期望：

```text
return = PLATFORM_ERR_OK
object.state = PLATFORM_OBJECT_CREATED
```

重复构造不属于本次必测项。

### Step 2 — Non-STARTED Guard

在 CREATED 状态调用一次：

```c
platform_uart_write(...)
```

期望：

```text
PLATFORM_ERR_INVALID_STATE
writtenLength = 0
```

用于验证 Blocking API 的 Platform 状态保护仍然生效。

### Step 3 — Lifecycle Init

调用：

```c
s_uart.device.lifecycle->init(&s_uart);
```

期望：

```text
return = PLATFORM_ERR_OK
state = PLATFORM_OBJECT_INITIALIZED
power = PLATFORM_DEVICE_POWER_IDLE
```

该步骤同时验证 Platform UART Config 能实际应用到 USART1。

### Step 4 — Lifecycle Start

调用：

```c
s_uart.device.lifecycle->start(&s_uart);
```

期望：

```text
return = PLATFORM_ERR_OK
state = PLATFORM_OBJECT_STARTED
power = PLATFORM_DEVICE_POWER_ACTIVE
```

### Step 5 — Blocking TX

发送固定字符串：

```text
UART_PHASE1_TX_OK\r\n
```

通过：

```c
platform_uart_write()
```

验收：

- 返回 `PLATFORM_ERR_OK`。
- `writtenLength` 等于发送字节数。
- PC 串口终端实际收到完整字符串。

这一步必须有真实 USART1 对端证据，不能只看返回值。

### Step 6 — Blocking RX

测试固定长度 4 bytes。

MCU 调用：

```c
platform_uart_read(..., 4U, 5000U, ...);
```

在 5 秒内从 PC 串口终端发送：

```text
PING
```

验收：

```text
return = PLATFORM_ERR_OK
readLength = 4
buffer = {'P','I','N','G'}
```

结果可通过 RTT 输出：

```text
UART RX PASS: PING
```

本测试只是固定长度 Blocking RX，不得描述为不定长 RX、DMA RX 或 IDLE RX。

### Step 7 — Lifecycle Stop / Restart

调用：

```text
STARTED
 -> stop
STOPPED
```

期望：

```text
state = PLATFORM_OBJECT_STOPPED
power = PLATFORM_DEVICE_POWER_IDLE
```

在 STOPPED 状态再次调用 `platform_uart_write()`：

```text
expect PLATFORM_ERR_INVALID_STATE
```

然后：

```text
STOPPED
 -> start
STARTED
```

再次发送一个短字符串：

```text
UART_RESTART_OK\r\n
```

确认串口对端仍能收到。

### Step 8 — Final Stop / Deinit

调用：

```text
STARTED
 -> stop
STOPPED
 -> deinit
CREATED
```

期望：

```text
deinit return = PLATFORM_ERR_OK
state = PLATFORM_OBJECT_CREATED
power = PLATFORM_DEVICE_POWER_OFF
```

这一步验证 `HAL_UART_DeInit()` 与 Platform 状态回退语义。

---

## 7. Recommended Temporary Test Output

RTT 最少输出以下结果：

```text
UART CONSTRUCT: PASS
UART PRE-START GUARD: PASS
UART INIT: PASS
UART START: PASS
UART TX: PASS
UART RX: PASS [PING]
UART STOP GUARD: PASS
UART RESTART TX: PASS
UART DEINIT: PASS
UART PHASE1 BOARD TEST: PASS
```

任何一步失败时输出：

```text
<STEP>: FAIL error=<platform_error_t value>
```

失败后停止继续执行后续依赖步骤，避免错误状态继续传播。

---

## 8. Build Verification

临时测试代码加入后执行 Keil Full Rebuild。

必须确认：

```text
0 Error(s)
```

执行完硬件测试并恢复临时测试代码后，再执行一次 Full Rebuild。

最终提交状态必须基于恢复后的正常代码，而不是临时测试版本。

如果再次出现：

```text
I/O error writing .o
Invalid argument
```

先按环境问题处理，不要修改 UART 或 Log 代码来规避；此前同类问题已通过系统重启恢复。

---

## 9. Completion Criteria

以下全部有证据后，UART Phase 1 才可标记 `COMPLETED`：

1. Construct PASS。
2. Non-STARTED Blocking Guard PASS。
3. init PASS。
4. start PASS。
5. Blocking TX 实际串口输出 PASS。
6. Blocking fixed-length RX 实际输入 PASS。
7. stop PASS。
8. STOPPED Guard PASS。
9. restart PASS，重启后 TX PASS。
10. deinit PASS，状态回到 CREATED / POWER_OFF。
11. 临时测试代码已恢复。
12. 恢复后的 Keil Full Rebuild 为 0 Error。
13. `handoff.md` 已更新实际结果。

如果缺少板上 TX / RX 证据：

```text
CODE_COMPLETE_PENDING_HARDWARE_VERIFICATION
```

出现真实 UART 实现缺陷：

```text
NEEDS_FIX
```

需要修改冻结架构或 Platform UART API：

```text
BLOCKED
```

---

## 10. After This Phase

只有 UART Phase 1 `COMPLETED` 后，才进入设计阶段讨论 UART Phase 2：

```text
USART1
 ↓
DMA RX
 ↓
IDLE / DMA Position
 ↓
Platform UART RX_DATA Event
 ↓
UART Service
 ↓
RingBuffer
```

Phase 2 开始前必须重新设计并冻结：

- DMA Stream / Channel。
- Normal vs Circular。
- HAL UART ReceiveToIdle / IDLE 使用方式。
- DMA Buffer Size。
- DMA Position / Wrap 算法。
- IDLE / HT / TC 组合策略。
- Callback / ISR 与 Task 边界。
- Buffer Ownership / Lifetime。
- Error / Cancel / Stop 语义。

本阶段不得提前实现这些内容。
