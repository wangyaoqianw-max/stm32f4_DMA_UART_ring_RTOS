# Board GPIO Binding + Target Smoke Test Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> 当前执行计划 / Current Active Plan  
> 状态：COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
> 日期：2026-09-02

**Goal:** 完成 LED、KEY、Software I2C SCL/SDA 四个板级 GPIO 资源到现有 Platform GPIO + STM32 GPIO Impl 的正式 Board/BSP 绑定，并完成 Host Test、Keil Build 和目标板 GPIO Smoke Test，从而关闭 Phase 2。

**Architecture:** 沿用现有 `Platform BSP -> Impl BSP -> Generic STM32 GPIO Impl` 绑定方式。Platform BSP 只暴露逻辑板级资源；具体 GPIO Port / Pin 只存在于 Impl BSP。BSP constructor 只负责对象构造与物理资源绑定，不执行 `platform_gpio_configure()`，不拥有 GPIO 生命周期。

**Tech Stack:** C、STM32F411CEU6、STM32 HAL GPIO、现有 Platform GPIO、Host C Test、Keil MDK-ARM、逻辑分析仪 / 万用表。

**Spec:**
- `00_Doc/04_Agent/development_roadmap.md` Phase 2
- `00_Doc/04_Agent/handoff.md`
- `00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md`
- `00_Doc/02_架构设计/GPIO_STM32_Impl_Phase1设计.md`

## Global Constraints

- 固定依赖方向保持 `APP -> Service -> Platform -> Impl -> Vendor`。
- 不修改已冻结的 Platform GPIO 公共 API / 类型。
- 不修改 `impl_platform_gpio_context_t`。
- 不增加 GPIO Registry、Context Pool、动态内存或 per-pin 通用实例框架。
- Platform GPIO / Generic STM32 GPIO Impl 不得知道 LED、KEY、SCL、SDA。
- Board/BSP 负责逻辑资源到具体 Port + Pin 的映射。
- BSP constructor 只 construct / bind，不调用 `platform_gpio_configure()`。
- GPIO Port RCC 继续由 CubeMX / Board Bootstrap 负责。
- 不实现 LED 产品语义、Button 消抖、Software I2C 时序或 Sensor 驱动。
- PA0 当前不使用 EXTI；Button Phase 第一版优先周期扫描。
- 不启用 STM32 Hardware I2C。
- 不修改当前已经确认正确的 CubeMX GPIO 模式。
- CubeMX 自动生成文件不得手工修改生成区；临时板测入口如确有需要只能放在 USER CODE 区，并在验收后恢复。
- 所有自研代码执行前必须完整读取 `00_Doc/02_架构设计/嵌入式项目C代码设计规范.md`。
- 每个实现 Task 提交前执行 Coding Standard Review。
- 若冻结设计与仓库现实存在实质冲突：`STOP / BLOCKED`，不得静默重设计。

当前冻结板级资源：

```text
Status LED      -> PC13 / active-low
User Key        -> PA0  / active-low
Soft I2C SCL    -> PB6
Soft I2C SDA    -> PB7
USART1 TX/RX    -> PA9 / PA10
```

当前 CubeMX 基线：

```text
PC13 : GPIO Output Push-Pull / No Pull / initial HIGH
PA0  : GPIO Input / Pull-Up
PB6  : GPIO Output Open-Drain / No Pull / initial HIGH
PB7  : GPIO Output Open-Drain / No Pull / initial HIGH
I2C1 : Disabled
EXTI : Not used for KEY
```

硬件语义：

```text
PC13 LOW  -> LED ON
PC13 HIGH -> LED OFF

PA0 LOW   -> KEY pressed
PA0 HIGH  -> KEY released

PB6/PB7 LOW  -> MCU actively pulls bus low
PB6/PB7 HIGH -> open-drain release; external pull-up raises bus
```

DHT20 与 MPU6050 模块原理图已确认 I2C 总线上存在外部上拉，因此 PB6/PB7 保持 `GPIO_NOPULL`。

---

# 0. Mandatory Preflight

