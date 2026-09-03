# STM32F4 DMA UART Ring RTOS

当前阶段：`Phase 4 — IMPLEMENTED / TARGET BOARD VERIFICATION PENDING`

| 项目 | 当前状态 |
| --- | --- |
| LED 架构 | `Indicator Service -> Platform LED -> Platform GPIO -> STM32 GPIO Impl` |
| LED 源码 | `03_Platform/platform_bsp/led/`，服务位于 `02_Service/service_indicator/` |
| Host 回归 | Platform LED、BSP LED、Indicator Service、Platform GPIO、BSP GPIO：PASS |
| Keil 构建 | 正常路径 Full Rebuild：0 errors；Phase 4 三源无警告 |
| 目标板 LED | OFF / ON / 3 次闪烁 / 最终 OFF：PASS |
| RTT | `start -> STOPPED -> RUNNING -> STOPPED -> ONCE_SUCCESS -> pass`：PASS |
| UART 串口回归 | PENDING：尚无独立 PC Serial Assistant 证据 |

临时 FreeRTOS smoke 已从正常固件、`freertos.c` 和 Keil 工程移除。剩余人工验证仅需使用既有 PC Serial Assistant 流程确认 UART 通信未受影响；不得为此新增 smoke 专用协议。
