/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_impl_platform_uart.c
 * @brief 验证 STM32 Platform UART 配置映射的 Impl 行为
 * @author YaoQian Wang
 * @date 2026-08-29
 * @version V1.0
 *
 *****************************************************************************/

#include "platform_uart.h"
#include "usart.h"

UART_HandleTypeDef huart1;

#include "../../04_Impl/impl_mcu/impl_platform_uart.c"

#define TEST_ASSERT(condition)       \
    do {                             \
        if (!(condition)) {          \
            return __LINE__;         \
        }                            \
    } while (0)

static platform_uart_config_t make_uart_config(platform_uart_data_bits_t dataBits,
                                               platform_uart_parity_t parity)
{
    platform_uart_config_t config = {
        115200U,
        dataBits,
        PLATFORM_UART_STOP_BITS_1,
        parity,
        PLATFORM_UART_FLOW_CONTROL_NONE,
        100U
    };

    return config;
}

static int test_supported_byte_stream_configs_are_accepted(void)
{
    UART_HandleTypeDef halUart = {0};
    platform_uart_config_t config = make_uart_config(PLATFORM_UART_DATA_BITS_8,
                                                      PLATFORM_UART_PARITY_NONE);

    TEST_ASSERT(stm32_uart_apply_config(&halUart, &config) == PLATFORM_ERR_OK);
    TEST_ASSERT(halUart.Init.WordLength == UART_WORDLENGTH_8B);
    TEST_ASSERT(halUart.Init.Parity == UART_PARITY_NONE);

    config = make_uart_config(PLATFORM_UART_DATA_BITS_8, PLATFORM_UART_PARITY_EVEN);
    TEST_ASSERT(stm32_uart_apply_config(&halUart, &config) == PLATFORM_ERR_OK);
    TEST_ASSERT(halUart.Init.WordLength == UART_WORDLENGTH_9B);
    TEST_ASSERT(halUart.Init.Parity == UART_PARITY_EVEN);

    config = make_uart_config(PLATFORM_UART_DATA_BITS_8, PLATFORM_UART_PARITY_ODD);
    TEST_ASSERT(stm32_uart_apply_config(&halUart, &config) == PLATFORM_ERR_OK);
    TEST_ASSERT(halUart.Init.WordLength == UART_WORDLENGTH_9B);
    TEST_ASSERT(halUart.Init.Parity == UART_PARITY_ODD);

    config = make_uart_config(PLATFORM_UART_DATA_BITS_7, PLATFORM_UART_PARITY_EVEN);
    TEST_ASSERT(stm32_uart_apply_config(&halUart, &config) == PLATFORM_ERR_OK);
    TEST_ASSERT(halUart.Init.WordLength == UART_WORDLENGTH_8B);
    TEST_ASSERT(halUart.Init.Parity == UART_PARITY_EVEN);

    config = make_uart_config(PLATFORM_UART_DATA_BITS_7, PLATFORM_UART_PARITY_ODD);
    TEST_ASSERT(stm32_uart_apply_config(&halUart, &config) == PLATFORM_ERR_OK);
    TEST_ASSERT(halUart.Init.WordLength == UART_WORDLENGTH_8B);
    TEST_ASSERT(halUart.Init.Parity == UART_PARITY_ODD);

    return 0;
}

static int test_nine_bit_configs_are_not_supported(void)
{
    UART_HandleTypeDef halUart = {0};
    platform_uart_config_t config = make_uart_config(PLATFORM_UART_DATA_BITS_9,
                                                      PLATFORM_UART_PARITY_NONE);

    TEST_ASSERT(stm32_uart_apply_config(&halUart, &config) == PLATFORM_ERR_NOT_SUPPORTED);

    config = make_uart_config(PLATFORM_UART_DATA_BITS_9, PLATFORM_UART_PARITY_EVEN);
    TEST_ASSERT(stm32_uart_apply_config(&halUart, &config) == PLATFORM_ERR_NOT_SUPPORTED);

    config = make_uart_config(PLATFORM_UART_DATA_BITS_9, PLATFORM_UART_PARITY_ODD);
    TEST_ASSERT(stm32_uart_apply_config(&halUart, &config) == PLATFORM_ERR_NOT_SUPPORTED);

    return 0;
}

int main(void)
{
    int result = test_supported_byte_stream_configs_are_accepted();

    if (result != 0) {
        return result;
    }

    return test_nine_bit_configs_are_not_supported();
}
