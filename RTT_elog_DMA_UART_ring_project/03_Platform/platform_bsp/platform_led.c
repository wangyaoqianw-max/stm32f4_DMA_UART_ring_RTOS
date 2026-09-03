/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_led.c
 * @brief Platform LED 轻量对象实现
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_led.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Private Functions *************************//
static platform_error_t platform_led_validate_active_level(
    platform_gpio_level_t activeLevel)
{
    if (activeLevel >= PLATFORM_GPIO_LEVEL_MAX) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return PLATFORM_ERR_OK;
}

static platform_gpio_level_t platform_led_get_inactive_level(
    platform_gpio_level_t activeLevel)
{
    return (activeLevel == PLATFORM_GPIO_LEVEL_LOW) ?
           PLATFORM_GPIO_LEVEL_HIGH : PLATFORM_GPIO_LEVEL_LOW;
}

static platform_error_t platform_led_validate_initialized(
    const platform_led_t *led)
{
    if (led == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (led->initialized == 0U) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return PLATFORM_ERR_OK;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t platform_led_init(platform_led_t *led)
{
    platform_error_t result = PLATFORM_ERR_OK;
    platform_gpio_config_t gpioConfig;

    if (led == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (led->initialized != 0U) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    result = platform_led_validate_active_level(led->activeLevel);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    gpioConfig.direction = PLATFORM_GPIO_DIRECTION_OUTPUT;
    gpioConfig.pull = PLATFORM_GPIO_PULL_NONE;
    gpioConfig.outputType = PLATFORM_GPIO_OUTPUT_PUSH_PULL;
    gpioConfig.initialLevel =
        platform_led_get_inactive_level(led->activeLevel);

    result = platform_gpio_configure(&led->gpio, &gpioConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    led->initialized = 1U;

    return PLATFORM_ERR_OK;
}

platform_error_t platform_led_on(platform_led_t *led)
{
    platform_error_t result = platform_led_validate_initialized(led);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return platform_gpio_write(&led->gpio, led->activeLevel);
}

platform_error_t platform_led_off(platform_led_t *led)
{
    platform_error_t result = platform_led_validate_initialized(led);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return platform_gpio_write(&led->gpio,
                               platform_led_get_inactive_level(
                                   led->activeLevel));
}

platform_error_t platform_led_toggle(platform_led_t *led)
{
    platform_error_t result = platform_led_validate_initialized(led);
    platform_gpio_level_t level = PLATFORM_GPIO_LEVEL_LOW;

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_gpio_read(&led->gpio, &level);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_led_validate_active_level(level);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (level == led->activeLevel) {
        return platform_led_off(led);
    }

    return platform_led_on(led);
}

platform_error_t platform_led_deinit(platform_led_t *led)
{
    platform_error_t result = platform_led_validate_initialized(led);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_gpio_deinit(&led->gpio);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    led->initialized = 0U;

    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
