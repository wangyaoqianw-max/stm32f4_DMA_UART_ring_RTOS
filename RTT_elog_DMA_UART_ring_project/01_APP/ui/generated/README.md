# GUI Guider 生成文件

来源：工作区根目录 `ui_project/generated/`，由 GUI Guider 2.0.1 于 2026-10-04 实际生成，目标 LVGL 9.4.0 / 240 × 280 / RGB565。

本目录导入 18 个生成文件，13 个 `.c` 编入 Keil 的 `GUI_Guider_Generated` 文件组：

- `gg_utils.c/.h` 和 `gui_guider.h`：页面入口与类型。
- `screens/`：页面和三个默认层。
- `events/`：原生空事件初始化及声明；真实业务事件在 APP 绑定。
- `assets/fonts/`：Montserrat Medium 10/12/14/24 及字体声明。
- `assets/images/gg_image.h`：无图片时生成的空声明，供公共头文件引用。

`../custom/custom.h` 保留原始生成工程的空扩展接口头，供事件文件包含。未导入 simulator、SDL、目标板初始化或第二份 LVGL。

## 边界与许可

原始生成文件未修改，逐文件 SHA-256 见 `source_manifest.json`。供应方版权声明保持；附带许可证与软件清单在 `licenses/`。用户已知晓许可核对结果，明确指示按个人、非商业项目继续；该指令不改变供应方许可文本。

手写业务代码位于 `../ui_sensor_monitor.c/.h`。显示、触摸、tick 与部分刷新继续使用本项目 Platform 端口。

## 再次生成

1. 打开工作区根目录 `ui_project/ui_project.guiguider`，保留 LVGL 9.4.0、240 × 280、16 位和字体字符集。
2. 在 Guider 保存并生成，将新的 `generated/` 对应文件更新到本目录；保持许可证，更新文件清单及 SHA-256。
3. 更新新增/删除的 Keil 文件及包含路径；不覆盖 `../ui_sensor_monitor.c/.h`。
4. 执行根目录 `python 05_Tools/Tests/run_host_tests.py` 和同一 Keil Target 完整重建。

已发现原始导出中温湿度数值标签高 27 px，而字体行高为 29 px。原生设计源已改为 y=20 / h=29；本次桌面无法访问 Guider 窗口，尚未重新生成此坐标改动。APP 对这两个标签应用同样修正，因此导入的生成文件仍保持逐字一致。再次生成修正后的设计后，APP 的同值适配仍有效。

APP 在创建页面时使用生成器的显式样式，禁用默认主题附加的状态过渡；按钮禁用样式使用实色。固件继续只链接生成字体，默认字体为 `lv_font_montserratMedium_14`，不同时启用内建 Montserrat 14。
