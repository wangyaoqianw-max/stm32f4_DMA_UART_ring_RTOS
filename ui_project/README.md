# Sensor Monitor 原生项目

使用 GUI Guider 2.0.1 打开 `ui_project.guiguider`，目标 LVGL 9.4.0、240×280、RGB565。`generated/` 保留实际导出文件，`custom/` 与 `licenses/` 保留生成工程扩展及许可证。

本地 `platform/` 包含模拟器环境和第二份 LVGL，不纳入 Git；需要模拟器时由 GUI Guider 创建/恢复。固件只使用工程 `05_Vendors/lvgl`。

两处温湿度数值标签已在原生源调整到 y=20 / h=29，尚未重新原生导出；固件 APP 已应用同值修正。重新生成与移植步骤见 `../RTT_elog_DMA_UART_ring_project/01_APP/ui/generated/README.md`，不要覆盖手写业务绑定。
