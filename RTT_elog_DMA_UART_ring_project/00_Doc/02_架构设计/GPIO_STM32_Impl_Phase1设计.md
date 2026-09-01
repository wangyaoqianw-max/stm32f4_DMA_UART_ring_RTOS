# GPIO STM32 Impl Phase 1 设计

状态：FROZEN  
日期：2026-09-01

> 本文冻结已经 Host Verified 的 Platform GPIO 在 STM32F411 + HAL 环境中的第一阶段实现合同。
> 本阶段只实现通用 STM32 GPIO Impl，不绑定具体板级 LED / KEY / Software I2C 引脚，不修改 Platform GPIO 公共 API，不完成目标板 GPIO Smoke Test。

---

# 1. 设计目标

为现有 `Platform GPIO Phase 1` 提供 STM32F411 + HAL 的具体实现，使以下 Platform 能力能够落到 HAL：

```text
platform_gpio_configure
platform_gpio_write
platform_gpio_read
platform_gpio_deinit
```

依赖关系：

```text
Platform GPIO
    ↓ platform_gpio_ops_t
STM32 GPIO Impl
    ↓
STM32 HAL GPIO
```

本阶段完成后应达到：

```text
Generic STM32 GPIO Impl          IMPLEMENTED
Impl Host Test                   PASS
Platform GPIO Regression         PASS
Dependency Boundary              PASS
Coding Standard Review           PASS
Keil Build                       PASS
Target Board GPIO                NOT YET VERIFIED
Board Binding                    NOT YET IMPLEMENTED
```

真正的板级资源绑定、CubeMX 最终 GPIO 资源配置和目标板 GPIO Smoke Test 进入 `Phase 2 — Board Resource + CubeMX Configuration`。

---

# 2. 阶段边界

## 2.1 本阶段实现

```text
STM32 GPIO Context
Generic GPIO construct / binding
Platform config -> HAL GPIO mapping
GPIO configure
GPIO write
GPIO read
GPIO deinit
single-pin validation
output initial-level ordering
Host Fake-HAL verification
Keil compile integration
```

## 2.2 本阶段不实现

```text
PC13 LED binding
PA0 KEY binding
Software I2C SCL / SDA binding
LED active-low semantics
KEY active-low semantics
Button debounce / click recognition
EXTI / NVIC / IRQ callback
Software I2C protocol
DHT20 / MPU6050
APP Control FSM
GPIO toggle
Alternate Function
Analog Mode
GPIO public speed API
GPIO registry / context pool
Dynamic allocation
GPIO RCC ownership
```

不得为了 Phase 1 编译或测试方便提前实施 Phase 2 的 Board Resource / CubeMX 工作。

---

# 3. 文件位置

生产代码：

```text
04_Impl/impl_mcu/
├── impl_platform_gpio.h
└── impl_platform_gpio.c
```

Host Test：

```text
Tests/impl_platform_gpio/
```

测试目录允许提供最小 STM32 HAL Stub / Fake Header，用于在 Host 环境捕获 `HAL_GPIO_Init()`、`HAL_GPIO_WritePin()`、`HAL_GPIO_ReadPin()`、`HAL_GPIO_DeInit()` 行为。

本阶段不创建：

```text
04_Impl/impl_bsp/impl_platform_bsp_gpio.c
```

Board / BSP 绑定属于 Phase 2。

---

# 4. 核心设计：Generic Impl + Caller-owned Context

GPIO Impl 采用通用实现，不在 `impl_platform_gpio.c` 内维护固定 Pin 实例、Context Pool 或 Registry。

冻结结构：

```c
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} impl_platform_gpio_context_t;
```

Context 只描述：

```text
这个 Platform GPIO 在 STM32 上对应哪个 GPIO Port + Pin
```

Context 不保存：

```text
platform_gpio_t *反向指针
LED / KEY / SCL / SDA 语义
active-low / active-high
运行状态
callback
registry index
```