执行前必须完整读取：

```text
00_Doc/00_项目需求/最终功能需求.md
00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md
00_Doc/02_架构设计/GPIO_STM32_Impl_Phase1设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/implementation_plan.md
```

检查参考实现：

```text
03_Platform/platform_bsp/platform_bsp_uart.h
04_Impl/impl_bsp/impl_platform_bsp_uart.c
Tests/platform_bsp_uart/test_platform_bsp_uart.c

03_Platform/platform_mcu/gpio/platform_gpio_types.h
03_Platform/platform_mcu/gpio/platform_gpio.h
03_Platform/platform_mcu/gpio/platform_gpio.c

04_Impl/impl_mcu/impl_platform_gpio.h
04_Impl/impl_mcu/impl_platform_gpio.c
Tests/impl_platform_gpio/

Core/Inc/main.h
Core/Src/gpio.c
RTT_elog_DMA_UART_ring_project.ioc
```

执行前确认 CubeMX 当前状态：

```text
PC13 -> LED_OUT
PA0  -> KEY_IN
PB6  -> I2C_SCL
PB7  -> I2C_SDA

PB6/PB7 = GPIO_MODE_OUTPUT_OD
PB6/PB7 = GPIO_NOPULL
PB6/PB7 initial = GPIO_PIN_SET

PC13 initial = GPIO_PIN_SET
PA0 pull = GPIO_PULLUP

GPIOB clock enabled
Hardware I2C disabled
KEY EXTI disabled
```

Preflight 固定汇报：

```text
Phase 2 Roadmap: READ
Platform GPIO Frozen Design: READ
GPIO STM32 Impl Frozen Design: READ
Coding Standard:
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
Status: READ
Agent Execution Rules: READ
CubeMX Phase 2 Configuration: INSPECTED
Current repository state: INSPECTED
Unrelated user changes: PRESERVED
```

若当前 `.ioc`、`gpio.c`、`main.h` 或现有 GPIO Impl 与上述冻结基线冲突，停止并报告，不得继续实现。

---

### Task 1: Define Platform BSP GPIO Contract and Host Test

**Files:**
- Create: `03_Platform/platform_bsp/platform_bsp_gpio.h`
- Create: `Tests/platform_bsp_gpio/test_platform_bsp_gpio.c`
- Create as required for Host isolation: `Tests/platform_bsp_gpio/main.h`
- Reuse or create minimal Fake HAL header only if the selected Host include strategy requires it.

**Interfaces:**
- Consumes: `platform_gpio_t`, `platform_error_t`.
- Produces:

```c
platform_error_t platform_bsp_gpio_construct_status_led(
    platform_gpio_t *gpio);

platform_error_t platform_bsp_gpio_construct_user_key(
    platform_gpio_t *gpio);

platform_error_t platform_bsp_gpio_construct_soft_i2c_scl(
    platform_gpio_t *gpio);

platform_error_t platform_bsp_gpio_construct_soft_i2c_sda(
    platform_gpio_t *gpio);
```

- [x] **Step 1: Create the public BSP GPIO header**

`platform_bsp_gpio.h` 只允许直接依赖：

```c
#include "platform_gpio.h"
```

公共 Header 不得暴露：

```text
GPIO_TypeDef
GPIOA / GPIOB / GPIOC
GPIO_PIN_x
impl_platform_gpio_context_t
STM32 HAL
```

公共 API Doxygen 按仓库 C 代码规范编写；`.c` 后续不重复相同公共 API 注释。

- [x] **Step 2: Write the failing Host test**

测试文件提供 Fake：

```c
platform_error_t impl_platform_gpio_construct(
    platform_gpio_t *gpio,
    const char *name,
    impl_platform_gpio_context_t *context);
```

Fake 至少记录：

```text
callCount
gpio
name
context pointer
context->port
context->pin
configured fake return result
```

必须覆盖：

