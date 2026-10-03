# CubeMX 引脚配置文档

## 1. 文档信息

| 项目 | 配置 |
| --- | --- |
| 工程 | `RTT_elog_DMA_UART_ring_project` |
| MCU | `STM32F411CEU6` |
| CubeMX 型号 | `STM32F411C(C-E)Ux` |
| 封装 | `UFQFPN48` |
| STM32CubeMX | `6.8.1` |
| 固件包 | `STM32Cube FW_F4 V1.27.1` |
| 配置源文件 | `RTT_elog_DMA_UART_ring_project.ioc` |
| 生成代码 | `Core/Src/gpio.c`、`Core/Src/usart.c`、`Core/Src/spi.c` |

本文档记录当前工程中由 CubeMX 配置并在生成代码中初始化的 MCU 引脚。表格中的引脚名称是 STM32 GPIO 命名，不是 UFQFPN48 封装的物理焊盘编号。

## 2. 业务 GPIO 与外设引脚分配

| MCU 引脚 | CubeMX 信号 | 工程标签 | GPIO/外设模式 | 上下拉 | 速度 | 初始输出电平 | 复用功能或说明 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PA0-WKUP | `GPIO_Input` | `KEY_IN` | GPIO 输入 | 上拉 `GPIO_PULLUP` | 不适用 | 不适用 | 按键输入；同时具有 WKUP 引脚属性 |
| PA1 | `GPIO_Output` | `LCD_BL` | 推挽输出 | 无 `GPIO_NOPULL` | 低速 | 低电平 `GPIO_PIN_RESET` | LCD 背光控制 |
| PA4 | `GPIO_Output` | `LCD_CS` | 推挽输出 | 无 `GPIO_NOPULL` | 高速 | 高电平 `GPIO_PIN_SET` | LCD 片选 |
| PA5 | `SPI1_SCK` | — | 复用推挽输出 | 无 `GPIO_NOPULL` | very high | 外设控制 | `GPIO_AF5_SPI1`，LCD SPI 时钟 |
| PA6 | `GPIO_Output` | `LCD_DC` | 推挽输出 | 无 `GPIO_NOPULL` | 高速 | 高电平 `GPIO_PIN_SET` | LCD 数据/命令控制 |
| PA7 | `SPI1_MOSI` | — | 复用推挽输出 | 无 `GPIO_NOPULL` | very high | 外设控制 | `GPIO_AF5_SPI1`，LCD SPI 数据输出 |
| PA9 | `USART1_TX` | — | 复用推挽输出 | 无 `GPIO_NOPULL` | very high | 外设控制 | `GPIO_AF7_USART1`，串口发送 |
| PA10 | `USART1_RX` | — | 复用推挽输入 | 无 `GPIO_NOPULL` | very high | 外设控制 | `GPIO_AF7_USART1`，串口接收 |
| PB6 | `GPIO_Output` | `I2C_SCL` | 开漏输出 `GPIO_MODE_OUTPUT_OD` | 无 `GPIO_NOPULL` | 低速 | 高电平 `GPIO_PIN_SET` | 软件 I2C 时钟线；未配置为 `I2C1_SCL` 硬件复用 |
| PB7 | `GPIO_Output` | `I2C_SDA` | 开漏输出 `GPIO_MODE_OUTPUT_OD` | 无 `GPIO_NOPULL` | 低速 | 高电平 `GPIO_PIN_SET` | 软件 I2C 数据线；未配置为 `I2C1_SDA` 硬件复用 |
| PB10 | `GPIO_Output` | `LCD_RST` | 推挽输出 | 无 `GPIO_NOPULL` | 低速 | 高电平 `GPIO_PIN_SET` | LCD 复位 |
| PC13-ANTI_TAMP | `GPIO_Output` | `LED_OUT` | 推挽输出 | 无 `GPIO_NOPULL` | 低速 | 高电平 `GPIO_PIN_SET` | LED 控制；实际亮灭电平取决于硬件连接 |

## 3. 系统、调试及时钟引脚

| MCU 引脚 | CubeMX 信号 | 配置模式 | 用途 |
| --- | --- | --- | --- |
| PA13 | `SYS_JTMS-SWDIO` | Serial Wire | SWDIO 调试数据线 |
| PA14 | `SYS_JTCK-SWCLK` | Serial Wire | SWCLK 调试时钟线 |
| PC14-OSC32_IN | `RCC_OSC32_IN` | LSE 外部振荡器 | 低速外部时钟输入 |
| PC15-OSC32_OUT | `RCC_OSC32_OUT` | LSE 外部振荡器 | 低速外部时钟输出 |
| PH0 - OSC_IN | `RCC_OSC_IN` | HSE 外部振荡器 | 高速外部时钟输入 |
| PH1 - OSC_OUT | `RCC_OSC_OUT` | HSE 外部振荡器 | 高速外部时钟输出 |

