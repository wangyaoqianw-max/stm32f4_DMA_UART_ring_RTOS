# Task 2 实施报告：User Key Button BSP Composition

## 修改内容

- 新增 `03_Platform/platform_bsp/button/platform_bsp_button.h`，声明 `platform_bsp_button_construct_user_key()`。
- 新增 `03_Platform/platform_bsp/button/platform_bsp_button.c`：检查 NULL，调用 `platform_bsp_gpio_construct_user_key(&button->gpio)`，成功后设置 `PROJECT_USER_KEY_ACTIVE_LEVEL` 和 `PROJECT_USER_KEY_PULL`，不调用 `platform_button_init()`。
- 新增 `Tests/platform_bsp_button/test_platform_bsp_button.c`，覆盖 NULL、单次 GPIO 构造调用、绑定但未配置状态、配置值和错误传播。

## TDD RED

命令：

```text
gcc -std=c11 -Wall -Wextra -Werror -I 03_Platform/platform_bsp -I 03_Platform/platform_mcu/gpio -I 03_Platform/platform_mcu/uart -I 03_Platform/platform_common -I 03_Platform/platform_os -I 00_Config -I 04_Impl/impl_board Tests/platform_bsp_button/test_platform_bsp_button.c 03_Platform/platform_bsp/button/platform_bsp_button.c -o %TEMP%\\test_platform_bsp_button.exe
```

预期失败已观察：`fatal error: button/platform_bsp_button.h: No such file or directory`（实现头文件尚不存在）。

## TDD GREEN

命令：同上，并链接 `03_Platform/platform_bsp/button/platform_button.c`。

结果：进程 `exit=0`，无 warning/error。

## 回归

- Platform Button：`-std=c11 -Wall -Wextra -Werror` Host 编译并运行，`platform_button exit=0`。
- Platform BSP Button：编译并运行通过，`exit=0`。
- Platform BSP GPIO：尝试按既有测试编译；基线 `impl_platform_gpio.c` 在测试 HAL 头缺少 `GPIO_MODE_INPUT`、`GPIO_MODE_OUTPUT_PP`、`GPIO_MODE_OUTPUT_OD`、`GPIO_NOPULL`、`GPIO_PULLUP`、`GPIO_PULLDOWN`、`GPIO_SPEED_FREQ_LOW` 及 `HAL_GPIO_*` 声明，因 `-Werror` 失败，未涉及本次改动。

## 规范自审

- 文件职责、命名、Header Guard、公共 API 文档和 include 层次符合规范。
- Button BSP 头/源不包含 HAL，不定义 PA0、GPIOA、GPIO_PIN_0，也未暴露硬件宏。
- 严格复用 `platform_bsp_gpio_construct_user_key(&button->gpio)`；成功后仅保留 GPIO 已绑定、未配置及 Button 未初始化状态。
- 错误采用 `platform_error_t`，NULL 和 GPIO 构造错误均正确返回。

## 疑虑

Platform BSP GPIO 回归受现有 HAL 测试桩缺失符号阻塞；建议后续由对应任务补齐测试桩定义后重跑。