```text
NULL gpio -> PLATFORM_ERR_INVALID_PARAM
           -> Impl constructor callCount remains 0

status_led
    -> name = "status_led_gpio"
    -> GPIOC / GPIO_PIN_13

user_key
    -> name = "user_key_gpio"
    -> GPIOA / GPIO_PIN_0

soft_i2c_scl
    -> name = "soft_i2c_scl_gpio"
    -> GPIOB / GPIO_PIN_6

soft_i2c_sda
    -> name = "soft_i2c_sda_gpio"
    -> GPIOB / GPIO_PIN_7

Impl constructor returns PLATFORM_ERR_IO
    -> BSP returns PLATFORM_ERR_IO unchanged
```

Fake `main.h` 必须提供当前 CubeMX 资源宏对应的 Host 替身，保证生产 BSP 源文件不因测试而改写：

```c
#define LED_OUT_Pin       GPIO_PIN_13
#define LED_OUT_GPIO_Port GPIOC
#define KEY_IN_Pin        GPIO_PIN_0
#define KEY_IN_GPIO_Port  GPIOA
#define I2C_SCL_Pin       GPIO_PIN_6
#define I2C_SCL_GPIO_Port GPIOB
#define I2C_SDA_Pin       GPIO_PIN_7
#define I2C_SDA_GPIO_Port GPIOB
```

测试还必须证明 BSP constructor 不执行：

```text
platform_gpio_configure
platform_gpio_write
platform_gpio_read
HAL_GPIO_Init
HAL_GPIO_WritePin
```

- [x] **Step 3: Build and run the focused test to verify RED**

从工程根目录使用 Host C 编译器，按现有 Test include 顺序优先放置 `Tests/platform_bsp_gpio/`，并链接：

```text
Tests/platform_bsp_gpio/test_platform_bsp_gpio.c
04_Impl/impl_bsp/impl_platform_bsp_gpio.c
```

当前预期失败原因：

```text
platform_bsp_gpio.h missing
or
platform_bsp_gpio_construct_* undefined
or
impl_platform_bsp_gpio.c missing
```

不得通过修改 Production Header 绕过 Host 隔离问题。

- [x] **Step 4: Coding Standard micro-review**

检查 Header Guard、文件头、snake_case API、lower_snake_case_t 类型规则、中文注释、Doxygen、4 空格缩进、无 TAB、无 HAL 泄漏。

- [x] **Step 5: Commit the contract/test slice**

Suggested commit:

```bash
git add RTT_elog_DMA_UART_ring_project/03_Platform/platform_bsp/platform_bsp_gpio.h \
        RTT_elog_DMA_UART_ring_project/Tests/platform_bsp_gpio/
git commit -m "test: define board GPIO binding contract"
```

---

### Task 2: Implement Board GPIO Physical Binding

**Files:**
- Create: `04_Impl/impl_bsp/impl_platform_bsp_gpio.c`
- Modify: `Tests/platform_bsp_gpio/test_platform_bsp_gpio.c` only if required by the already frozen production contract.

**Interfaces:**
- Consumes:

```c
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} impl_platform_gpio_context_t;

platform_error_t impl_platform_gpio_construct(
    platform_gpio_t *gpio,
    const char *name,
    impl_platform_gpio_context_t *context);
```

- Produces:

```text
status_led     -> PC13
user_key       -> PA0
soft_i2c_scl   -> PB6
soft_i2c_sda   -> PB7
```

- [x] **Step 1: Define the four caller-owned static Context objects**

Production BSP 必须使用 CubeMX `main.h` 生成的资源宏：

```c
static impl_platform_gpio_context_t g_statusLedContext =
{
    LED_OUT_GPIO_Port,
    LED_OUT_Pin
};

static impl_platform_gpio_context_t g_userKeyContext =
{
    KEY_IN_GPIO_Port,
    KEY_IN_Pin
};

static impl_platform_gpio_context_t g_softI2cSclContext =
{
    I2C_SCL_GPIO_Port,
    I2C_SCL_Pin
};

static impl_platform_gpio_context_t g_softI2cSdaContext =
{
    I2C_SDA_GPIO_Port,
    I2C_SDA_Pin
};
```