原因：GPIO Phase 1 没有异步 IRQ / HAL Callback 反向事件流，因此不需要像当前 UART Impl 一样保存 Platform 对象反向指针。

---

# 5. Context 所有权与生命周期

冻结原则：

> Context storage 由调用者拥有，GPIO Impl 只引用，不分配、不释放。

禁止：

```text
malloc / free
static context pool
fixed g_gpioPc13Context / g_gpioPa0Context
runtime registry
```

Phase 2 可以根据具体板级资源静态定义多个 Context，例如 LED / KEY / SCL / SDA，但增加 GPIO 使用者不得要求修改 Generic STM32 GPIO Impl。

`platform_gpio_t` 通过自身的：

```c
void *implContext;
```

单向引用 `impl_platform_gpio_context_t`。

Context 必须至少在对应 `platform_gpio_t` 使用期间保持有效。

---

# 6. Construct API

推荐冻结为：

```c
platform_error_t impl_platform_gpio_construct(
    platform_gpio_t *gpio,
    const char *name,
    impl_platform_gpio_context_t *context);
```

职责：

```text
1. 校验 gpio / context
2. 校验 context->port / context->pin
3. 构造 platform_gpio_init_params_t
4. 注入 STM32 GPIO Ops
5. 绑定 implContext = context
6. 调用 platform_gpio_init()
```

`impl_platform_gpio_construct()` 不得调用：

```text
HAL_GPIO_Init
HAL_GPIO_WritePin
HAL_GPIO_ReadPin
HAL_GPIO_DeInit
RCC Enable
```

因此它只建立抽象对象与实现绑定，不改变硬件。

真正硬件配置仍由：

```text
platform_gpio_configure()
    -> STM32 GPIO Ops.configure()
```

触发。

---

# 7. 一对象一物理 Pin 合同

一个 `platform_gpio_t` 只能对应一个物理 GPIO Pin。

Context 中的 `pin` 必须是单 bit：

```text
GPIO_PIN_0
GPIO_PIN_1
...
GPIO_PIN_15
```

不允许：

```text
0
GPIO_PIN_0 | GPIO_PIN_1
GPIO_PIN_All
任意多 bit mask
```

虽然 STM32 HAL `HAL_GPIO_Init()` 支持多 Pin bit mask，但 Platform GPIO 当前不是 GPIO Group / Port API，因此 Impl 必须比 HAL 的 `IS_GPIO_PIN()` 更严格。

推荐内部校验语义：

```text
pin != 0
AND
pin 仅有一个 bit 为 1
```

例如：

```c
(pin != 0U) && ((pin & (pin - 1U)) == 0U)
```

同时应保证 pin 不超出 STM32 GPIO 16-bit Pin mask。

---

# 8. Port 校验

Phase 1 不在 GPIO Impl 内维护 `GPIOA / GPIOB / GPIOC ...` 白名单。

只要求：

```text
context != NULL
context->port != NULL
context->pin = valid single physical pin
```

Port / Pin Context 是受控的静态板级绑定数据，不来自外部运行时输入。

具体 MCU 引脚资源合法性和资源冲突由 Phase 2 的 Board Resource / CubeMX Configuration 负责冻结。

---

# 9. Context 获取与防御性校验

`configure / write / read / deinit` 应统一通过 private helper 获取 Context，例如：

```text
stm32_gpio_get_context()
```

建议语义：

```text
gpio == NULL
    -> PLATFORM_ERR_INVALID_PARAM

gpio->implContext == NULL
    -> PLATFORM_ERR_NOT_INITIALIZED

context->port == NULL
    -> PLATFORM_ERR_NOT_INITIALIZED

context->pin invalid
    -> PLATFORM_ERR_INVALID_PARAM
```

Platform 层已经负责 `initialized / configured / direction` 等公共状态约束；Impl 不复制一套 Platform 状态机，只检查执行 HAL 所需的具体实现上下文。

---

# 10. Platform Config -> HAL 映射

现有 Platform 配置：

```c
platform_gpio_config_t
{
    direction;
    pull;
    outputType;
    initialLevel;
}
```

