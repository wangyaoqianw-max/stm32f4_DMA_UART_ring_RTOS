# 项目面试准备手稿

本目录用于本项目的技术面试准备，内容严格围绕当前仓库实际实现组织，不作为新的架构规范或实施计划。

## 文档划分

1. `01_五层架构设计_面试手稿.md`
   - APP / Service / Platform / Impl / Vendor
   - 依赖方向、Composition Root、Ownership、生命周期、错误语义、迁移边界
   - 重点回答“为什么这样设计”以及“当前方案的代价是什么”

2. `02_UART_DMA_RingBuffer_面试手稿.md`
   - UART 基础、Circular DMA、IDLE/HT/TC
   - DMA position、SPSC RingBuffer、ISR/Task 协作
   - TX DMA transaction、buffer lifetime、timeout/cancel、错误和吞吐边界

3. `03_FreeRTOS_面试手稿.md`
   - 五个产品 Task、优先级、Task Ownership
   - Queue、Thread Flag、阻塞/非阻塞策略
   - absolute deadline、STOP race、资源分配、RTOS Adapter
   - 常见并发与调度追问

4. `04_传感器_I2C_SPI_ST7789_面试手稿.md`
   - 软件 I2C、DHT20、MPU6050、Acquisition Service
   - SPI、ST7789、局部刷新、Display Queue、显示资源约束
   - 两部分合并准备，作为次重点模块

## 建议使用方式

每个问题按四层掌握：

```text
30~60 秒主回答
    ↓
为什么这样设计
    ↓
追到底层机制 / 当前代码
    ↓
说明方案边界与替代方案
```

后续逐个模块阅读源码时，优先把问题反向映射到真实代码，而不是继续扩充泛化知识点。

## 事实边界

面试时只陈述已经在仓库或测试中验证过的内容。尤其注意：

- 当前 Task stack / Queue depth 是已验证的 bring-up baseline，不是经过完整 high-water/peak occupancy 优化后的最小值；
- APP/Service 业务对象和 DMA/RingBuffer storage 采用静态 ownership，但当前 CMSIS-RTOS2 Thread/Queue 创建没有显式提供静态 control block/stack storage，不能声称整个 RTOS 完全静态分配；
- 当前 SPI Display 是同步阻塞发送，不是 SPI DMA；
- 当前 UART RX 为 DMA Circular + IDLE/HT/TC，应用协议帧边界仍由 CRLF parser 决定，IDLE 不是协议帧结束符。
