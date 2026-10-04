# 原生 UI 工程预检

日期：2026-10-04
状态：原生静态页面已完成，Guider 生成、模拟器构建和渲染验证通过；业务交互与固件接入尚未实施。

## 已确认

- 当前分支 `main`，本阶段未提交或推送。
- 工作区设计源文件：`ui_project/ui_project.guiguider`。
- GUI Guider 2.0.1，LVGL 9.4.0，240 × 280，16 位颜色，Simulator 目标。
- 初始默认 screen 为空；现已直接修改工程 JSON，创建 32 个 LVGL 对象，包括页面根对象、4 个数据卡片、8 个数据值标签、状态与反馈标签、START/STOP 和 ONCE 按钮。
- 已生成正式工程的 `ui_project/generated/`，生成器为 GUI Guider 2.0.1。
- 本机安装：`E:\APP\ProgramFile\Guider\GUIGuider\GUIGuider.exe`。
- 同版本本机工程 `E:\my_project_2026\Guider_project\test_001\test_001.guiguider` 提供 Label/Button 初始结构；字体属性由本次原生编辑器保存样本确认。
- 原生字段为 `text_size` 和 `text_family`，字体使用 `montserratMedium.ttf`，尺寸 10/12/14/24；内边距使用 `pad_left/right/top/bottom`。生成器忽略 `pad_all`，因此已移除这种写法。

## 官方依据

本机官方文档 `resources/assets/docs/UG/advanced_usage.html` 的 JSON-based UI editing 章节支持直接编辑 JSON，并建议先用可视化编辑器建立初始结构，再批量调整，最后返回编辑器/模拟器验证。

`resources/assets/docs/UG/ide_overview.html` 的 Shortcut keys 章节确认：Ctrl+S 保存，Ctrl+G 生成代码，Ctrl+E 打开 JSON 编辑器，Ctrl+J 打开 JS 模拟器。

## 自动验证结果

- JSON 解析、对象名称唯一性、页面和子控件边界检查通过。
- 原生生成的 `generated/screens/gg_screen.c` 包含 32 次对象创建，四种字号及四边内边距均进入生成代码。
- 动态数字所需的 `+`、`-`、`.`、`%`、`/` 已加入基本字符集，并检查四个生成字体的 cmap，均包含数字及上述符号。
- 使用 Guider 自带 CMake 3.x / MinGW GCC 15.2.0 构建 Simulator，退出码 0，`[100%] Built target simulator`。
- 实际模拟器截图确认标题、STOPPED、四张卡片、八项空数据占位、单位、反馈和两个按钮完整显示，没有多余滚动条。该截图不代表实板圆角遮挡验证。
- 构建产物：`06_Output/Build/guider_sensor_monitor/bin/simulator.exe`。
- 构建日志保存于计划工作目录的 `simulator_build.log`；实际渲染截图为本次 Codex 可视化目录下 `guider-simulator-render.png`。
- 原生生成控件字段采用父子名称拼接，例如 `guider_ui.screen.container_temperature_label_temperature_value`。

## 操作记录和边界

先前要求用户手工添加原生控件样本的判断已撤回。本次已验证直接批量编辑 `.guiguider` 可以自动完成 UI 设计，不需要用户逐个拖放控件。

重新导入当前已打开的同一路径工程时，Guider 会保存内存中的旧内容，可能覆盖磁盘改动。本次先从计划工作目录的候选副本导入，再载入正式工程；最终生成文件位于 `ui_project/generated/`。后续外部编辑前应先保存/关闭原生工程，或使用候选副本导入。

按钮当前仅有原生静态布局，尚未连接 ControlTask；模拟器运行只验证静态渲染，未声称 START/STOP/ONCE 业务逻辑已通过。固件源码、Keil 工程和开发板均未修改/烧录。

后续按既有计划完成控制反馈、数据绑定和固件接入，再执行 Host 单元测试及 Keil 构建；需要实板人工验证时停下。