不得重新写一套重复的物理绑定：

```c
/* forbidden duplicate board mapping */
GPIOC, GPIO_PIN_13
GPIOA, GPIO_PIN_0
GPIOB, GPIO_PIN_6
GPIOB, GPIO_PIN_7
```

CubeMX `main.h` 是本板 Pin 宏的来源，Impl BSP 负责把这些宏转换为 Generic GPIO Context。

- [x] **Step 2: Implement the four BSP constructors**

每个函数流程固定为：

```text
gpio == NULL ?
    -> PLATFORM_ERR_INVALID_PARAM

valid gpio
    -> select frozen static Context
    -> impl_platform_gpio_construct(gpio, frozenName, &context)
    -> return result unchanged
```

例如 Status LED：

```c
platform_error_t platform_bsp_gpio_construct_status_led(
    platform_gpio_t *gpio)
{
    if (gpio == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return impl_platform_gpio_construct(gpio,
                                        "status_led_gpio",
                                        &g_statusLedContext);
}
```

其他三个函数使用对应冻结名称和 Context。

禁止在 BSP constructor 中调用：

```c
platform_gpio_configure();
platform_gpio_write();
platform_gpio_read();
HAL_GPIO_Init();
HAL_GPIO_WritePin();
```

- [x] **Step 3: Run `Tests/platform_bsp_gpio` and verify GREEN**

Expected:

```text
all Platform BSP GPIO binding tests PASS
```

- [x] **Step 4: Run GPIO regressions**

至少执行：

```text
Tests/platform_gpio
Tests/impl_platform_gpio
Tests/platform_bsp_uart
Tests/platform_bsp_gpio
```

Expected:

```text
ALL PASS
```

- [x] **Step 5: Run dependency/boundary scan**

确认：

```text
platform_bsp_gpio.h does not include STM32 HAL
platform_gpio.* has no LED / KEY / SCL / SDA knowledge
impl_platform_gpio.* has no LED / KEY / SCL / SDA knowledge
only impl_platform_bsp_gpio.c owns concrete board mapping
no RCC enable added to Generic GPIO Impl
```

- [x] **Step 6: Coding Standard Review**

至少回答：

```text
1. Naming / Header / comments compliant?
2. NULL paths checked?
3. Static Context lifetime valid for whole firmware lifetime?
4. No dynamic allocation / registry / pool?
5. No device polarity leaked into Generic GPIO Impl?
6. No generated/Vendor code modified?
```

- [x] **Step 7: Commit**

Suggested commit:

```bash
git add RTT_elog_DMA_UART_ring_project/04_Impl/impl_bsp/impl_platform_bsp_gpio.c \
        RTT_elog_DMA_UART_ring_project/Tests/platform_bsp_gpio/
git commit -m "feat: bind board GPIO resources"
```

---

### Task 3: Integrate Board GPIO BSP into Keil Project

**Files:**
- Modify: `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx` only as required to compile the new production source.

**Interfaces:**
- Consumes: `impl_platform_bsp_gpio.c`.
- Produces: target project can link the new `platform_bsp_gpio_construct_*()` APIs.

- [x] **Step 1: Inspect the existing BSP source group**

Locate the group currently containing:

```text
04_Impl/impl_bsp/impl_platform_bsp_uart.c
```

The new GPIO BSP implementation must be added to the same responsibility-level group; do not create a new arbitrary architecture layer.

- [x] **Step 2: Add `impl_platform_bsp_gpio.c` to the Keil project**

Add exactly:

```text
04_Impl/impl_bsp/impl_platform_bsp_gpio.c
```

Do not add Host Test sources to the product target.

- [x] **Step 3: Verify include paths**

The target must resolve at least:

```text
03_Platform/platform_bsp
03_Platform/platform_mcu/gpio
04_Impl/impl_mcu
Core/Inc
```

Reuse existing include paths when already present; do not duplicate them.

- [x] **Step 4: Run Keil Full Rebuild**

Gate:

```text
0 errors
no warning newly introduced by Phase 2 board binding
```

