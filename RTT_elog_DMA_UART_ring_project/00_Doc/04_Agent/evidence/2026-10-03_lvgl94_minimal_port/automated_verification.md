# LVGL 9.4 最小移植：自动化验证

日期：2026-10-03

分支：`main`

状态：首次烧录前自动化验证快照；后续结果见本目录实板与交付记录

本文件保留首次烧录前的自动化快照；后续实板结果与资源调整见 [实板与资源记录](board_verification.md)。

## 源码与配置

- 直接来源：`wangyaoqianw-max/Embedded_Engineering_Library/third_party/LVGL/v9.4`，提交 `8ae16732fb3be01e4fed0c5d8cdae78c1ad46cd0`。
- 本地 `lv_version.h` 为 `9.4.0`；`LICENCE.txt` 为 MIT 许可。与 LVGL 官方标签的逐文件一致性未验证。
- 复制完整 `src/` 1149 个文件，其中 459 个 `.c`、593 个 `.h`；MDK 工程编入 388 个 LVGL `.c`，路径均存在且无重复。未编入 LVGL 自带硬件驱动、demos、examples。
- 对 `src/` 和 6 个顶层源码/许可文件共 1155 个文件逐文件计算 SHA-256，与上述提交的本地检出内容比较，0 处不一致；工程副本的来源说明 `README.md` 单独更新。
- `lv_conf.h`：RGB565、`LV_OS_NONE`、24 KiB LVGL 内建池，临时页启用 Label、Button、Montserrat 14 和默认主题。
- 显示缓冲 9600 B；Display Task 栈配置 4096 B；FreeRTOS heap 配置 20480 B。两个 heap 分别核算，4096 B 任务栈从 FreeRTOS heap 分配。

## 自动化结果

| 项目 | 移植前本次基线 | 移植后本次结果 |
| --- | ---: | ---: |
| Host 用例 | 41/41 通过 | 42/42 通过 |
| Keil 完整 rebuild | 0 错误 / 13 告警 | 0 错误 / 341 告警 |
| Flash（Code + RO + RW） | 67584 B | 261852 B |
| 静态 RAM（RW + ZI） | 46432 B | 86272 B |

目标区域：Flash 512 KiB，SRAM 128 KiB。最终静态分配在链接边界内，剩余 Flash 262436 B、静态 RAM 44800 B；这些数字不代表运行时 heap 余量或栈高水位。

Host 新增端口用例检查 tick 回调、RGB565 单缓冲、局部 flush 的闭区间面积和像素数量、SPI 失败时仍调用 `lv_display_flush_ready()`、触摸缓存裁剪与释放。Display Task 用例检查 LCD/GUI 启动顺序、故障隔离、每轮 4 条消息预算、消息缓存、触摸通知及 GUI 服务。曾先运行失败用例，再实现代码并跑通回归。

最终重建日志：`06_Output/Logs/lvgl94_verified/RTT_elog_DMA_UART_ring_project-RTT_elog_DMA_UART_ring_project-rebuild.log`。最终 map：`RTT_elog_DMA_UART_ring_project/MDK-ARM/Objects/RTT_elog_DMA_UART_ring_project.map`。Host 汇总：`06_Output/Logs/host_tests/summary.json`。新增告警主要来自第三方 LVGL 源码和头文件，包括源文件结尾无换行、枚举类型混用；自研 `platform_gui.c` 与 `ui_smoke.c` 各收到 3 条来自 LVGL UEFI 头文件的换行告警，本身无编译告警。

`05_Tools\toolkit.bat build` 随后增量构建返回 0，输出 `[BUILD][PASS] Keil build completed without errors or warnings.`；该增量结果不替代上述全量重建的 341 条告警记录。

## 等待人工验证

本次未烧录、未运行实板测试。触摸端口暂按原始坐标直通并裁剪；须先按左上、右上、左下、右下、中心顺序采样，确认轴交换、反转、释放和长按。之后才能判定页面四角颜色、局部刷新、按钮点击、原 START/STOP/ONCE、采集、UART、实体键、IRQ 到反馈延迟，以及 Display Task 栈、FreeRTOS heap、LVGL 池的运行时余量。当前不宣称这些项目通过。
