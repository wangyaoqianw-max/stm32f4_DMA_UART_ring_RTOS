# 字体优化验证（2026-10-04）

状态：自动验证完成；用户授权后烧录校验通过，用户确认文字均清晰可见，实板字体观感验收通过；用户已授权以 v1.1.1 提交推送。V1.1 标签和归档保持原样。

## 变更

从 Guider 2.0.1 的同一 montserratMedium.ttf 用 LVGL 官方 lv_font_conv 1.5.3 生成未压缩 4 bpp 字体；10/12/14 字号按实际页面和业务文案裁剪，24 字号仅保留数字、小数点和正负号。保留原始行高和基线，页面坐标及所有内存预算不改。

优化字体单独存放在 `01_APP/ui/fonts/`；原生项目及 `01_APP/ui/generated/` 原始文件、来源清单保持原样。Keil 与固件 Host 页面测试改用优化字体，原生模拟器路径仍使用原字体。再导出后的字体优化步骤见字体 README。

## 自动测试与构建

- `python 05_Tools/Tests/run_host_tests.py`：43/43 PASS。
- 真实 LVGL 页面测试遍历显示文字，直接检查字体 glyph 回调，禁止缺字被 fallback/占位符掩盖；保留读数边界、布局检查和 100 次状态切换。
- Keil 完整重建：0 errors / 380 warnings，数量与 V1.1 相同；未修改告警策略。
- Host LVGL 池 free/largest=3552 B、peak=17888 B，与 V1.1 相同；不能代替实板测量。
- `sensor_monitor_maximum.png` 是新字体的真实 Host 渲染，不是实板截图。

## 资源比较

| 项目 | V1.1 | 优化后 | 减少 |
|---|---:|---:|---:|
| 四套页面字体 RO | 49664 B | 4532 B | 45132 B |
| 页面字体引用的内建 fallback | 30028 B | 0 B | 30028 B |
| 工程 Flash（Code+RO+RW） | 337252 B | 262092 B | 75160 B |
| 工程 RAM（RW+ZI） | 94640 B | 94640 B | 0 B |

V1.1 原字体均引用 lv_font_montserrat_14_aligned；这套 fallback 导致额外字库进入链接。优化后字符集覆盖实际用途，fallback 为 NULL。原始四套字体占用 48.5 KiB 的统计没有包括 fallback，本次补齐该来源；未改全局 LVGL 配置。map 对比仅这些字体对象 RO 改变，Code/RW/ZI 相同。

Flash 剩余 262196 B，SRAM 剩余 36432 B。布局指标未变化；抗锯齿由 8 bpp 降为 4 bpp，仍需实板确认字形观感。

## 实板字体验收

用户已授权烧录：优化 HEX 哈希与记录一致，J-Link 返回 status=ok / errorlevel=0 / verified=true，下载后复位运行；COM9 STATUS 返回 STATUS STOPPED。当前板上运行优化固件，用户随后回复“可以，都能清晰看到”，确认本次字体显示效果通过；本次不重复此前已完成的完整业务回归。按用户指定版本归档至根目录 `06_Output/Releases/v1.1.1/`，对应 Git 标签 `v1.1.1`；不覆盖 V1.1 已验收固件。
