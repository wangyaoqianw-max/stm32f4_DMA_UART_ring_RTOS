/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_bsp_led.c
 * @brief Platform BSP Status LED 构造实现
 * @author YaoQian Wang
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_bsp_led.h"

#include <stddef.h>

#include "platform_bsp_gpio.h"
#include "project_config.h"
//******************************** Includes *********************************//

//******************************** Functions *********************************//
platform_error_t platform_bsp_led_construct_status_led(platform_led_t *led)
{
    if (led == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    led->activeLevel = PROJECT_STATUS_LED_ACTIVE_LEVEL;

    return platform_bsp_gpio_construct_status_led(&led->gpio);
}
//******************************** Functions *********************************//
