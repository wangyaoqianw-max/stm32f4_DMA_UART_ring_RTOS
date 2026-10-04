# 固件 v1.1.1

日期：2026-10-04。Git 标签：`v1.1.1`；基线：`V1.1`。

本版优化 Sensor Monitor 字体：四套字符子集、4 bpp、保留原行高和基线，移除无用 fallback 字库。布局和业务功能保持。

- Flash：262092 B，比 V1.1 减少 75160 B（73.4 KiB）。
- RAM：94640 B，不变。
- Host 43/43；Keil 0 错误、380 个既有告警。
- J-Link 烧录校验通过，串口返回 STATUS STOPPED；用户确认所有文字清晰可见。
- 板上资源高水位与最坏时延未测。

HEX 是已烧录并验收的固件，AXF 与 map 来自同次构建。SHA-256 和大小见 `manifest.json`。V1.1 归档保持原样。

完整证据见工程 `00_Doc/04_Agent/evidence/2026-10-04_font_optimization/verification.md`。