Existing historical warnings are not expanded into this Phase unless the new code causes or depends on them.

- [x] **Step 5: Inspect generated/configuration regression**

Confirm Task 3 did not unintentionally alter:

```text
USART1
DMA2_Stream2 circular RX
FreeRTOS
Clock Tree
RTT / EasyLogger
RTT_elog_DMA_UART_ring_project.ioc
Core/Src/gpio.c
```

- [x] **Step 6: Commit**

Suggested commit:

```bash
git add RTT_elog_DMA_UART_ring_project/MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
git commit -m "build: integrate board GPIO binding"
```

---

### Task 4: Prepare and Execute Target Board GPIO Smoke Test

**Files:**
- Create: `Tests/board_gpio_smoke/README.md`
- Create a minimal temporary smoke source under `Tests/board_gpio_smoke/` only if required for target execution.
- Modify `Core/Src/main.c` only inside an existing USER CODE section if a temporary call site is necessary; restore the normal call path after board verification.

**Interfaces:**
- Consumes:

```c
platform_bsp_gpio_construct_status_led()
platform_bsp_gpio_construct_user_key()
platform_bsp_gpio_construct_soft_i2c_scl()
platform_bsp_gpio_construct_soft_i2c_sda()

platform_gpio_configure()
platform_gpio_write()
platform_gpio_read()
```

- Produces: real-board verification record for the complete GPIO vertical slice.

## Hardware precondition

PB6/PB7 使用 Open-Drain + No Pull，因此测试释放 HIGH 时必须存在外部上拉。

当前 DHT20 与 MPU6050 模块原理图均已确认存在 I2C 外部上拉。板测时至少连接一个具有有效上拉且正确供电的模块，或使用明确的临时外部上拉到 3.3 V。

必须保证：

```text
MCU GND == Sensor / external pull-up GND
I2C pull-up rail == 3.3 V compatible rail
```

不得为了 Smoke Test 把 CubeMX 改成内部 Pull-Up。

- [x] **Step 1: Document the smoke-test sequence**

`Tests/board_gpio_smoke/README.md` 必须记录接线、预期电平、测试顺序和 PASS/FAIL 表格，不加入 Software I2C 协议步骤。

- [x] **Step 2: Construct the four GPIO objects through BSP**

Smoke harness 必须使用：

```c
platform_bsp_gpio_construct_status_led(&statusLedGpio);
platform_bsp_gpio_construct_user_key(&userKeyGpio);
platform_bsp_gpio_construct_soft_i2c_scl(&softI2cSclGpio);
platform_bsp_gpio_construct_soft_i2c_sda(&softI2cSdaGpio);
```

不得在 Smoke Test 直接构造 `GPIO_TypeDef + GPIO_PIN_x` 绕过 BSP。

- [x] **Step 3: Configure through Platform GPIO**

LED：

```c
const platform_gpio_config_t ledConfig =
{
    PLATFORM_GPIO_DIRECTION_OUTPUT,
    PLATFORM_GPIO_PULL_NONE,
    PLATFORM_GPIO_OUTPUT_PUSH_PULL,
    PLATFORM_GPIO_LEVEL_HIGH
};
```

KEY：

```c
const platform_gpio_config_t keyConfig =
{
    PLATFORM_GPIO_DIRECTION_INPUT,
    PLATFORM_GPIO_PULL_UP,
    PLATFORM_GPIO_OUTPUT_PUSH_PULL,
    PLATFORM_GPIO_LEVEL_LOW
};
```

`outputType` / `initialLevel` 对 INPUT 不产生硬件写入；这里保持确定值，仅为完整初始化结构体字段。

SCL / SDA：

```c
const platform_gpio_config_t softI2cConfig =
{
    PLATFORM_GPIO_DIRECTION_OUTPUT,
    PLATFORM_GPIO_PULL_NONE,
    PLATFORM_GPIO_OUTPUT_OPEN_DRAIN,
    PLATFORM_GPIO_LEVEL_HIGH
};
```

所有返回值必须检查；失败立即记录并停止后续相关 GPIO 操作。

