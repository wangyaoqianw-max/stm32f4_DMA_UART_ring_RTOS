# 00_Config

本目录只保存产品级、编译期确定的静态配置；修改后需要重新编译才能生效。

APP Phase 1 的 `project_config.h` 集中定义通信 UART 参数、通信缓存尺寸、通信任务参数和运行策略超时。

以下内容不属于静态配置，必须保留在实际运行模块中：

- 对象实例或对象指针
- Storage 地址
- 运行时 Context、State、Statistics
- HAL、DMA 或 USART Handle
- Impl 映射
- Callback
- RTOS native handle
