/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file board_gpio_smoke.c
 * @brief 目标板 GPIO 纵向 Smoke Test 临时实现
 * @author Codex
 * @date 2026-09-02
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "board_gpio_smoke.h"

#include "main.h"
#include "platform_bsp_gpio.h"
#include "service_log.h"

#include <stdio.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define LOG_TAG "gpio_smoke"
//******************************** Defines **********************************//

//******************************** Private Functions *************************//
static platform_error_t board_gpio_smoke_report_failure(
    const char *step,
    platform_error_t result)
{
    printf("GPIO_SMOKE,FAIL,step=%s,result=%d\n", step, (int)result);
    SERVICE_LOG_E("gpio smoke failed: %s, result=%d", step, (int)result);

    return result;
}

static void board_gpio_smoke_deinit(
    platform_gpio_t *gpio,
    const char *name)
{
    platform_error_t result = PLATFORM_ERR_OK;

    if ((gpio == NULL) || (gpio->configured == 0U)) {
        return;
    }

    result = platform_gpio_deinit(gpio);
    if (result != PLATFORM_ERR_OK) {
        printf("GPIO_SMOKE,DEINIT_FAIL,name=%s,result=%d\n",
               name,
               (int)result);
        SERVICE_LOG_E("gpio smoke deinit failed: %s, result=%d",
                      name,
                      (int)result);
    }
}

