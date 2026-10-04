# 固件优化字体

生成工具：LVGL 官方 lv_font_conv 1.5.3；原字体是 GUI Guider 2.0.1 的 `resources/assets/font/montserratMedium.ttf`。原字库版权声明保留，源字体 SHA-256、字符集、尺寸及输出 SHA-256 记录在 `manifest.json`。

4 bpp、未压缩，保留原生导出的行高和基线。只保留当前页面实际所需字符，无 fallback；新增文案时须扩展对应字符集，执行缺字测试。

原始 Guider 生成字体保留在 `../generated/assets/fonts/`。Keil 只编译本目录字体，符号名及原头文件声明保持兼容。原生 Guider 模拟器仍使用原始字体，固件 Host 测试使用本目录。

## 重新生成

从工作区根目录执行（依赖保存在忽略的本地构建目录）：

```powershell
npm.cmd install --prefix 06_Output/Build/font_tools --cache 06_Output/Build/npm_cache --ignore-scripts --no-audit --no-fund lv_font_conv@1.5.3
python 05_Tools/Tests/generate_ui_fonts.py --font E:/APP/ProgramFile/Guider/GUIGuider/resources/assets/font/montserratMedium.ttf --converter 06_Output/Build/font_tools/node_modules/lv_font_conv/lv_font_conv.js
python 05_Tools/Tests/run_host_tests.py
```

原生重新导出后核对文案、字体尺寸、行高与基线，更新 manifest 字符集，重生成优化字体并执行同一 Keil Target 构建。不要把原始字体重新加入固件链接，否则会重复定义同名符号。