STM32 HAL 使用 `GPIO_InitTypeDef`。

## 10.1 Direction / Output Type

冻结映射：

| Platform | HAL |
| --- | --- |
| INPUT | `GPIO_MODE_INPUT` |
| OUTPUT + PUSH_PULL | `GPIO_MODE_OUTPUT_PP` |
| OUTPUT + OPEN_DRAIN | `GPIO_MODE_OUTPUT_OD` |

INPUT 模式下 `outputType` 和 `initialLevel` 虽然仍由 Platform 保证为合法枚举，但 STM32 Impl 不赋予硬件意义。

## 10.2 Pull

| Platform | HAL |
| --- | --- |
| `PLATFORM_GPIO_PULL_NONE` | `GPIO_NOPULL` |
| `PLATFORM_GPIO_PULL_UP` | `GPIO_PULLUP` |
| `PLATFORM_GPIO_PULL_DOWN` | `GPIO_PULLDOWN` |

## 10.3 Speed

Platform Phase 1 不公开 GPIO Speed，因此不得新增 Platform Speed 枚举或字段。

STM32 GPIO Impl Phase 1 使用固定私有策略：

```text
GPIO_SPEED_FREQ_LOW
```

可通过 Impl private macro 表达，例如：

```c
#define STM32_GPIO_DEFAULT_SPEED GPIO_SPEED_FREQ_LOW
```

该策略适用于当前 LED、KEY 和后续低速 Software I2C 基线；若真实板测出现明确需求，再通过专项设计调整，不反向污染 Platform API。

## 10.4 Alternate

Phase 1 不支持 Alternate Function。

`GPIO_InitTypeDef.Alternate` 不参与当前 GPIO 能力语义，可初始化为安全确定值，避免未初始化结构字段。

---

# 11. Configure 行为

`configure` 的职责：

```text
1. 获取并校验 STM32 Context
2. 将 Platform Config 转换为 GPIO_InitTypeDef
3. OUTPUT 时先准备 initialLevel
4. 调用 HAL_GPIO_Init()
5. 返回 Platform Error
```

## 11.1 OUTPUT initial-level ordering

冻结顺序：

```text
HAL_GPIO_WritePin(initialLevel)
    ↓
HAL_GPIO_Init(output mode)
```

不得反过来先切 Output 再写初始电平。

目的：减少从复位 / 输入状态切到输出状态时的错误瞬态，尤其保护后续可能出现的：

```text
CS
RESET
ENABLE
Open-Drain Software I2C
```

电平映射：

```text
PLATFORM_GPIO_LEVEL_LOW  -> GPIO_PIN_RESET
PLATFORM_GPIO_LEVEL_HIGH -> GPIO_PIN_SET
```

Open Drain + HIGH 表示在输出数据寄存器层面准备释放线路。

## 11.2 INPUT configure

INPUT 只执行：

```text
map Mode / Pull
HAL_GPIO_Init()
```

不得因为 `initialLevel` 字段存在而执行无意义的 `HAL_GPIO_WritePin()`。

---

# 12. Write 行为

Platform 层已经确保：

```text
initialized == true
configured == true
direction == OUTPUT
level valid
```

STM32 Impl 负责：

```text
get context
map logical level
HAL_GPIO_WritePin(port, pin, state)
```

冻结映射：

```text
LOW  -> GPIO_PIN_RESET
HIGH -> GPIO_PIN_SET
```

Impl 不处理 active-low / active-high 设备极性。

STM32 HAL `HAL_GPIO_WritePin()` 返回 `void`，正常调用没有 HAL status 可映射；成功执行后返回 `PLATFORM_ERR_OK`。

---

# 13. Read 行为

STM32 Impl：

```text
get context
HAL_GPIO_ReadPin(port, pin)
map GPIO_PinState -> platform_gpio_level_t
```

映射：

```text
GPIO_PIN_RESET -> PLATFORM_GPIO_LEVEL_LOW
GPIO_PIN_SET   -> PLATFORM_GPIO_LEVEL_HIGH
```

