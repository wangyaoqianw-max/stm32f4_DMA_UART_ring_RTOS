/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file impl_platform_bsp_uart.c
 * @brief 当前板级 Platform BSP UART 绑定
 * @author YaoQian Wang
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_bsp_uart.h"

#include "impl_platform_uart.h"
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
    const platform_uart_config_t *config)
{
    /**
     * BSP 契约在本层拒绝无效的调用者组合输入。
     **/
    if ((NULL == uart) || (NULL == config)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    /**
     * USART1 映射仅保留在当前板级实现内部。
     **/
    return impl_platform_uart_usart1_construct(uart,
                                               "communication_uart",
                                               PLATFORM_DEVICE_CAP_NONE,
                                               config,
                                               NULL,
                                               NULL);
}
//******************************** Functions ********************************//
