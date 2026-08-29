# Current Implementation Plan

## Metadata
Status: DESIGN
Scope: STM32 UART Platform Impl
Architecture Version: 1

## 1. Objective

将已经完成的 Platform UART 抽象
接入 STM32F411 USART1。

本阶段只贯通：

Platform UART
    ↓
STM32 UART Impl
    ↓
HAL UART

暂不实现：
- RingBuffer
- 协议解析
- UART Service
- APP 通信业务

## 2. Current State

Completed:
- Platform common object model
- Platform UART types
- Platform UART public API
- Platform UART unit tests

Not implemented:
- STM32 UART Impl
- DMA
- IDLE
- UART Service
- RingBuffer

## 3. Files In Scope

Create / Implement:
- 04_Impl/impl_mcu/impl_platform_uart.c
- corresponding header if required

Possibly Modify:
- Keil project configuration
- CubeMX USER CODE sections

Do Not Modify:
- Platform UART contract
- Vendor source
- unrelated modules

## 4. Interfaces

使用现有：
platform_uart_t
platform_uart_ops_t
platform_lifecycle_ops_t

不得重新设计 Platform UART API。

## 5. Implementation Steps

1. 定义 STM32 UART Impl Context
2. 建立 USART1 与 platform_uart_t 的绑定
3. 实现 lifecycle
4. 实现 blocking write/read
5. 编译
6. 测试
7. 再进入 async/DMA 阶段

## 6. Constraints

...

## 7. Verification

...

## 8. Completion Criteria

...