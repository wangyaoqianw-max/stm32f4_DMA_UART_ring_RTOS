# Platform GPIO Phase 1 设计

状态：FROZEN  
日期：2026-08-31

> 本文冻结普通数字 GPIO 的 Platform Phase 1 抽象合同。
> 本阶段只设计跨 MCU 的 GPIO 基础能力，不实现 STM32 HAL 适配，不配置具体板级引脚，不引入 EXTI / LED / KEY 业务语义。

---

# 1. 设计目标

建立一个可复用、可 Host Test、与 STM32 HAL 解耦的 GPIO Platform 抽象，使上层能够表达：

```text
GPIO object construction
GPIO configuration
GPIO read
GPIO write
GPIO deconfiguration
```

本阶段完成后的依赖链停在：

```text
APP / Service / BSP
        ↓
Platform GPIO
        ↓
GPIO Ops
        ↓
[Impl not implemented in this phase]
```

本阶段不以“点亮真实 LED”为完成条件；真实 STM32 GPIO 能力留给后续 `GPIO STM32 Impl Phase 1`。

---

# 2. 模块位置与职责

目标目录：

```text
03_Platform/platform_mcu/gpio/
├── platform_gpio_types.h
├── platform_gpio.h
└── platform_gpio.c
```

职责：

- `platform_gpio_types.h`：定义跨平台 GPIO 枚举和静态配置类型；
- `platform_gpio.h`：定义 GPIO 轻量对象、Ops 和公共 API；
- `platform_gpio.c`：实现参数校验、状态约束、Ops 转发和状态更新。

公共 Platform Header 不得包含 STM32 HAL 类型、寄存器类型、具体 Port / Pin 编号或 Vendor Handle。

---

# 3. GPIO 是 Resource，不是完整 Device

Phase 1 中 `platform_gpio_t` 不继承 `platform_device_t`，不使用 `platform_lifecycle_t`。

原因：

1. UART 等复杂外设存在自然的初始化、启动、停止、错误恢复和异步传输生命周期；
2. 单个 GPIO Pin 更接近 MCU 基础资源；
3. 为 GPIO 强行引入 `STARTED / STOPPED` 会产生没有实际硬件意义的状态；
4. GPIO 当前只需要区分“Platform 对象是否已构造”和“硬件配置是否已应用”。

因此采用轻量对象模型，同时保留本工程已经验证有效的：

```text
static config
runtime binding
Ops injection
implementation context
explicit state
```

这不是废弃 `platform_device_t`，而是区分：

```text
Device   -> UART / Display / Storage 等具备完整设备语义的对象
Resource -> GPIO Pin 等基础 MCU 资源
```

---

# 4. Platform 与 BSP / Board 的语义边界

Platform GPIO 只表达电气 / 逻辑 GPIO 语义：

```text
HIGH / LOW
INPUT / OUTPUT
PULL_NONE / PULL_UP / PULL_DOWN
PUSH_PULL / OPEN_DRAIN
```

Platform GPIO 不得表达：

```text
LED_ON / LED_OFF
KEY_PRESSED / KEY_RELEASED
LCD_RESET_ASSERT
SPI_CS_ACTIVE
```

板级极性和逻辑设备角色属于 Board / BSP。

例如低电平点亮 LED：

```text
Platform BSP LED_ON
        ↓
Board polarity mapping
        ↓
PLATFORM_GPIO_LEVEL_LOW
        ↓
Platform GPIO
```

因此更换为高电平点亮 LED 时，只改变 Board / BSP 绑定，不改变 App 语义和 GPIO Platform API。

---

# 5. 公共数据类型

## 5.1 前置声明

```c
typedef struct platform_gpio platform_gpio_t;
```

## 5.2 逻辑电平

```c
typedef enum
{
    PLATFORM_GPIO_LEVEL_LOW = 0,
    PLATFORM_GPIO_LEVEL_HIGH,
    PLATFORM_GPIO_LEVEL_MAX
} platform_gpio_level_t;
```

## 5.3 GPIO 方向

```c
typedef enum
{
    PLATFORM_GPIO_DIRECTION_INPUT = 0,
    PLATFORM_GPIO_DIRECTION_OUTPUT,
    PLATFORM_GPIO_DIRECTION_MAX
} platform_gpio_direction_t;
```

## 5.4 上下拉

```c
typedef enum
{
    PLATFORM_GPIO_PULL_NONE = 0,
    PLATFORM_GPIO_PULL_UP,
    PLATFORM_GPIO_PULL_DOWN,
    PLATFORM_GPIO_PULL_MAX
} platform_gpio_pull_t;
```

