# STM32F411 多任务采集与显示工程

## 当前状态

2026-10-04：GUI Guider 2.0.1 / LVGL 9.4.0 Sensor Monitor 已替换临时点击页，显示温湿度、加速度与陀螺仪八项数据，提供 START/STOP/ONCE 控制，并沿用 Control Task 业务状态同步。

Host 43/43；Keil 完整重建 0 错误、380 个既有源码告警。RAM 94640 B，Flash 337252 B。J-Link 烧录校验通过，用户确认实板显示与功能正常，本次功能交付完成。板上高水位、最坏时延和故障注入未测。详见[实板交付记录](00_Doc/04_Agent/evidence/2026-10-04_guider_sensor_monitor/board_delivery.md)。

原生设计位于根目录 `ui_project/ui_project.guiguider`；导入生成文件和手写业务绑定分开维护，[重新生成步骤](01_APP/ui/generated/README.md)记录了尚未重新导出的标签坐标修正。

- [开发路线书](00_Doc/04_Agent/development_roadmap.md)
- [任务划分与实施计划](00_Doc/04_Agent/implementation_plan.md)
- [触屏驱动验证记录](00_Doc/04_Agent/CST816T_Driver_Verification.md)
- [工程交接](00_Doc/04_Agent/handoff.md)
- [文档索引](00_Doc/README.md)

升级顺序：CubeMX → CST816T 驱动 → LVGL 9.4.0 → GUI Guider。

本次固件版本：**V1.1**；已验收产物与校验清单位于 [版本归档](../06_Output/Releases/V1.1/README.md)。

## 最新固件 v1.1.1（2026-10-04）

字体优化已通过自动验证、烧录校验和实板显示验收。Flash 262092 B，比 V1.1 减少 73.4 KiB；RAM 94640 B 不变。[固件与校验清单](../06_Output/Releases/v1.1.1/README.md)。V1.1 归档保留。