Platform 已允许 INPUT 和 OUTPUT 都读取，因此 Impl 不额外限制方向。

Impl 返回实际读取的逻辑电平，不解释 LED / KEY 等设备语义。

---

# 14. Deinit 行为

STM32 Impl 直接执行：

```c
HAL_GPIO_DeInit(context->port, context->pin);
```

然后返回：

```text
PLATFORM_ERR_OK
```

STM32 HAL `HAL_GPIO_DeInit()` 返回 `void`，无需伪造 HAL status mapping。

Platform 层负责在 Ops 成功返回后：

```text
configured = false
```

Impl 不直接修改 `platform_gpio_t` 的 Platform 状态字段。

`deinit()` 不关闭 GPIO Port RCC。

---

# 15. HAL Error / Platform Error 语义

GPIO HAL 基础 API 与 UART 不同：

```text
HAL_GPIO_Init       -> void
HAL_GPIO_DeInit     -> void
HAL_GPIO_WritePin   -> void
HAL_GPIO_ReadPin    -> GPIO_PinState
```

因此 Phase 1 不创建无意义的 `stm32_gpio_map_hal_status()`。

可明确检测的错误来自对象 / Context / Pin 参数：

```text
NULL / missing context
invalid single-pin binding
missing port
```

这些错误转换为现有 `platform_error_t`，不新增 GPIO 专用错误码。

---

# 16. RCC / CubeMX / Board 所有权

冻结原则：

> GPIO STM32 Impl 不负责 GPIO Port RCC Enable / Disable。

GPIO Port Clock 属于系统基础资源初始化，由 CubeMX / Board Bootstrap 负责。

调用 `platform_gpio_configure()` 的前置条件：

```text
对应 GPIO Port Clock 已经启用
```

Phase 1 不实现：

```text
__HAL_RCC_GPIOA_CLK_ENABLE()
__HAL_RCC_GPIOB_CLK_ENABLE()
...
```

Phase 2 根据最终板级资源表和 CubeMX 配置决定实际需要启用的 GPIO Port Clock。

职责边界：

```text
CubeMX / Board Bootstrap
    -> GPIO Port RCC

STM32 GPIO Impl
    -> Pin Mode
    -> Pull
    -> Output Type
    -> Initial / Runtime Level

Board / BSP Binding
    -> 具体 Port + Pin
    -> 设备语义 / 极性
```

这样避免 CubeMX 与 Impl 同时成为 RCC / Pin 配置真值源。

---

# 17. Board / BSP 边界

本阶段 Generic GPIO Impl 不知道：

```text
PC13
PA0
LED
KEY
SCL
SDA
Active Low
```

这些内容属于 Phase 2 及后续设备模块。

后续 Board Binding 可以为多个 Platform GPIO 提供 caller-owned Context，例如：

```text
LED Context
KEY Context
SCL Context
SDA Context
```

但 Generic STM32 GPIO Impl 不因增加这些实例而修改。

---

# 18. Host Test 设计

建立：

```text
Tests/impl_platform_gpio/
```

使用最小 HAL Stub / Fake HAL 捕获调用行为。

至少覆盖：

## 18.1 Construct / Context

```text
NULL gpio
NULL context
NULL port
zero pin
multi-bit pin
valid single pin
Platform object successfully bound to context and STM32 Ops
construct does not call HAL
```

## 18.2 Config Mapping

```text
INPUT + NOPULL
INPUT + PULLUP
INPUT + PULLDOWN
OUTPUT + PUSH_PULL
OUTPUT + OPEN_DRAIN
Speed always LOW
```

## 18.3 Initial Level Ordering

OUTPUT 必须验证调用顺序：

```text
WRITE(initialLevel)
INIT
```

INPUT 必须验证不会额外调用 WRITE。

## 18.4 Write

```text
LOW  -> GPIO_PIN_RESET
HIGH -> GPIO_PIN_SET
correct port / pin forwarded
```

## 18.5 Read

