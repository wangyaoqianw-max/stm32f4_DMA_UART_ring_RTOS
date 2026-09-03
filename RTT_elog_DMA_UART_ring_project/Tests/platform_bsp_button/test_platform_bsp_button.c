/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_bsp_button.c
 * @brief 验证 Platform BSP User Key Button 装配行为
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <string.h>

#include "button/platform_bsp_button.h"
#include "project_config.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            return __LINE__; \
        } \
    } while (0)
//******************************** Defines *********************************//

//******************************** Types ***********************************//
typedef struct
{
    uint32_t callCount;
    platform_gpio_t *gpio;
    platform_error_t result;
} fake_gpio_constructor_record_t;
//******************************** Types ***********************************//

//******************************** Variables ********************************//
static fake_gpio_constructor_record_t g_fakeGpioConstructor;
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static void fake_reset(void)
{
    memset(&g_fakeGpioConstructor, 0, sizeof(g_fakeGpioConstructor));
    g_fakeGpioConstructor.result = PLATFORM_ERR_OK;
}

static int test_construct_binds_user_key_gpio_and_configuration(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;

    fake_reset();
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_bsp_button_construct_user_key(&button));
    TEST_ASSERT(1U == g_fakeGpioConstructor.callCount);
    TEST_ASSERT(&button.gpio == g_fakeGpioConstructor.gpio);
    TEST_ASSERT(PROJECT_USER_KEY_ACTIVE_LEVEL == button.activeLevel);
    TEST_ASSERT(PROJECT_USER_KEY_PULL == button.pull);
    TEST_ASSERT(1U == button.gpio.initialized);
    TEST_ASSERT(0U == button.gpio.configured);
    TEST_ASSERT(0U == button.initialized);

    return 0;
}

static int test_construct_rejects_null_button_without_gpio_binding(void)
{
    fake_reset();
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER ==
                platform_bsp_button_construct_user_key(NULL));
    TEST_ASSERT(0U == g_fakeGpioConstructor.callCount);

    return 0;
}

static int test_construct_propagates_gpio_constructor_error(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;

    fake_reset();
    g_fakeGpioConstructor.result = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO ==
                platform_bsp_button_construct_user_key(&button));
    TEST_ASSERT(1U == g_fakeGpioConstructor.callCount);
    TEST_ASSERT(0U == button.initialized);

    return 0;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t platform_bsp_gpio_construct_user_key(platform_gpio_t *gpio)
{
    g_fakeGpioConstructor.callCount++;
    g_fakeGpioConstructor.gpio = gpio;
    gpio->initialized = 1U;
    gpio->configured = 0U;

    return g_fakeGpioConstructor.result;
}

int main(void)
{
    int result = test_construct_binds_user_key_gpio_and_configuration();

    if (0 != result) {
        return result;
    }

    result = test_construct_rejects_null_button_without_gpio_binding();
    if (0 != result) {
        return result;
    }

    return test_construct_propagates_gpio_constructor_error();
}
//******************************** Functions *********************************//