## 5.5 输出类型

```c
typedef enum
{
    PLATFORM_GPIO_OUTPUT_PUSH_PULL = 0,
    PLATFORM_GPIO_OUTPUT_OPEN_DRAIN,
    PLATFORM_GPIO_OUTPUT_MAX
} platform_gpio_output_type_t;
```

## 5.6 静态配置

```c
typedef struct
{
    platform_gpio_direction_t direction;
    platform_gpio_pull_t pull;
    platform_gpio_output_type_t outputType;
    platform_gpio_level_t initialLevel;
} platform_gpio_config_t;
```

字段合同：

```text
direction    INPUT / OUTPUT 均有效
pull         INPUT / OUTPUT 均有效
outputType   仅 OUTPUT 有业务意义
initialLevel 仅 OUTPUT 有业务意义
```

INPUT 模式下 `outputType` 和 `initialLevel` 不参与实际 GPIO 配置语义，但仍必须是合法枚举值。Phase 1 不增加额外的 `NOT_APPLICABLE` 值，避免扩大状态空间。

---

# 6. Phase 1 不抽象 GPIO Speed

Phase 1 不公开：

```text
LOW / MEDIUM / HIGH / VERY_HIGH speed
```

当前跨 MCU 的真实需求仅包括方向、上下拉、输出结构和逻辑电平。GPIO Speed 更接近具体 MCU 的 IO Driver 特性。

设计原则：

> 不为了映射 STM32 HAL 字段而反向塑造 Platform API。

如果后续真实 SPI CS、LCD、时序或 EMI 需求证明 Speed 必须成为跨平台合同，再通过专项设计扩展。

---

# 7. GPIO 轻量对象模型

## 7.1 Ops

```c
typedef struct
{
    platform_error_t (*configure)(platform_gpio_t *gpio,
                                  const platform_gpio_config_t *config);
    platform_error_t (*write)(platform_gpio_t *gpio,
                              platform_gpio_level_t level);
    platform_error_t (*read)(platform_gpio_t *gpio,
                             platform_gpio_level_t *level);
    platform_error_t (*deinit)(platform_gpio_t *gpio);
} platform_gpio_ops_t;
```

Ops 由后续 Impl 注入。Platform 层不得知道 Ops 内部如何访问硬件。

## 7.2 GPIO 对象

```c
struct platform_gpio
{
    const char *name;
    platform_gpio_config_t config;
    const platform_gpio_ops_t *ops;
    void *implContext;
    platform_bool_t initialized;
    platform_bool_t configured;
};
```

对象初始化宏：

```c
#define PLATFORM_GPIO_INITIALIZER {0}
```

`name` 只用于对象识别 / 调试语义，可为 `NULL`，不参与硬件行为判断。

Platform 不缓存“当前实际电平”作为独立真值；实际电平由硬件 / Impl 决定。

---

# 8. 状态模型

GPIO 不使用完整 Lifecycle，只使用：

```text
initialized
configured
```

状态流：

```text
PLATFORM_GPIO_INITIALIZER
        ↓
initialized = false
configured  = false
        ↓ platform_gpio_init()
initialized = true
configured  = false
        ↓ platform_gpio_configure()
initialized = true
configured  = true
        ↓ platform_gpio_deinit()
initialized = true
configured  = false
```

语义：

```text
initialized
= Platform GPIO 对象已经构造，Ops / implContext 已绑定

configured
= GPIO 配置已经由 Impl 成功应用到硬件
```

因此：

```text
platform_gpio_init()      != hardware configuration
platform_gpio_configure() == request hardware configuration
platform_gpio_deinit()    == deconfigure hardware, not destroy object
```

Phase 1 不提供 `platform_gpio_destroy()`；对象和上下文均采用 caller-owned / static 生命周期。

---

# 9. 构造参数

```c
typedef struct
{
    const char *name;
    const platform_gpio_ops_t *ops;
    void *implContext;
} platform_gpio_init_params_t;
```

Platform 只保存 opaque `implContext`。

后续 STM32 Impl 可以在自己的私有类型中保存 `GPIO_TypeDef *`、Pin mask 等信息，但任何 STM32 类型不得进入 Platform 公共 Header。

---

# 10. 公共 API

Phase 1 冻结以下接口：

```c
platform_error_t platform_gpio_init(
    platform_gpio_t *gpio,
    const platform_gpio_init_params_t *params);

platform_error_t platform_gpio_configure(
    platform_gpio_t *gpio,
    const platform_gpio_config_t *config);

platform_error_t platform_gpio_write(
    platform_gpio_t *gpio,
    platform_gpio_level_t level);

platform_error_t platform_gpio_read(
    platform_gpio_t *gpio,
    platform_gpio_level_t *level);

platform_error_t platform_gpio_deinit(
    platform_gpio_t *gpio);
```