## 4. 外设相关配置

### 4.1 SPI1

| 配置项 | 值 |
| --- | --- |
| 工作模式 | Master |
| 数据方向 | `SPI_DIRECTION_2LINES`（生成代码配置） |
| 数据位宽 | 8 bit |
| 时钟极性 | `SPI_POLARITY_HIGH` |
| 时钟相位 | `SPI_PHASE_2EDGE` |
| 波特率分频 | `SPI_BAUDRATEPRESCALER_8` |
| 计算速率 | 12.5 Mbit/s |
| NSS | 软件管理 |
| SPI 引脚 | PA5=`SCK`，PA7=`MOSI` |
| LCD 控制 GPIO | PA4=`CS`，PA6=`DC`，PB10=`RST`，PA1=`BL` |

当前 CubeMX 引脚分配只使用 SPI1 的 SCK 和 MOSI，未为 SPI1 分配 MISO 引脚；PA6 被分配给 LCD_DC。若后续需要 SPI 读回功能，应重新检查 SPI 数据方向和引脚分配。

### 4.2 USART1 与 DMA

| 配置项 | 值 |
| --- | --- |
| USART 模式 | Asynchronous |
| TX 引脚 | PA9，`GPIO_AF7_USART1` |
| RX 引脚 | PA10，`GPIO_AF7_USART1` |
| 波特率 | 115200 |
| 数据格式 | 8 data bits，1 stop bit，无校验 |
| 硬件流控 | None |
| RX DMA | DMA2 Stream2，Peripheral-to-Memory，Circular，Medium priority |
| TX DMA | DMA2 Stream7，Memory-to-Peripheral，Normal，Low priority |
| DMA 通道 | RX/TX 均为 Channel 4 |
| USART1 中断 | 已启用，抢占优先级 5，子优先级 0 |

### 4.3 I2C 相关引脚

PB6 和 PB7 在 CubeMX 中分别标记为 `I2C_SCL` 和 `I2C_SDA`，但实际配置为普通 GPIO 开漏输出：

- PB6：`GPIO_MODE_OUTPUT_OD`，低速，无上下拉，默认输出高电平。
- PB7：`GPIO_MODE_OUTPUT_OD`，低速，无上下拉，默认输出高电平。
- `.ioc` 中没有 `I2C1` 外设配置，也没有硬件 I2C 复用功能。

因此当前配置应按软件 I2C/手动时序 GPIO 使用。外部 I2C 器件所需的上拉电阻需要由硬件提供，或在后续设计中重新配置为硬件 I2C。

## 5. GPIO 初始输出汇总

| 工程标签 | 引脚 | CubeMX/生成代码初始电平 | 备注 |
| --- | --- | --- | --- |
| `LED_OUT` | PC13 | `GPIO_PIN_SET` | 亮灭逻辑由板级电路决定 |
| `LCD_BL` | PA1 | `GPIO_PIN_RESET` | 背光默认关闭 |
| `LCD_CS` | PA4 | `GPIO_PIN_SET` | 片选默认无效 |
| `LCD_DC` | PA6 | `GPIO_PIN_SET` | 数据/命令控制线默认置高 |
| `LCD_RST` | PB10 | `GPIO_PIN_SET` | 复位线默认释放 |
| `I2C_SCL` | PB6 | `GPIO_PIN_SET` | 开漏线默认释放 |
| `I2C_SDA` | PB7 | `GPIO_PIN_SET` | 开漏线默认释放 |

## 6. 配置来源与维护说明

本文档依据以下工程文件整理：

- `RTT_elog_DMA_UART_ring_project.ioc`
- `Core/Inc/main.h`
- `Core/Src/gpio.c`
- `Core/Src/usart.c`
- `Core/Src/spi.c`

当 CubeMX 重新生成代码后，应同步复核本文件，重点检查：

1. `.ioc` 中的 `PA*`、`PB*`、`PC*`、`PH*` 配置是否发生变化。
2. `Core/Src/gpio.c` 中 GPIO 初始电平、模式、上下拉和速度是否变化。
3. `Core/Src/usart.c`、`Core/Src/spi.c` 中的复用功能、DMA 流和外设参数是否变化。