- [x] **Step 4: Verify PC13 Status LED**

通过 `platform_gpio_write()` 验证：

```text
LOW  -> LED ON
HIGH -> LED OFF
```

最终恢复 HIGH / OFF。

Expected:

```text
PASS
```

- [x] **Step 5: Verify PA0 User Key**

通过 `platform_gpio_read()` 验证：

```text
released -> HIGH
pressed  -> LOW
```

本 Task 不做 debounce / single / double / long-press。

Expected:

```text
PASS
```

- [x] **Step 6: Verify PB6 SCL open-drain behavior**

使用逻辑分析仪或万用表：

```text
platform_gpio_write(LOW)
    -> SCL approximately 0 V

platform_gpio_write(HIGH)
    -> MCU releases SCL
    -> external pull-up raises SCL to HIGH
```

不得把 `HIGH` 描述为 MCU push-pull drive high。

- [x] **Step 7: Verify PB7 SDA write/read behavior**

验证：

```text
write LOW
read -> LOW

write HIGH / RELEASE
read -> HIGH
```

逻辑分析仪 / 万用表同时确认释放后由外部上拉形成高电平。

- [x] **Step 8: Confirm Phase 3 protocol is not implemented here**

本 Task 禁止发送：

```text
START
STOP
7-bit address
ACK / NACK
DHT20 command
MPU6050 register access
```

这些属于 Phase 3 Software I2C。

- [x] **Step 9: Restore normal firmware startup**

若使用了临时 USER CODE 调用：

```text
disable/remove temporary smoke call
retain reusable test source/doc under Tests only if useful
normal APP startup restored
```

不得留下开机自动翻转 SCL/SDA/LED 的临时产品行为。

- [x] **Step 10: Run final Keil Full Rebuild**

Expected:

```text
0 errors
normal firmware startup path restored
```

- [x] **Step 11: Record manual board result**

只有人工真实观察完成后才允许记录：

```text
PC13 Status LED                       PASS
PA0 User Key                          PASS
PB6 Open-Drain Pull-Low / Release     PASS
PB7 Open-Drain Pull-Low / Release     PASS
PB7 Physical Readback                 PASS
```

Agent 不得仅凭 Host Test 或编译结果把真实板测标记为 PASS。

---

### Task 5: Phase 2 Closure and Handoff

**Files:**
- Modify: `00_Doc/04_Agent/handoff.md`
- Modify: `00_Doc/04_Agent/implementation_plan.md`

**Interfaces:**
- Consumes: Task 1-4 verification results.
- Produces: Phase 2 closure state and clean handoff to Phase 3 design.

- [x] **Step 1: Record final board resource contract**

Handoff 必须包含：

```text
PC13 Status LED       active-low
PA0  User Key         active-low
PB6  Software I2C SCL open-drain / external pull-up / no internal pull
PB7  Software I2C SDA open-drain / external pull-up / no internal pull
```

- [x] **Step 2: Record CubeMX state**

```text
Board Resource Freeze            PASS
CubeMX Configuration             PASS
Board / GPIO Context Binding     PASS
GPIO BSP Host Test               PASS
GPIO Platform Regression         PASS
Keil Build                       PASS
Target Board GPIO Smoke Test     PASS only after real observation
Coding Standard Review           PASS
```

- [x] **Step 3: Run Phase 2 completion gate**

必须同时满足：

```text
Keil Build PASS

PC13 output controllable
PA0 input readable
PB6 open-drain pull-low/release verified
PB7 open-drain pull-low/release/read verified

LED / KEY / SCL / SDA no resource conflict
CubeMX / USER CODE boundary PASS

No hardware I2C introduced
No EXTI introduced
No Software I2C protocol code introduced
```

- [x] **Step 4: Mark Phase 2 completed only when all gates pass**

最终状态：

