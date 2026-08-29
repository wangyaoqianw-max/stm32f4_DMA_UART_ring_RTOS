# Agent Architecture Contract

## 1. Architecture
APP
 ↓
Service
 ↓
Platform
 ↓
Impl
 ↓
Vendor / HAL / Hardware

## 2. Dependency Rules
- APP 只能依赖 Service。
- Service 可以依赖 Platform。
- Platform 不得依赖具体 STM32 HAL、FreeRTOS、Vendor。
- Impl 实现 Platform 契约。
- Vendor 原则上不修改。
- 禁止跨层直接访问硬件。

## 3. Layer Responsibilities
### APP
业务流程、状态机、任务组织。

### Service
RingBuffer、UART RX 服务、协议处理、日志服务等。

### Platform
定义硬件/系统能力接口，不包含具体 MCU 实现。

### Impl
HAL、DMA、IRQ、RTOS、BSP 和第三方库适配。

### Vendor
HAL、FreeRTOS、EasyLogger、RTT 等外部组件。

## 4. Core Design Rules
- 不使用动态内存，除非需求明确允许。
- ISR 中禁止阻塞。
- ISR 中不执行完整协议解析。
- Platform 公共接口不得泄漏 HAL Handle。
- Platform 公共接口不得泄漏 FreeRTOS Handle。
- 数据所有权必须明确。
- 异步接口必须定义完成/失败/取消语义。
- CubeMX 生成文件仅在 USER CODE 区域修改。

## 5. Generated Code Boundary
Core/
Drivers/
Middlewares/

这些目录视为工具或 Vendor 管理区域。

## 6. Agent Rules
- 不得为了修复编译错误擅自破坏架构边界。
- 不得自行改变冻结接口。
- 发现设计缺陷时停止架构性修改并写入 handoff.md。
- 优先最小修改。
- 不进行与当前任务无关的重构。