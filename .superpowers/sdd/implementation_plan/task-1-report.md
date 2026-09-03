# Task 1 Report: Platform Button Contract and Static Configuration

## Status

IMPLEMENTED / HOST VERIFIED.  本任务只实现 Platform Button 和产品级静态
配置；未实现 BSP Button、Button Service、目标板 smoke 或 APP 控制逻辑。

## Modified files

- `RTT_elog_DMA_UART_ring_project/03_Platform/platform_bsp/button/platform_button.h`
  - 新增 caller-owned、零初始化的 `platform_button_t`，以及
    `RELEASED/PRESSED` 逻辑状态和 init/read/deinit 公共接口。
- `RTT_elog_DMA_UART_ring_project/03_Platform/platform_bsp/button/platform_button.c`
  - 通过 Platform GPIO 配置 INPUT 和 Board 提供的 pull；将物理电平映射为
    `PRESSED/RELEASED`，并传播 GPIO 错误。
- `RTT_elog_DMA_UART_ring_project/00_Config/project_config.h`
  - 新增固定 User Key 极性/上下拉与 10/30/300/3000 ms Button 宏。
- `RTT_elog_DMA_UART_ring_project/Tests/platform_button/test_platform_button.c`
  - 新增 Host fake-GPIO 契约测试。
- `RTT_elog_DMA_UART_ring_project/Tests/project_config/test_project_config.c`
  - 为六个新增产品级宏增加编译期断言。

## TDD RED

先只写 `Tests/platform_button/test_platform_button.c`，未创建 Button 生产代码。

```powershell
gcc -std=c11 -Wall -Wextra -Werror \
  -I RTT_elog_DMA_UART_ring_project/03_Platform/platform_bsp \
  -I RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/gpio \
  -I RTT_elog_DMA_UART_ring_project/03_Platform/platform_common \
  -I RTT_elog_DMA_UART_ring_project/04_Impl/impl_board \
  RTT_elog_DMA_UART_ring_project/Tests/platform_button/test_platform_button.c \
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/gpio/platform_gpio.c \
  -o RTT_elog_DMA_UART_ring_project/Tests/platform_button/test_platform_button.exe
```

输出（exit 1，符合预期）：

```text
fatal error: button/platform_button.h: No such file or directory
```

## GREEN

新增最小 Platform Button 实现后运行：

```powershell
gcc -std=c11 -Wall -Wextra -Werror \
  -I RTT_elog_DMA_UART_ring_project/03_Platform/platform_bsp \
  -I RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/gpio \
  -I RTT_elog_DMA_UART_ring_project/03_Platform/platform_common \
  -I RTT_elog_DMA_UART_ring_project/04_Impl/impl_board \
  RTT_elog_DMA_UART_ring_project/Tests/platform_button/test_platform_button.c \
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_bsp/button/platform_button.c \
  RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/gpio/platform_gpio.c \
  -o RTT_elog_DMA_UART_ring_project/Tests/platform_button/test_platform_button.exe
& RTT_elog_DMA_UART_ring_project/Tests/platform_button/test_platform_button.exe
```

输出：exit 0，无 warning、无 test failure。

覆盖：NULL init/read/deinit、NULL state、未绑定对象、非法 activeLevel/pull、
INPUT + pull 配置、active-low/active-high 映射、未 init read、configure/read/
deinit 错误传播，以及成功 deinit 后保留 GPIO binding。

## Focused regression

以下命令均使用 `-std=c11 -Wall -Wextra -Werror`，并包含
`-I RTT_elog_DMA_UART_ring_project/04_Impl/impl_board`：

```text
test_platform_button.c + platform_button.c + platform_gpio.c  -> PASS platform_button
test_platform_gpio.c + platform_gpio.c                        -> PASS platform_gpio
test_platform_led.c + platform_led.c + platform_gpio.c        -> PASS platform_led
test_project_config.c                                         -> PASS project_config
```

最终聚合运行输出（exit 0）：

```text
PASS platform_button
PASS platform_gpio
PASS platform_led
PASS project_config
```

## Coding-standard self-review

- Platform Button 仅依赖 Platform GPIO；没有 HAL、FreeRTOS、printf 或动态内存。
- 未新增 `impl_button`、`platform_device_t`、注册器、BSP Button 或 Service。
- 所有公共接口检查 NULL 和生命周期；init 检查 activeLevel/pull 枚举，错误原样传播。
- Button deinit 仅清除 Button 的 initialized；GPIO binding 保持有效，可再次 init。
- 新增头文件具备文件头、独立 include、header guard；实现使用 early return 和
  静态私有校验函数。
- 已执行 `git diff --check`；新增/修改文件无超过 120 列的代码行，未发现 Tab。

## Concerns

- Task 1 的 Host 范围已验证；Keil full rebuild 和真实板验证属于后续集成/板测，
  本任务未运行、也不作通过声明。