```text
Phase 2 — Board Resource + CubeMX Configuration
COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

若真实目标板尚未验证，则必须保持：

```text
Phase 2 — Board Resource + CubeMX Configuration
IMPLEMENTED / TARGET BOARD VERIFICATION PENDING
```

- [x] **Step 5: Stop before Phase 3 implementation**

不得继续实现 Software I2C。

下一步必须先做 Phase 3 专项设计并冻结：

```text
Software I2C object/interface model
SCL/SDA release/read strategy
microsecond timing source
clock-stretch policy if supported
ACK/NACK behavior
transaction timeout
bus recovery policy
DHT20 / MPU6050 required transaction subset
Host / logic-analyzer verification strategy
```

然后再重写新的 `00_Doc/04_Agent/implementation_plan.md`。

---

# Phase 2 Definition of Done

Phase 2 关闭前最终检查：

```text
[x] Platform BSP GPIO public contract exists
[x] Board physical binding exists only in Impl BSP
[x] PC13 / PA0 / PB6 / PB7 mappings match CubeMX
[x] BSP constructors do not configure hardware
[x] Host BSP binding tests PASS
[x] Platform GPIO regression PASS
[x] STM32 GPIO Impl regression PASS
[x] Keil Full Rebuild PASS
[x] LED board smoke PASS
[x] KEY board smoke PASS
[x] SCL open-drain smoke PASS
[x] SDA open-drain/read smoke PASS
[x] No EXTI introduced
[x] No Hardware I2C introduced
[x] No Software I2C protocol implementation leaked into Phase 2
[x] Coding Standard Review PASS
[x] handoff.md updated
```

Phase 2 完成后停止执行，等待 Phase 3 Software I2C 专项设计。

## Execution Record

2026-09-02: Preflight completed on `main`; frozen design, coding standard, CubeMX GPIO configuration and current repository state inspected. Unrelated user changes: none found.

2026-09-02: Task 1 completed. Added the Platform BSP GPIO public contract and Host Test with Host-only `main.h` / HAL substitutes. RED observed before the contract/implementation existed; focused Host Test PASS after the minimum binding implementation was available. Commit: `e89ae7f`.

2026-09-02: Task 2 implementation completed. Added four caller-owned static board Context bindings through CubeMX resource macros. Focused BSP Test, Platform GPIO, STM32 GPIO Impl and Platform BSP UART regressions PASS. Coding Standard Review: PASS. Commit: `b81d7a0`.

2026-09-02: Task 3 completed. Added `impl_platform_bsp_gpio.c` to the existing `impl/impl_bsp` Keil group without changing include paths or CubeMX-generated files. Full Rebuild linked with 0 errors; the new production BSP source introduced no warning. Commit: `b3b91c7`.

2026-09-02: Task 4 prepared `Tests/board_gpio_smoke/board_gpio_smoke.c/.h/README.md`. Temporary Keil compilation passed with 0 errors and no Smoke source warning; the temporary group was removed and a final normal Full Rebuild passed with 0 errors. Real target observation was not available. The manual procedure requires serial assistant + RTT logs together with meter/logic-analyzer readings.

2026-09-02: Independent review found that Smoke Test cleanup would deinitialize GPIOs before `osKernelStart()`, and that level mismatches were not emitted as structured failures. Removed the deinitialization path, added serial/RTT `FAIL` reporting for key and SDA level mismatches, and recompiled the temporary Smoke source with 0 errors and no Smoke source warning.

2026-09-02: Phase 2 remains `IMPLEMENTED / TARGET BOARD VERIFICATION PENDING`. PC13, PA0, PB6 and PB7 board smoke gates remain unchecked. Stop before Phase 3 Software I2C.

2026-09-02: User confirmed the target-board GPIO smoke test passed using the serial assistant + RTT logs together with physical observation / logic-analyzer verification: PC13 LED on/off, PA0 key release/press levels, PB6 open-drain pull-low/release, and PB7 open-drain pull-low/release/readback. All Phase 2 target-board gates are now PASS.

2026-09-02: Phase 2 closed as `COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED`. The next step is Phase 3 Software I2C dedicated design; do not implement the protocol until the Phase 3 interface, timing, release/read, ACK/NACK, timeout and recovery decisions are frozen.
