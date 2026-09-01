/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_gpio.c
 * @brief Platform GPIO 抽象接口实现
 * @author Codex
 * @date 2026-09-01
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_gpio.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Functions *********************************//
platform_error_t platform_gpio_init(
    platform_gpio_t *gpio,
    const platform_gpio_init_params_t *params)
{
    if (gpio == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (params == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (gpio->initialized != 0U) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    if (params->ops == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    gpio->name = params->name;
    gpio->ops = params->ops;
    gpio->implContext = params->implContext;
    gpio->initialized = 1U;
    gpio->configured = 0U;

    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
