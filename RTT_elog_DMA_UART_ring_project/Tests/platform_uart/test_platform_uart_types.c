/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_uart_types.c
 * @brief 验证 Platform UART 公共类型契约
 * @author Codex
 * @date 2026-08-28
 * @version V1.0
 *
 *****************************************************************************/

#include "platform_uart_types.h"
#include "platform_device.h"

typedef char assert_platform_size_is_32_bit[(sizeof(platform_size_t) == 4U) ? 1 : -1];
typedef char assert_uart_class_appended[
    (PLATFORM_DEVICE_CLASS_UART == (PLATFORM_DEVICE_CLASS_POWER + 1)) ? 1 : -1];

/**
 * @brief 验证调用者使用的 Platform UART 公共类型
 * @param[in] 无
 * @param[out] 无
 * @return 公共类型契约有效时返回 0
 */
int main(void)
{
    /**
     * 使用具体配置验证公共字段和枚举可被调用者使用，
     * 同时不引入任何具体硬件依赖。
     **/
    platform_uart_config_t config = {
        115200U,
        PLATFORM_UART_DATA_BITS_8,
        PLATFORM_UART_STOP_BITS_1,
        PLATFORM_UART_PARITY_NONE,
        PLATFORM_UART_FLOW_CONTROL_NONE,
        100U
    };

    return (115200U == config.baudRate) ? 0 : 1;
}
