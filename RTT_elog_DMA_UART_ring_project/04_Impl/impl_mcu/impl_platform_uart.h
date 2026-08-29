/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file impl_platform_uart.h
 * @brief STM32 USART1 的 Platform UART 构造入口
 * @author YaoQian Wang
 * @date 2026-08-29
 * @version V1.0
 *
 *****************************************************************************/

#ifndef IMPL_PLATFORM_UART_H
#define IMPL_PLATFORM_UART_H

//******************************** Includes *********************************//
#include "platform_uart.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 构造并绑定 STM32 USART1 的 Platform UART 对象
 * @param[in,out] uart            : 已使用 PLATFORM_UART_INITIALIZER 清零的对象存储
 * @param[in] name                : Platform UART 名称
 * @param[in] caps                : 设备能力标志
 * @param[in] config              : Platform UART 静态配置
 * @param[in] callback            : 可选异步事件回调，Phase 1 可为 NULL
 * @param[in] callbackContext     : 可选回调上下文
 * @return platform_error_t : 构造结果；本函数不初始化或启动硬件
 */
platform_error_t impl_platform_uart_usart1_construct(
    platform_uart_t *uart,
    const char *name,
    uint32_t caps,
    const platform_uart_config_t *config,
    platform_uart_callback_t callback,
    void *callbackContext);
//******************************** Functions ********************************//

#endif
