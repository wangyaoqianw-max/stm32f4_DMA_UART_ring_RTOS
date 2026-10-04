# Sensor Monitor 移植预检

日期：2026-10-04

## 自动测试基线

- 命令：`python 05_Tools/Tests/run_host_tests.py`
- 结果：退出码 0，42/42 通过。该结果属于移植前基线，不代表新增 UI 已通过测试。
- 日志：仓库根目录 `.superpowers/sdd/GUI_Guider_Sensor_Monitor_Execution_Plan/host_baseline.log`。

## 生成代码许可待确认

- `ui_project/generated/screens/gg_screen.c` 等生成文件声明 `Copyright 2026 NXP` 和 `NXP Proprietary`。
- `ui_project/licenses/LICENSE.txt` 为 NXP Online Code Hosting Software License Agreement v1.4 / May 2025。
- 第 1.2 节将 Authorized System 定义为包含 NXP 产品的硬件，或仅与 NXP 产品结合使用的软件；第 2.1 节将使用及派生作品范围限定在与 NXP 产品组合使用。
- `ui_project/SCR.txt` 的顶层 Outgoing License 字段为空。LVGL 子组件列为 MIT，尚未发现将 NXP 专有生成代码另行授权给非 NXP 平台的条款。
- [NXP 员工于 2026-01-12 对非 NXP 使用及购买许可的答复](https://community.nxp.com/t5/GUI-Guider/Gui-Guider%E7%94%A8%E4%BA%8E%E9%9D%9ENXP%E7%9A%84%E4%BA%A7%E5%93%81%E6%98%AF%E5%90%A6%E5%8F%AF%E4%BB%A5%E8%B4%AD%E4%B9%B0License/m-p/2292230/highlight/true)：未提供商业许可，GUI Guider 仅支持 NXP 平台产品。该答复与本地许可限制一致，但不能代替对用户另行取得授权的核验。

预检时尚未确认 STM32 工程具备上述代码的适用授权，曾依据 embedded-code-development 的来源与许可预检要求暂停导入。随后用户明确说明这是个人、非商业项目，并指示按原方案继续。因此继续 Guider 页面集成，保留供应方声明和许可证；用户指令不作为另行取得 NXP 授权的证明，也不改变许可文字。

本阶段不烧录、提交或推送。
