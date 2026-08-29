# 工程交接说明

更新时间：2026-08-29

## 1. 工程快照

- 工程根目录：`RTT_elog_DMA_UART_ring_project/`
- 当前分支：`main`
- 当前 HEAD：`061e670 增加代码设计规范文档`
- MCU：STM32F411CEU6，UFQFPN48
- 软件环境：STM32 HAL、CMSIS-RTOS2 / FreeRTOS、Keil、EasyLogger、SEGGER RTT
- CubeMX 当前配置：USART1 异步模式、一个 `defaultTask`；尚未发现 USART1 DMA 映射配置
- 目标：实现 UART + DMA + Ring Buffer + FreeRTOS，并通过分层架构隔离业务与硬件

依赖方向固定为：

```text
APP -> Service -> Platform -> Impl -> HAL / RTOS / Hardware
```

## 2. 当前完成情况

### 已完成

- 已建立 `APP / Service / Platform / Impl / Vendors` 分层目录和架构说明。
- `platform_common` 已提供对象、设备、服务、生命周期、错误码和公共类型基础。
- Platform UART 抽象层已经完成：
  - `03_Platform/platform_mcu/uart/platform_uart_types.h`
  - `03_Platform/platform_mcu/uart/platform_uart.h`
  - `03_Platform/platform_mcu/uart/platform_uart.c`
  - 支持阻塞读写、异步读写、取消和统一事件回调。
  - 不暴露 HAL、DMA 或 RTOS 类型，不使用动态内存。
- Platform UART 设计和测试已经落盘：
  - `00_Doc/02_架构设计/Platform_UART抽象层设计.md`
  - `Tests/platform_uart/test_platform_uart_types.c`
  - `Tests/platform_uart/test_platform_uart.c`
- EasyLogger、SEGGER RTT 及既有日志适配代码已存在，`defaultTask` 当前会调用日志初始化；该旧日志接口未在 UART 工作中重构或完整验证。

### 尚未完成

- `00_Doc/00_项目需求/项目需求说明书.md` 目前为空，且目录未提交。
- `04_Impl/impl_mcu/impl_dma.c/.h` 和 `impl_platform_uart.c` 为空占位文件。
- `02_Service/service_log/` 的 `.c/.h/cfg.h` 为空占位文件。
- APP 业务、UART 接收 Service、Ring Buffer、协议解析均未实现。
- 尚未完成 UART DMA / IDLE 中断、HAL Callback、FreeRTOS 通知链路和板上联调。
- 本轮只执行了主机侧 Platform UART 测试，未执行 Keil 全工程构建和硬件测试。

## 3. Platform UART 已确定契约

- `platform_uart_t` 的首字段必须是 `platform_device_t`。
- 对象首次构造前使用 `PLATFORM_UART_INITIALIZER` 零初始化；同一对象禁止重复构造。
- 生命周期继续使用 `platform_lifecycle_ops_t`，UART Ops 不重复生命周期接口。
- 数据接口仅允许在 `PLATFORM_OBJECT_STARTED` 状态调用。
- 异步事件也只在 `STARTED` 状态接受；Impl 必须在退出该状态前关闭并排空事件源。
- 同步接口失败时完成长度为 0；成功完成量不得超过请求长度。
- 异步 Buffer 由调用者持有，在完成、错误或取消事件到达前必须有效。
- 回调默认可能运行于 ISR，不得阻塞、使用普通 Mutex、动态分配或执行协议解析。
- `RX_DATA` 只表示收到字节，不代表完整协议帧。

## 4. 最近验证结果

2026-08-29 使用 GCC C11、`-pedantic-errors -Wall -Wextra -Werror` 重新验证：

```text
test_platform_uart_types: 0
test_platform_uart:       0
Platform UART 禁止依赖扫描: 0
```

代码审查最终结论：无 Critical、无 Important，Ready。

## 5. 当前 Git 状态

本文件写入前只有以下未跟踪目录：

```text
?? RTT_elog_DMA_UART_ring_project/00_Doc/00_项目需求/
?? RTT_elog_DMA_UART_ring_project/00_Doc/04_Agent/
```

Platform UART 的主要提交为 `dcdb25b` 至 `e551601`；代码规范提交为 `061e670`。
接手时必须先重新执行 `git status --short`，不得覆盖或顺带提交用户的新改动。

## 6. 推荐继续顺序

1. 完成并确认 `00_Doc/00_项目需求/项目需求说明书.md`。
2. 根据需求确定 DMA 模式、IDLE 策略、Ring Buffer 溢出策略、任务通知方式和多实例要求。
3. 补充 CubeMX USART1 DMA / NVIC 资源配置并记录资源分配。
4. 设计并实现 UART Impl，将 HAL、DMA、IRQ 和 RTOS 依赖限制在 `04_Impl`。
5. 实现 UART 接收 Service 和 Ring Buffer，再接入 APP。
6. 依次执行主机测试、Keil 构建和板上压力测试。

不要直接开始 Impl：需求书中的数据流、缓存、溢出、取消和停止行为确定后，先回查
`Platform_UART抽象层设计.md` 是否仍满足需求。

## 7. 接手模型必读

按顺序读取：

1. `00_Doc/04_Agent/handoff.md`
2. `00_Doc/00_项目需求/项目需求说明书.md`
3. `00_Doc/02_架构设计/软件架构说明.md`
4. `00_Doc/02_架构设计/Platform_UART抽象层设计.md`
5. `00_Doc/02_架构设计/嵌入式项目C代码设计规范.md`

项目自研代码默认使用中文注释；Platform 不得泄漏 HAL/RTOS 类型；修改前先确认范围，保留所有无关工作区改动。