本阶段不提供 Toggle、IRQ、EXTI、AF、Analog、Group / Port 批量 API。

---

# 11. `platform_gpio_init()` 合同

职责：

```text
validate object storage
validate init params
reject duplicate construction
bind name
bind ops
bind implContext
set initialized = true
set configured = false
```

不得调用 HAL、寄存器或真正配置 Pin。

错误合同：

```text
gpio == NULL        -> PLATFORM_ERR_NULL_POINTER
params == NULL      -> PLATFORM_ERR_NULL_POINTER
params->ops == NULL -> PLATFORM_ERR_INVALID_PARAM
already initialized -> PLATFORM_ERR_ALREADY_INITIALIZED
```

单个 Ops 函数可以为 `NULL`；对应公共 API 调用时返回 `PLATFORM_ERR_NOT_SUPPORTED`。

---

# 12. `platform_gpio_configure()` 合同

调用前要求：

```text
initialized == true
config != NULL
all enum values valid
ops->configure != NULL
```

Platform 执行：

```text
validate
    ↓
ops->configure(gpio, config)
    ↓ success only
copy config into gpio->config
configured = true
```

如果 Impl 返回失败：

```text
configured 状态不得被错误地置为 true
不得把失败配置覆盖为对象的已生效 config
Impl error 原样向上返回
```

已经配置的 GPIO 允许再次调用 `platform_gpio_configure()`，支持：

```text
INPUT -> OUTPUT
OUTPUT -> INPUT
OUTPUT config A -> OUTPUT config B
```

如果对象原本已经处于 `configured == true`，重新配置失败时必须保留旧的 `config` 和 `configured == true`，因为旧配置仍是 Platform 最后一次确认成功的配置。

---

# 13. 输出初始电平合同

当：

```text
direction == PLATFORM_GPIO_DIRECTION_OUTPUT
```

`initialLevel` 表示 GPIO 切换为输出时应建立的初始逻辑输出值。

后续 Impl 必须尽可能采用：

```text
prepare output latch / initial level
        ↓
configure pin as output
```

而不是先进入 Output 再设置初始电平。

目的：减少输出模式切换时的错误脉冲，尤其适用于：

```text
SPI CS
LCD RESET
Chip Enable
Power Enable
```

Platform 只冻结行为合同，不冻结 STM32 HAL 调用顺序；具体实现由 Impl Phase 设计决定。

---

# 14. `platform_gpio_write()` 合同

允许条件：

```text
initialized == true
configured == true
direction == PLATFORM_GPIO_DIRECTION_OUTPUT
level is valid
ops->write != NULL
```

错误合同：

```text
not initialized -> PLATFORM_ERR_NOT_INITIALIZED
not configured  -> PLATFORM_ERR_INVALID_STATE
current INPUT   -> PLATFORM_ERR_INVALID_STATE
invalid level   -> PLATFORM_ERR_INVALID_PARAM
write op NULL   -> PLATFORM_ERR_NOT_SUPPORTED
```

Impl 错误原样传播。

Platform 不在成功写入后维护额外的 `currentLevel` 字段，避免软件缓存成为硬件状态的第二真值。

---

# 15. `platform_gpio_read()` 合同

INPUT 和 OUTPUT 模式均允许读取。

调用前要求：

```text
initialized == true
configured == true
level != NULL
ops->read != NULL
```

错误合同：

```text
not initialized -> PLATFORM_ERR_NOT_INITIALIZED
not configured  -> PLATFORM_ERR_INVALID_STATE
level == NULL   -> PLATFORM_ERR_NULL_POINTER
read op NULL    -> PLATFORM_ERR_NOT_SUPPORTED
```

Platform 调用 `ops->read()` 获取实际逻辑值，Impl 错误原样传播。

---

# 16. `platform_gpio_deinit()` 合同

`deinit()` 表示“反配置硬件”，不表示销毁 Platform 对象。

调用前要求：

```text
initialized == true
configured == true
ops->deinit != NULL
```

成功后：

```text
initialized = true
configured  = false
```

因此同一对象之后可以再次：

```c
platform_gpio_configure(&gpio, &newConfig);
```

失败时：

```text
configured 保持 true
Impl error 原样返回
```

错误合同：

```text
not initialized -> PLATFORM_ERR_NOT_INITIALIZED
not configured  -> PLATFORM_ERR_INVALID_STATE
deinit op NULL   -> PLATFORM_ERR_NOT_SUPPORTED
```

