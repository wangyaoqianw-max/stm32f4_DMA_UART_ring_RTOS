/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_uart_types.c
 * @brief Verify the public Platform UART type contract
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
 * @brief Verify the Platform UART public type values used by callers
 * @param[in] None
 * @param[out] None
 * @return Zero when the public type contract is valid
 */
int main(void)
{
    /**
     * Use a concrete configuration to ensure each public field and enum is
     * available to callers without any hardware-specific dependency.
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
