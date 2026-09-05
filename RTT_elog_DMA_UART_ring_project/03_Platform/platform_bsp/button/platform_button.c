/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_button.c
 * @brief Platform Button 轻量对象实现
 * @author YaoQian Wang
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_button.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Private Functions *************************//
static platform_error_t platform_button_validate_active_level(
    platform_gpio_level_t activeLevel)
{
    if ((activeLevel != PLATFORM_GPIO_LEVEL_LOW) &&
        (activeLevel != PLATFORM_GPIO_LEVEL_HIGH)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t platform_button_validate_pull(platform_gpio_pull_t pull)
{
    if (pull >= PLATFORM_GPIO_PULL_MAX) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t platform_button_validate_initialized(
    const platform_button_t *button)
{
    if (button == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (button->initialized == 0U) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return PLATFORM_ERR_OK;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t platform_button_init(platform_button_t *button)
{
    platform_error_t result = PLATFORM_ERR_OK;
    platform_gpio_config_t config = {
        PLATFORM_GPIO_DIRECTION_INPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_LOW
    };

    if (button == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (button->initialized != 0U) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    result = platform_button_validate_active_level(button->activeLevel);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_button_validate_pull(button->pull);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    config.pull = button->pull;
    result = platform_gpio_configure(&button->gpio, &config);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    button->initialized = 1U;

    return PLATFORM_ERR_OK;
}

platform_error_t platform_button_read(platform_button_t *button,
                                      platform_button_state_t *state)
{
    platform_error_t result = platform_button_validate_initialized(button);
    platform_gpio_level_t level = PLATFORM_GPIO_LEVEL_LOW;

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (state == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    result = platform_gpio_read(&button->gpio, &level);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_button_validate_active_level(level);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    *state = (level == button->activeLevel) ?
             PLATFORM_BUTTON_STATE_PRESSED : PLATFORM_BUTTON_STATE_RELEASED;

    return PLATFORM_ERR_OK;
}

platform_error_t platform_button_deinit(platform_button_t *button)
{
    platform_error_t result = platform_button_validate_initialized(button);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_gpio_deinit(&button->gpio);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    button->initialized = 0U;

    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
