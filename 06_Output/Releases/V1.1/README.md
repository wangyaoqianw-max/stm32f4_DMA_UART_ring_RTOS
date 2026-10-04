# 固件 V1.1

日期：2026-10-04。Git 标签：`V1.1`。

STM32F411CE / GUI Guider Sensor Monitor / LVGL 9.4.0。包含温湿度、加速度、陀螺仪显示及 START/STOP/ONCE 控制。

- `.hex`：已烧录并由用户确认显示和功能正常的固件。
- `.axf`：同次构建的调试符号产物。
- `.map`：同次构建的资源布局。
- `manifest.json`：版本、产物大小、SHA-256 和验证状态。

Host 43/43、Keil 0 错误/380 个既有源码告警；J-Link 校验通过。新增页面板上资源高水位与最坏时延未测。完整交付记录见工程文档 `00_Doc/04_Agent/evidence/2026-10-04_guider_sensor_monitor/board_delivery.md`。
