# 本地工具入口

从 `STM32F4_Bootloader_OTA_Test_Project/05_Tools` 的通用 Core、Keil 和 J-Link 工具移植，来源提交 `77e170c`。工程配置已改为当前 STM32F411CEU6 Keil Target；未包含来源工程的 OTA、YMODEM 和阶段测试工具。

首次使用时，编辑 Git 忽略的 `Config/toolchain.local.bat` 中本机工具路径；可从 `Config/toolchain.local.example.bat` 复制模板。

```bat
05_Tools\toolkit.bat build
05_Tools\toolkit.bat flash [run|prepare]
05_Tools\toolkit.bat rtt [seconds]
05_Tools\toolkit.bat run [seconds]
```

`run` 依次执行 Keil 编译、J-Link 烧录和 RTT 采集。日志输出到 `06_Output/Logs`。烧录和 RTT 会访问真实目标板，需确认已连接对应设备后执行。
