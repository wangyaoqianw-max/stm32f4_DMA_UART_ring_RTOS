/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_bsp_button.c
 * @brief Platform BSP User Key Button 构造实现
 * @author YaoQian Wang
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_bsp_button.h"

#include <stddef.h>

#include "platform_bsp_gpio.h"
#include "project_config.h"
//******************************** Includes *********************************//

//******************************** Functions *********************************//
platform_error_t platform_bsp_button_construct_user_key(
    platform_button_t *button)
{
    platform_error_t result = PLATFORM_ERR_OK;

    if (button == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    result = platform_bsp_gpio_construct_user_key(&button->gpio);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    button->activeLevel = PROJECT_USER_KEY_ACTIVE_LEVEL;
    button->pull = PROJECT_USER_KEY_PULL;

    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
