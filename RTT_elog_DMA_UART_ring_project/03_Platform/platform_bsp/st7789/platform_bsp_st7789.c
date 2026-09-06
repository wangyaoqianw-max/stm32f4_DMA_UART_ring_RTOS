/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_bsp_st7789.c
 * @brief 当前板级 240x280 ST7789T3 静态构造实现
 * @author YaoQian Wang
 * @date 2026-09-06
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_bsp_st7789.h"

#include "platform_bsp_gpio.h"
#include "project_config.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Functions *********************************//
platform_error_t platform_bsp_st7789_construct_display(
    platform_st7789_t *display)
{
    platform_st7789_t constructed = PLATFORM_ST7789_INITIALIZER;
    platform_error_t result = PLATFORM_ERR_OK;

    if (display == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    constructed.width = PROJECT_DISPLAY_WIDTH;
    constructed.height = PROJECT_DISPLAY_HEIGHT;
    constructed.xOffset = PROJECT_DISPLAY_X_OFFSET;
    constructed.yOffset = PROJECT_DISPLAY_Y_OFFSET;
    constructed.madctl = PROJECT_DISPLAY_MADCTL;
    constructed.spiConfig.mode = PLATFORM_SPI_MODE_3;
    constructed.spiConfig.bitOrder = PLATFORM_SPI_BIT_ORDER_MSB_FIRST;
    constructed.spiConfig.dataBits = 8U;
    constructed.spiConfig.maxClockHz = PROJECT_DISPLAY_SPI_MAX_CLOCK_HZ;
    constructed.csActiveLevel = PLATFORM_GPIO_LEVEL_LOW;
    constructed.resetActiveLevel = PLATFORM_GPIO_LEVEL_LOW;
    constructed.backlightOnLevel = PLATFORM_GPIO_LEVEL_HIGH;

    result = platform_bsp_gpio_construct_lcd_cs(&constructed.cs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    result = platform_bsp_gpio_construct_lcd_dc(&constructed.dc);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    result = platform_bsp_gpio_construct_lcd_reset(&constructed.reset);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    result = platform_bsp_gpio_construct_lcd_backlight(
        &constructed.backlight);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    *display = constructed;
    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
