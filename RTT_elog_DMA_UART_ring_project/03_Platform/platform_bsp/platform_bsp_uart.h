/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_bsp_uart.h
 * @brief Platform BSP 通信串口构造契约
 * @author YaoQian Wang
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PLATFORM_BSP_UART_H
#define PLATFORM_BSP_UART_H

//******************************** Includes *********************************//
#include "platform_uart.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 构造并绑定逻辑通信串口
 * @param[in,out] uart : 调用者拥有的 Platform UART 对象存储
 * @param[in] config : 调用者提供的 UART 行为配置
 * @return platform_error_t : 构造与绑定结果
 * @note 本函数仅执行构造和板级绑定。
 * @note 本函数不执行 UART 生命周期操作。
 */
platform_error_t platform_bsp_uart_construct_communication(
    platform_uart_t *uart,
    const platform_uart_config_t *config);
//******************************** Functions ********************************//

#endif