static void board_gpio_smoke_wait(const char *stage)
{
    printf("GPIO_SMOKE,WAIT,stage=%s\n", stage);
    SERVICE_LOG_I("gpio smoke wait: %s", stage);
    HAL_Delay(1000U);
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
void board_gpio_smoke_run(void)
{
    platform_gpio_t statusLedGpio = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t userKeyGpio = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t softI2cSclGpio = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t softI2cSdaGpio = PLATFORM_GPIO_INITIALIZER;
    const platform_gpio_config_t ledConfig = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_HIGH
    };
    const platform_gpio_config_t keyConfig = {
        PLATFORM_GPIO_DIRECTION_INPUT,
        PLATFORM_GPIO_PULL_UP,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_LOW
    };
    const platform_gpio_config_t softI2cConfig = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_OPEN_DRAIN,
        PLATFORM_GPIO_LEVEL_HIGH
    };
    platform_gpio_level_t level = PLATFORM_GPIO_LEVEL_LOW;
    platform_error_t result = PLATFORM_ERR_OK;

    printf("GPIO_SMOKE,START\n");
    SERVICE_LOG_I("gpio smoke start");

    result = platform_bsp_gpio_construct_status_led(&statusLedGpio);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("construct_status_led", result);
        return;
    }

    result = platform_bsp_gpio_construct_user_key(&userKeyGpio);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("construct_user_key", result);
        return;
    }

    result = platform_bsp_gpio_construct_soft_i2c_scl(&softI2cSclGpio);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("construct_soft_i2c_scl", result);
        return;
    }

    result = platform_bsp_gpio_construct_soft_i2c_sda(&softI2cSdaGpio);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("construct_soft_i2c_sda", result);
        return;
    }

    result = platform_gpio_configure(&statusLedGpio, &ledConfig);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("configure_status_led", result);
        goto cleanup;
    }

    result = platform_gpio_configure(&userKeyGpio, &keyConfig);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("configure_user_key", result);
        goto cleanup;
    }

    result = platform_gpio_configure(&softI2cSclGpio, &softI2cConfig);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("configure_soft_i2c_scl", result);
        goto cleanup;
    }

    result = platform_gpio_configure(&softI2cSdaGpio, &softI2cConfig);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("configure_soft_i2c_sda", result);
        goto cleanup;
    }

    printf("GPIO_SMOKE,CONFIGURED\n");
    SERVICE_LOG_I("gpio smoke configured");

    result = platform_gpio_write(&statusLedGpio, PLATFORM_GPIO_LEVEL_LOW);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("status_led_low", result);
        goto cleanup;
    }

    printf("GPIO_SMOKE,LED,level=LOW,expected=ON\n");
    SERVICE_LOG_I("gpio smoke LED low, expected on");
    board_gpio_smoke_wait("status_led_low");

    result = platform_gpio_write(&statusLedGpio, PLATFORM_GPIO_LEVEL_HIGH);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("status_led_high", result);
        goto cleanup;
    }

    printf("GPIO_SMOKE,LED,level=HIGH,expected=OFF\n");
    SERVICE_LOG_I("gpio smoke LED high, expected off");
    board_gpio_smoke_wait("status_led_high");

    printf("GPIO_SMOKE,KEY,action=RELEASE,expected=HIGH\n");
    SERVICE_LOG_I("gpio smoke key release now, expected high");
    board_gpio_smoke_wait("key_release");
    result = platform_gpio_read(&userKeyGpio, &level);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("user_key_release_read", result);
        goto cleanup;
    }

    printf("GPIO_SMOKE,KEY,action=RELEASE,read=%s,expected=HIGH\n",
           (level == PLATFORM_GPIO_LEVEL_HIGH) ? "HIGH" : "LOW");
    SERVICE_LOG_I("gpio smoke key release read=%s",
                  (level == PLATFORM_GPIO_LEVEL_HIGH) ? "HIGH" : "LOW");

    printf("GPIO_SMOKE,KEY,action=PRESS,expected=LOW\n");
    SERVICE_LOG_I("gpio smoke key press now, expected low");
    board_gpio_smoke_wait("key_press");
    result = platform_gpio_read(&userKeyGpio, &level);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("user_key_press_read", result);
        goto cleanup;
    }

    printf("GPIO_SMOKE,KEY,action=PRESS,read=%s,expected=LOW\n",
           (level == PLATFORM_GPIO_LEVEL_HIGH) ? "HIGH" : "LOW");
    SERVICE_LOG_I("gpio smoke key press read=%s",
                  (level == PLATFORM_GPIO_LEVEL_HIGH) ? "HIGH" : "LOW");

    result = platform_gpio_write(&softI2cSclGpio, PLATFORM_GPIO_LEVEL_LOW);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("soft_i2c_scl_low", result);
        goto cleanup;
    }

    printf("GPIO_SMOKE,SCL,drive=LOW,expected=BUS_LOW\n");
    SERVICE_LOG_I("gpio smoke SCL low, expected bus low");
    board_gpio_smoke_wait("soft_i2c_scl_low");

    result = platform_gpio_write(&softI2cSclGpio, PLATFORM_GPIO_LEVEL_HIGH);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("soft_i2c_scl_release", result);
        goto cleanup;
    }

    printf("GPIO_SMOKE,SCL,drive=RELEASE,expected=EXTERNAL_PULLUP_HIGH\n");
    SERVICE_LOG_I("gpio smoke SCL release, expected external pull-up high");
    board_gpio_smoke_wait("soft_i2c_scl_release");

    result = platform_gpio_write(&softI2cSdaGpio, PLATFORM_GPIO_LEVEL_LOW);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("soft_i2c_sda_low", result);
        goto cleanup;
    }

    result = platform_gpio_read(&softI2cSdaGpio, &level);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("soft_i2c_sda_low_read", result);
        goto cleanup;
    }

    printf("GPIO_SMOKE,SDA,drive=LOW,read=%s,expected=LOW\n",
           (level == PLATFORM_GPIO_LEVEL_HIGH) ? "HIGH" : "LOW");
    SERVICE_LOG_I("gpio smoke SDA low read=%s",
                  (level == PLATFORM_GPIO_LEVEL_HIGH) ? "HIGH" : "LOW");
    board_gpio_smoke_wait("soft_i2c_sda_low");

    result = platform_gpio_write(&softI2cSdaGpio, PLATFORM_GPIO_LEVEL_HIGH);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("soft_i2c_sda_release", result);
        goto cleanup;
    }

    result = platform_gpio_read(&softI2cSdaGpio, &level);
    if (result != PLATFORM_ERR_OK) {
        (void)board_gpio_smoke_report_failure("soft_i2c_sda_release_read", result);
        goto cleanup;
    }

    printf("GPIO_SMOKE,SDA,drive=RELEASE,read=%s,expected=HIGH\n",
           (level == PLATFORM_GPIO_LEVEL_HIGH) ? "HIGH" : "LOW");
    SERVICE_LOG_I("gpio smoke SDA release read=%s",
                  (level == PLATFORM_GPIO_LEVEL_HIGH) ? "HIGH" : "LOW");
    board_gpio_smoke_wait("soft_i2c_sda_release");

    printf("GPIO_SMOKE,END,MANUAL_VERIFICATION_PENDING\n");
    SERVICE_LOG_I("gpio smoke end, manual verification pending");

cleanup:
    board_gpio_smoke_deinit(&softI2cSdaGpio, "soft_i2c_sda");
    board_gpio_smoke_deinit(&softI2cSclGpio, "soft_i2c_scl");
    board_gpio_smoke_deinit(&userKeyGpio, "user_key");
    board_gpio_smoke_deinit(&statusLedGpio, "status_led");
}
//******************************** Functions *********************************//
