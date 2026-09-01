/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file impl_platform_gpio.c
 * @brief STM32 通用 GPIO 的 Platform Impl 构造与上下文绑定
 * @author Codex
 * @date 2026-09-01
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "impl_platform_gpio.h"

#include "platform_def.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Declaring *********************************//
static platform_bool_t stm32_gpio_is_single_pin(uint16_t pin);
//******************************** Declaring *********************************//

//******************************** Constants *********************************//
static const platform_gpio_ops_t g_stm32GpioOps = {
    NULL,
    NULL,
    NULL,
    NULL
};
//******************************** Constants *********************************//

//******************************** Private Functions *************************//
static platform_bool_t stm32_gpio_is_single_pin(uint16_t pin)
{
    if (pin == 0U) {
        return PLATFORM_FALSE;
    }

    return (((uint16_t)(pin & (uint16_t)(pin - 1U))) == 0U) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t impl_platform_gpio_construct(
    platform_gpio_t *gpio,
    const char *name,
    impl_platform_gpio_context_t *context)
{
    platform_gpio_init_params_t params;

    if ((gpio == NULL) || (context == NULL) ||
        (context->port == NULL) ||
        (stm32_gpio_is_single_pin(context->pin) != PLATFORM_TRUE)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    params.name = name;
    params.ops = &g_stm32GpioOps;
    params.implContext = context;

    return platform_gpio_init(gpio, &params);
}
//******************************** Functions *********************************//
