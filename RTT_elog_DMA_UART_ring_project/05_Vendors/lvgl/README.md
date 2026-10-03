# LVGL 9.4.0 工程副本

- 直接来源：`wangyaoqianw-max/Embedded_Engineering_Library` 的 `third_party/LVGL/v9.4`，提交 `8ae16732fb3be01e4fed0c5d8cdae78c1ad46cd0`。
- `lv_version.h` 声明 `9.4.0`。来源仓库与 LVGL 官方 v9.4.0 标签的逐文件一致性未验证。
- 本工程复制了完整 `src/`（1149 个文件）、`lvgl.h`、`lvgl_private.h`、`lv_version.h`、`lv_conf_template.h`、`LICENCE.txt` 和 `COPYRIGHTS.md`。未复制 `demos/`、`examples/`。LVGL C 源码与头文件未修改。
- 许可证：MIT，见 [LICENCE.txt](LICENCE.txt)；随附第三方版权信息见 [COPYRIGHTS.md](COPYRIGHTS.md)。
- 工程配置位于 `../../00_Config/lv_conf.h`，当前 MDK 工程编入 `src/` 中 388 个 C 文件；未使用 LVGL 自带的硬件驱动。
- 当前集成临时显示与触摸测试页；板上显示、触摸方向、点击与内存余量已有记录，最坏响应时延未测。本轮已按用户确认交付，详见 [交付记录](../../00_Doc/04_Agent/evidence/2026-10-03_lvgl94_minimal_port/delivery.md)。