```text
GPIO_PIN_RESET -> PLATFORM LOW
GPIO_PIN_SET   -> PLATFORM HIGH
correct port / pin forwarded
```

## 18.6 Deinit

```text
correct HAL_GPIO_DeInit(port, pin)
no RCC operation
Platform configured state cleared by Platform layer after success
```

## 18.7 Regression

继续运行现有：

```text
Tests/platform_gpio/
```

确保 Impl 阶段不修改 Platform GPIO 已冻结公共行为。

---

# 19. Keil 验证

Phase 1 必须把新的 `impl_platform_gpio.c` 纳入 Keil 工程并完成实际 Build 验证。

验证只证明：

```text
STM32 HAL integration compiles
header/include dependency is valid
production target can link Impl
```

不证明：

```text
PC13 LED works
PA0 KEY works
real GPIO level correct
Board Resource verified
```

这些目标板行为属于 Phase 2。

---

# 20. Dependency Boundary

允许：

```text
04_Impl/impl_mcu/impl_platform_gpio.*
    -> Platform GPIO public headers
    -> STM32 HAL GPIO headers
```

禁止新增：

```text
APP -> Impl
Service -> Impl
Platform public header -> STM32 HAL
Generic GPIO Impl -> LED / KEY / Sensor business
```

Host Test Stub 不得污染 Production include path 或生产代码。

---

# 21. Logging / RTOS / Concurrency

GPIO STM32 Impl Phase 1 不主动增加日志。

理由：

- GPIO 基础操作频率未来可能很高，尤其 Software I2C；
- 逐 GPIO 操作日志会污染时序和 RTT；
- APP / Service 根据返回结果记录关键错误即可。

本阶段不引入：

```text
Mutex
Semaphore
Critical Section
RTOS API
```

GPIO 本身的多调用者同步策略由真正共享资源的上层能力负责，例如未来 Software I2C transaction ownership，而不是 Generic GPIO Impl 预先加锁。

---

# 22. 完成门槛

GPIO STM32 Impl Phase 1 完成必须满足：

```text
GPIO STM32 Impl production files      IMPLEMENTED
Generic caller-owned Context           VERIFIED
Single physical pin validation         VERIFIED
Platform -> HAL mapping                VERIFIED
Initial level before output init       VERIFIED
GPIO Impl Host Tests                   PASS
Platform GPIO Regression               PASS
Dependency Boundary                    PASS
Coding Standard Review                 PASS
Keil Build                             PASS
```

最终阶段状态：

```text
GPIO STM32 Impl Phase 1 = IMPLEMENTED / HOST + KEIL VERIFIED
Target Board GPIO        = NOT YET VERIFIED
```

不得在 Phase 1 完成报告中声称 Target Board GPIO 已验证。

---

# 23. Phase 2 交接边界

Phase 1 完成后进入：

```text
Phase 2 — Board Resource + CubeMX Configuration
```

Phase 2 至少负责：

```text
读取 / 更新板级硬件资源表
冻结 LED / KEY / Soft-I2C SCL / SDA Port + Pin
CubeMX GPIO Port RCC 基础配置
Board / BSP Context binding
Keil Build
真实 GPIO Output Smoke Test
真实 GPIO Input Smoke Test
```

目前已知可作为后续候选板测资源：

```text
板载 LED : PC13，低电平有效
板载 KEY : PA0，低电平有效
```

但这些具体资源不得写入 Phase 1 Generic GPIO Impl。

---

# 24. 核心冻结结论

```text
GPIO STM32 Impl = Generic MCU Implementation
Context         = Caller-owned port + one physical pin
Context Pool    = NO
Registry        = NO
Reverse pointer = NO
Dynamic memory  = NO
RCC ownership   = CubeMX / Board Bootstrap
GPIO Speed      = Impl-private LOW
Output configure= Write initial level before HAL_GPIO_Init
Board binding   = Phase 2
Target board    = Phase 2
```

未经新的专项设计评审，不得在实施阶段扩大上述公共边界。