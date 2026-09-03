/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_bsp_led.c
 * @brief 验证 Platform BSP Status LED 装配行为
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <string.h>

#include "platform_bsp_led.h"
#include "project_config.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define TEST_ASSERT(condition)       \
    do {                             \
        if (!(condition)) {          \
            return __LINE__;         \
        }                            \
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
static uint32_t g_ledInitCallCount;
static uint32_t g_ledOnCallCount;
static uint32_t g_ledOffCallCount;
static uint32_t g_ledToggleCallCount;
static uint32_t g_ledDeinitCallCount;
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static void fake_reset(void)
{
    memset(&g_fakeGpioConstructor, 0, sizeof(g_fakeGpioConstructor));
    g_fakeGpioConstructor.result = PLATFORM_ERR_OK;
    g_ledInitCallCount = 0U;
    g_ledOnCallCount = 0U;
    g_ledOffCallCount = 0U;
    g_ledToggleCallCount = 0U;
    g_ledDeinitCallCount = 0U;
}

static int test_construct_binds_status_led_gpio_and_active_level(void)
{
    platform_led_t led = PLATFORM_LED_INITIALIZER;

    fake_reset();
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_bsp_led_construct_status_led(&led));
    TEST_ASSERT(1U == g_fakeGpioConstructor.callCount);
    TEST_ASSERT(&led.gpio == g_fakeGpioConstructor.gpio);
    TEST_ASSERT(PROJECT_STATUS_LED_ACTIVE_LEVEL == led.activeLevel);
    TEST_ASSERT(0U == led.initialized);

    return 0;
}

static int test_construct_rejects_null_led_without_gpio_binding(void)
{
    fake_reset();
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_bsp_led_construct_status_led(NULL));
    TEST_ASSERT(0U == g_fakeGpioConstructor.callCount);

    return 0;
}

static int test_construct_propagates_gpio_constructor_error(void)
{
    platform_led_t led = PLATFORM_LED_INITIALIZER;

    fake_reset();
    g_fakeGpioConstructor.result = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO ==
                platform_bsp_led_construct_status_led(&led));
    TEST_ASSERT(1U == g_fakeGpioConstructor.callCount);
    TEST_ASSERT(PROJECT_STATUS_LED_ACTIVE_LEVEL == led.activeLevel);
    TEST_ASSERT(0U == led.initialized);

    return 0;
}

static int test_construct_performs_no_led_lifecycle_or_product_behavior(void)
{
    platform_led_t led = PLATFORM_LED_INITIALIZER;

    fake_reset();
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_bsp_led_construct_status_led(&led));
    TEST_ASSERT(0U == g_ledInitCallCount);
    TEST_ASSERT(0U == g_ledOnCallCount);
    TEST_ASSERT(0U == g_ledOffCallCount);
    TEST_ASSERT(0U == g_ledToggleCallCount);
    TEST_ASSERT(0U == g_ledDeinitCallCount);

    return 0;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t platform_bsp_gpio_construct_status_led(
    platform_gpio_t *gpio)
{
    g_fakeGpioConstructor.callCount++;
    g_fakeGpioConstructor.gpio = gpio;

    return g_fakeGpioConstructor.result;
}

platform_error_t platform_led_init(platform_led_t *led)
{
    (void)led;
    g_ledInitCallCount++;

    return PLATFORM_ERR_OK;
}

platform_error_t platform_led_on(platform_led_t *led)
{
    (void)led;
    g_ledOnCallCount++;

    return PLATFORM_ERR_OK;
}

platform_error_t platform_led_off(platform_led_t *led)
{
    (void)led;
    g_ledOffCallCount++;

    return PLATFORM_ERR_OK;
}

platform_error_t platform_led_toggle(platform_led_t *led)
{
    (void)led;
    g_ledToggleCallCount++;

    return PLATFORM_ERR_OK;
}

platform_error_t platform_led_deinit(platform_led_t *led)
{
    (void)led;
    g_ledDeinitCallCount++;

    return PLATFORM_ERR_OK;
}

int main(void)
{
    int result = test_construct_binds_status_led_gpio_and_active_level();

    if (0 != result) {
        return result;
    }

    result = test_construct_rejects_null_led_without_gpio_binding();
    if (0 != result) {
        return result;
    }

    result = test_construct_propagates_gpio_constructor_error();
    if (0 != result) {
        return result;
    }

    return test_construct_performs_no_led_lifecycle_or_product_behavior();
}
//******************************** Functions *********************************//
