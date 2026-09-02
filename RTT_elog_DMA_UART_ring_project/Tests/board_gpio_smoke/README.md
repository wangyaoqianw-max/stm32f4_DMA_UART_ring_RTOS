# Board GPIO Smoke Test

## 目的

验证以下完整纵向链路：

```text
Board BSP -> Platform GPIO -> STM32 GPIO Impl -> HAL -> 目标板引脚
```

本 Smoke Test 只验证 GPIO，不实现 Software I2C 的 START、STOP、地址、ACK/NACK，也不访问 DHT20 或 MPU6050 寄存器。

当前状态：

```text
Host Test                         PASS
Keil Full Rebuild                PASS (0 errors)
Target Board GPIO Smoke Test     PASS (serial assistant + RTT + logic analyzer verified)
```

## 观察工具

目标板测试必须同时打开两个观察通道：

1. 串口助手连接 USART1：115200 baud、8 data bits、无校验、1 stop bit、无流控。
2. 调试器连接 RTT Viewer / RTT 日志窗口。

串口助手观察结构化阶段标记，例如 `GPIO_SMOKE,LED,level=LOW,expected=ON`；
RTT 观察同一阶段的 `gpio smoke ...` 日志、初始化错误和 `FAIL` 日志。两边应按相同测试顺序记录，任一通道缺失或结果矛盾时不得判 PASS。

## 接线与前置条件

| 资源 | 引脚 | 电气配置 | 预期板级语义 |
| --- | --- | --- | --- |
| Status LED | PC13 | Push-Pull / No Pull / initial HIGH | LOW 点亮，HIGH 熄灭 |
| User Key | PA0 | Input / Pull-Up / no EXTI | 释放 HIGH，按下 LOW |
| Soft I2C SCL | PB6 | Open-Drain / No Pull / initial HIGH | LOW 主动拉低，HIGH 释放 |
| Soft I2C SDA | PB7 | Open-Drain / No Pull / initial HIGH | LOW 主动拉低，HIGH 释放 |

PB6/PB7 的 HIGH 不是 Push-Pull 输出高电平，而是开漏释放后由外部上拉形成高电平。板测前必须确认：

- DHT20 或 MPU6050 模块的 I2C 外部上拉有效，或临时接入明确的 3.3 V 上拉；
- MCU、传感器和上拉电源共地；
- 上拉电压与 MCU 3.3 V 电平兼容；
- 不为了测试把 PB6/PB7 改成内部 Pull-Up；
- 硬件 I2C 保持关闭，PA0 保持无 EXTI。

## 临时目标入口

`board_gpio_smoke.c` 和 `board_gpio_smoke.h` 只作为目标板测试代码，当前不加入正常产品启动路径，也不加入已提交的产品 Keil 工程组。

需要进行实板测试时，临时执行以下操作：

1. 将 `Tests/board_gpio_smoke/board_gpio_smoke.c` 加入临时 Keil 测试分组，并临时增加 `Tests/board_gpio_smoke` include path。
2. 在 `Core/Src/main.c` 的现有 `USER CODE` 区包含 `board_gpio_smoke.h`。
3. 在 `MX_FREERTOS_Init()` 返回之后、`osKernelStart()` 之前的 `USER CODE` 区调用：

```c
board_gpio_smoke_run();
```

此位置保证 HAL、GPIO、USART1、Service Log 和 USART1 Mutex 已完成初始化；Harness 使用 `HAL_Delay()` 留出人工观察时间，不能挪到调度器启动前的更早位置，也不能放入 ISR。

## 测试顺序

Harness 必须通过以下 BSP 接口构造对象，不得直接写 `GPIO_TypeDef` 或 `GPIO_PIN_x`：

```c
platform_bsp_gpio_construct_status_led(&statusLedGpio);
platform_bsp_gpio_construct_user_key(&userKeyGpio);
platform_bsp_gpio_construct_soft_i2c_scl(&softI2cSclGpio);
platform_bsp_gpio_construct_soft_i2c_sda(&softI2cSdaGpio);
```

之后通过 `platform_gpio_configure()`、`platform_gpio_write()` 和 `platform_gpio_read()` 完成以下步骤。
每一步都检查返回值；失败时串口助手输出 `GPIO_SMOKE,FAIL,...`，RTT 输出 ERROR，并停止后续相关操作。

| 顺序 | 操作 | 串口助手预期 | RTT 预期 | 人工判定 |
| --- | --- | --- | --- | --- |
| 1 | 构造并配置四个 GPIO | `GPIO_SMOKE,CONFIGURED` | `gpio smoke configured` | 配置成功 |
| 2 | PC13 写 LOW | `expected=ON` | `LED low, expected on` | LED 点亮 |
| 3 | PC13 写 HIGH | `expected=OFF` | `LED high, expected off` | LED 熄灭 |
| 4 | PA0 释放后读取 | `expected=HIGH` 且 `read=HIGH` | release read HIGH | 按键释放为高 |
| 5 | PA0 按下后读取 | `expected=LOW` 且 `read=LOW` | press read LOW | 按键按下为低 |
| 6 | PB6 写 LOW | `expected=BUS_LOW` | SCL low | 万用表/逻辑分析仪约 0 V |
| 7 | PB6 写 HIGH/RELEASE | `expected=EXTERNAL_PULLUP_HIGH` | SCL release | 外部上拉为高 |
| 8 | PB7 写 LOW 后读取 | `expected=LOW` 且 `read=LOW` | SDA low read LOW | 拉低与回读正确 |
| 9 | PB7 写 HIGH/RELEASE 后读取 | `expected=HIGH` 且 `read=HIGH` | SDA release read HIGH | 释放与回读正确 |
| 10 | 保持 GPIO 配置并结束 | `GPIO_SMOKE,END,...` | `gpio smoke end...` | 记录结果 |

Harness 中 PA0 会先提示 `action=RELEASE`，再提示 `action=PRESS`；操作者应在对应等待窗口内操作按键。PB6/PB7 的电压必须以仪表或逻辑分析仪为准，不能只依据软件日志判定开漏物理行为。

## 结果记录规则

真实人工观察完成前，以下项目全部保持 `PENDING`：

```text
PC13 Status LED                       PENDING
PA0 User Key                          PENDING
PB6 Open-Drain Pull-Low / Release     PENDING
PB7 Open-Drain Pull-Low / Release     PENDING
PB7 Physical Readback                 PENDING
```

板测完成后，将串口助手日志、RTT 日志和仪表/逻辑分析仪观察结果逐项对照记录。若任何项目失败，记录失败现象和对应阶段，不得用 Host Test 或 Keil 编译结果替代实板 PASS。

## 测试结束后的恢复

实板观察完成或中止后，必须：

1. 从 `Core/Src/main.c` 删除临时 include 和 `board_gpio_smoke_run()` 调用；
2. 从 Keil 工程移除临时 Smoke Test 源文件和 include path；
3. 重新执行正常固件 Full Rebuild；
4. 确认正常启动路径没有开机自动翻转 LED、SCL 或 SDA。

Smoke Harness 不调用 `platform_gpio_deinit()`：测试入口位于 `osKernelStart()` 前，结束时必须保持四个 GPIO 的有效配置，避免破坏后续正常启动路径。

在目标板真实验证完成前，Phase 2 状态只能是：

```text
IMPLEMENTED / TARGET BOARD VERIFICATION PENDING
```

验证完成后仍须停止在 Phase 2，不得从本文件直接开始 Phase 3 Software I2C 实现。