---

# 17. 错误处理与日志

GPIO 不新增错误码，继续使用 `platform_error_t`。

主要使用：

```text
PLATFORM_ERR_OK
PLATFORM_ERR_NULL_POINTER
PLATFORM_ERR_INVALID_PARAM
PLATFORM_ERR_INVALID_STATE
PLATFORM_ERR_NOT_INITIALIZED
PLATFORM_ERR_ALREADY_INITIALIZED
PLATFORM_ERR_NOT_SUPPORTED
```

Impl 可以返回 `PLATFORM_ERR_IO` 等底层失败，Platform 必须原样传播。

Platform GPIO 不打印普通日志。

错误拥有足够上下文的上层根据既有日志合同决定是否记录，避免同一根因在 Impl / Platform / Service / APP 多层重复打印。

---

# 18. CubeMX 与 GPIO 配置所有权

Phase 1 不修改 CubeMX 具体 Pin 功能配置。

当前原则：

```text
CubeMX / Core
    -> MCU bootstrap
    -> GPIO Port RCC 等基础启动能力可暂时保留

Platform GPIO config
    -> 跨平台 GPIO 配置真值

Impl GPIO
    -> 把 Platform config 转换为 STM32 hardware configuration
```

因此 Phase 1 不在 CubeMX 中新增：

```text
PAx -> GPIO_Output
PCx -> GPIO_Input
```

GPIO Port Clock 最终由 CubeMX Bootstrap 还是 GPIO Impl 自管理，留到 STM32 Impl Phase 根据现有工程启动顺序专项决定；Platform Phase 不提前冻结。

---

# 19. 与未来 STM32 Impl 的边界

未来 STM32 Impl 预计需要私有 Context，例如：

```c
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} stm32_gpio_impl_context_t;
```

该示例只说明 Impl 需要保存“具体端口 + Pin”的能力，不属于本阶段公共 API，也不冻结具体类型名称。

未来 Impl 负责：

```text
Platform enum -> STM32 HAL enum mapping
HAL_GPIO_Init
HAL_GPIO_ReadPin
HAL_GPIO_WritePin
HAL_GPIO_DeInit
initialLevel glitch avoidance
GPIO hardware error mapping where applicable
```

Platform 代码和 Host Test 必须能够完全不包含这些 STM32 符号。

---

# 20. Explicit Non-Goals

Phase 1 明确禁止加入：

```text
EXTI / GPIO IRQ
NVIC
ISR callback routing
Button debounce
Long press / short press
LED API
KEY API
GPIO toggle
Alternate Function
Analog Mode
GPIO speed
STM32 GPIO Port / GPIO_PIN_x
GPIO_InitTypeDef
HAL_GPIO_xxx
CubeMX concrete pin configuration
GPIO Group / Port batch operations
RTOS Mutex / Semaphore
Thread-safety wrapper
Dynamic allocation
GPIO Registry
```

如果实施时发现必须加入上述内容才能完成当前 Platform Host Test，说明设计或实现发生越界，应 STOP / BLOCKED 并返回设计阶段。

---

# 21. Host Test 验收合同

新建：

```text
Tests/platform_gpio/
```

Host Test 至少覆盖：

```text
Public type/header isolation
Initializer zero state
init success
init NULL validation
repeated init rejection
configure enum validation
configure Ops forwarding
first configure failure keeps configured=false
successful reconfigure updates config
failed reconfigure preserves previous successful config
write before configure rejected
write while INPUT rejected
OUTPUT write forwarding
invalid write level rejected
read NULL output rejected
INPUT read forwarding
OUTPUT read forwarding
deinit forwarding
deinit failure preserves configured=true
successful deinit sets configured=false
configure again after deinit
missing individual op -> NOT_SUPPORTED
Impl errors propagate unchanged
```

Host Test 不得 include：

```text
stm32f4xx_hal.h
GPIO_TypeDef
GPIO_InitTypeDef
HAL_GPIO_xxx
```

---

# 22. 完成条件

GPIO Platform Phase 1 只有满足以下条件才可标记完成：

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

阶段完成状态：

```text
GPIO Platform Phase 1
    FROZEN + HOST VERIFIED
```

这不代表目标板 GPIO 已验证。

后续阶段：

```text
GPIO STM32 Impl Phase 1
    ↓
Board / BSP logical GPIO binding
    ↓
simple target-board GPIO verification
```

后续 Impl Phase 必须单独设计并建立新的 `implementation_plan.md`，不得在本阶段计划中提前施工。