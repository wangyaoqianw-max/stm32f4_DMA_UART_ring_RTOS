/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file impl_platform_bsp_gpio.c
 * @brief 当前板级 Platform BSP GPIO 物理资源绑定
 * @author YaoQian Wang
 * @date 2026-09-02
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_bsp_gpio.h"

#include "impl_platform_gpio.h"
#include "main.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Constants ********************************//
static impl_platform_gpio_context_t g_statusLedContext = {
    LED_OUT_GPIO_Port,
    LED_OUT_Pin
};

static impl_platform_gpio_context_t g_userKeyContext = {
    KEY_IN_GPIO_Port,
    KEY_IN_Pin
};

static impl_platform_gpio_context_t g_softI2cSclContext = {
    I2C_SCL_GPIO_Port,
    I2C_SCL_Pin
};

static impl_platform_gpio_context_t g_softI2cSdaContext = {
    I2C_SDA_GPIO_Port,
    I2C_SDA_Pin
};

static impl_platform_gpio_context_t g_lcdCsContext = {
    LCD_CS_GPIO_Port,
    LCD_CS_Pin
};

static impl_platform_gpio_context_t g_lcdDcContext = {
    LCD_DC_GPIO_Port,
    LCD_DC_Pin
};

static impl_platform_gpio_context_t g_lcdResetContext = {
    LCD_RST_GPIO_Port,
    LCD_RST_Pin
};

static impl_platform_gpio_context_t g_lcdBacklightContext = {
    LCD_BL_GPIO_Port,
    LCD_BL_Pin
};
//******************************** Constants ********************************//

//******************************** Functions *********************************//
platform_error_t platform_bsp_gpio_construct_status_led(
    platform_gpio_t *gpio)
{
    if (gpio == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return impl_platform_gpio_construct(gpio,
                                        "status_led_gpio",
                                        &g_statusLedContext);
}

platform_error_t platform_bsp_gpio_construct_user_key(
    platform_gpio_t *gpio)
{
    if (gpio == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return impl_platform_gpio_construct(gpio,
                                        "user_key_gpio",
                                        &g_userKeyContext);
}

platform_error_t platform_bsp_gpio_construct_soft_i2c_scl(
    platform_gpio_t *gpio)
{
    if (gpio == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return impl_platform_gpio_construct(gpio,
                                        "soft_i2c_scl_gpio",
                                        &g_softI2cSclContext);
}

platform_error_t platform_bsp_gpio_construct_soft_i2c_sda(
    platform_gpio_t *gpio)
{
    if (gpio == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return impl_platform_gpio_construct(gpio,
                                        "soft_i2c_sda_gpio",
                                        &g_softI2cSdaContext);
}

platform_error_t platform_bsp_gpio_construct_lcd_cs(
    platform_gpio_t *gpio)
{
    if (gpio == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return impl_platform_gpio_construct(gpio,
                                        "lcd_cs_gpio",
                                        &g_lcdCsContext);
}

platform_error_t platform_bsp_gpio_construct_lcd_dc(
    platform_gpio_t *gpio)
{
    if (gpio == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return impl_platform_gpio_construct(gpio,
                                        "lcd_dc_gpio",
                                        &g_lcdDcContext);
}

platform_error_t platform_bsp_gpio_construct_lcd_reset(
    platform_gpio_t *gpio)
{
    if (gpio == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return impl_platform_gpio_construct(gpio,
                                        "lcd_reset_gpio",
                                        &g_lcdResetContext);
}

platform_error_t platform_bsp_gpio_construct_lcd_backlight(
    platform_gpio_t *gpio)
{
    if (gpio == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return impl_platform_gpio_construct(gpio,
                                        "lcd_backlight_gpio",
                                        &g_lcdBacklightContext);
}
//******************************** Functions *********************************//
