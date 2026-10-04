# Sensor Monitor 实板验收与交付

日期：2026-10-04；分支：`main`。

## 烧录与运行

用户明确指示“烧录试试”，随后确认“正常显示，功能也都正常”，并授权更新状态、提交和推送。

- 下载器：J-Link，序列号 602713300；STM32F411CE / SWD / 4000 kHz；连接时供电 3.283 V。
- 固件：`MDK-ARM/Objects/RTT_elog_DMA_UART_ring_project.hex`。
- HEX SHA-256：`9b461fdb75d7227568c19bf14c0eba04d6bbb44cfb9d7780525d63427fe7ceab`，与自动验证产物一致。
- J-Link 返回 `status=ok`、`errorlevel=0`、`verified=true`；下载后执行复位并运行。
- COM9 / 115200 发送 `STATUS\r\n`，收到 `STATUS STOPPED\r\n`，确认固件响应正常。
- 用户确认新页面显示正常、功能正常。未拆分记录八项读数、每个控制入口及圆角检查的逐项操作结果；不将整体确认扩写为逐项实测。

## 交付状态

本次 Sensor Monitor 页面移植按用户确认完成功能交付。自动测试与资源核算见 [验证记录](verification.md)。

未测项：新增页面板上任务栈、FreeRTOS/LVGL 堆高水位，队列峰值，最坏刷新和触摸反馈时延，故障注入。保留为后续按需检查，不冒用上一阶段资源观测。

原生设计两处数值标签坐标已同步，但尚未重新原生导出；固件 APP 同值修正有效，导入生成文件保持原样。再次导出步骤见 `01_APP/ui/generated/README.md`。

本次提交包含固件、Host 测试、原生 Guider 项目与生成文件、许可证、验证证据和文档。Guider 本地 `platform/` 中捆绑的第二份 LVGL、SDL 与模拟器运行环境不纳入 Git；需要模拟器时由 GUI Guider 2.0.1 创建/恢复运行环境。

## 版本归档

用户指定本次固件版本为 **V1.1**。同次构建并已验收的 HEX、AXF、map 及 SHA-256 清单保存在根目录 `06_Output/Releases/V1.1/`；对应 Git 标签为 `V1.1`。
