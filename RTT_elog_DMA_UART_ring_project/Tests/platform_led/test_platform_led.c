/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_led.c
 * @brief 验证 Platform LED 轻量对象契约
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "platform_led.h"
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
    platform_error_t configureResult;
    platform_error_t writeResult;
    platform_error_t readResult;
    platform_error_t deinitResult;
    platform_gpio_config_t configuredValue;
    platform_gpio_level_t writtenLevel;
    platform_gpio_level_t readLevel;
    uint32_t configureCallCount;
    uint32_t writeCallCount;
    uint32_t readCallCount;
    uint32_t deinitCallCount;
} fake_gpio_context_t;
//******************************** Types ***********************************//

//******************************** Private Functions *************************//
static platform_error_t fake_configure(
    platform_gpio_t *gpio,
    const platform_gpio_config_t *config)
{
    fake_gpio_context_t *context = (fake_gpio_context_t *)gpio->implContext;

    context->configuredValue = *config;
    context->configureCallCount++;

    return context->configureResult;
}

static platform_error_t fake_write(
    platform_gpio_t *gpio,
    platform_gpio_level_t level)
{
    fake_gpio_context_t *context = (fake_gpio_context_t *)gpio->implContext;

    context->writtenLevel = level;
    context->writeCallCount++;

    return context->writeResult;
}

static platform_error_t fake_read(
    platform_gpio_t *gpio,
    platform_gpio_level_t *level)
{
    fake_gpio_context_t *context = (fake_gpio_context_t *)gpio->implContext;

    *level = context->readLevel;
    context->readCallCount++;

    return context->readResult;
}

static platform_error_t fake_deinit(platform_gpio_t *gpio)
{
    fake_gpio_context_t *context = (fake_gpio_context_t *)gpio->implContext;

    context->deinitCallCount++;

    return context->deinitResult;
}

static const platform_gpio_ops_t g_fakeGpioOps = {
    fake_configure,
    fake_write,
    fake_read,
    fake_deinit
};

static platform_error_t prepare_bound_led(
    platform_led_t *led,
    fake_gpio_context_t *context,
    platform_gpio_level_t activeLevel)
{
    platform_gpio_init_params_t gpioParams = {
        "led-gpio",
        &g_fakeGpioOps,
        context
    };

    led->activeLevel = activeLevel;

    return platform_gpio_init(&led->gpio, &gpioParams);
}

static int test_zero_initialized_object_rejects_unbound_lifecycle(void)
{
    platform_led_t led = PLATFORM_LED_INITIALIZER;

    TEST_ASSERT(0U == led.gpio.initialized);
    TEST_ASSERT(0U == led.gpio.configured);
    TEST_ASSERT(0U == led.initialized);
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == platform_led_init(&led));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == platform_led_on(&led));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == platform_led_off(&led));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == platform_led_toggle(&led));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == platform_led_deinit(&led));

    return 0;
}

static int test_operations_reject_null_led(void)
{
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_led_init(NULL));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_led_on(NULL));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_led_off(NULL));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_led_toggle(NULL));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_led_deinit(NULL));

    return 0;
}

static int test_init_configures_output_and_establishes_off(void)
{
    platform_led_t led = PLATFORM_LED_INITIALIZER;
    fake_gpio_context_t context = {0};

    TEST_ASSERT(PLATFORM_ERR_OK ==
                prepare_bound_led(&led, &context, PLATFORM_GPIO_LEVEL_LOW));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_led_init(&led));
    TEST_ASSERT(1U == context.configureCallCount);
    TEST_ASSERT(PLATFORM_GPIO_DIRECTION_OUTPUT ==
                context.configuredValue.direction);
    TEST_ASSERT(PLATFORM_GPIO_PULL_NONE == context.configuredValue.pull);
    TEST_ASSERT(PLATFORM_GPIO_OUTPUT_PUSH_PULL ==
                context.configuredValue.outputType);
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_HIGH ==
                context.configuredValue.initialLevel);
    TEST_ASSERT(1U == led.initialized);
    TEST_ASSERT(1U == led.gpio.configured);

    return 0;
}

static int test_active_low_on_and_off_map_to_physical_levels(void)
{
    platform_led_t led = PLATFORM_LED_INITIALIZER;
    fake_gpio_context_t context = {0};

    TEST_ASSERT(PLATFORM_ERR_OK ==
                prepare_bound_led(&led, &context, PLATFORM_GPIO_LEVEL_LOW));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_led_init(&led));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_led_on(&led));
    TEST_ASSERT(1U == context.writeCallCount);
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_LOW == context.writtenLevel);
    TEST_ASSERT(PLATFORM_ERR_OK == platform_led_off(&led));
    TEST_ASSERT(2U == context.writeCallCount);
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_HIGH == context.writtenLevel);

    return 0;
}

static int test_toggle_changes_physical_output_from_readback(void)
{
    platform_led_t led = PLATFORM_LED_INITIALIZER;
    fake_gpio_context_t context = {0};

    TEST_ASSERT(PLATFORM_ERR_OK ==
                prepare_bound_led(&led, &context, PLATFORM_GPIO_LEVEL_LOW));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_led_init(&led));

    context.readLevel = PLATFORM_GPIO_LEVEL_HIGH;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_led_toggle(&led));
    TEST_ASSERT(1U == context.readCallCount);
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_LOW == context.writtenLevel);

    context.readLevel = PLATFORM_GPIO_LEVEL_LOW;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_led_toggle(&led));
    TEST_ASSERT(2U == context.readCallCount);
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_HIGH == context.writtenLevel);

    return 0;
}

static int test_init_and_operations_propagate_gpio_errors(void)
{
    platform_led_t led = PLATFORM_LED_INITIALIZER;
    fake_gpio_context_t context = {0};

    context.configureResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_OK ==
                prepare_bound_led(&led, &context, PLATFORM_GPIO_LEVEL_LOW));
    TEST_ASSERT(PLATFORM_ERR_IO == platform_led_init(&led));
    TEST_ASSERT(0U == led.initialized);

    context.configureResult = PLATFORM_ERR_OK;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_led_init(&led));

    context.writeResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == platform_led_on(&led));

    context.readResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == platform_led_toggle(&led));

    context.deinitResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == platform_led_deinit(&led));
    TEST_ASSERT(1U == led.initialized);

    return 0;
}

static int test_deinit_clears_led_hardware_state(void)
{
    platform_led_t led = PLATFORM_LED_INITIALIZER;
    fake_gpio_context_t context = {0};

    TEST_ASSERT(PLATFORM_ERR_OK ==
                prepare_bound_led(&led, &context, PLATFORM_GPIO_LEVEL_LOW));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_led_init(&led));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_led_deinit(&led));
    TEST_ASSERT(1U == context.deinitCallCount);
    TEST_ASSERT(0U == led.initialized);
    TEST_ASSERT(0U == led.gpio.configured);
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == platform_led_off(&led));

    return 0;
}

static int test_init_rejects_invalid_active_level_and_duplicate_init(void)
{
    platform_led_t invalidLed = PLATFORM_LED_INITIALIZER;
    platform_led_t led = PLATFORM_LED_INITIALIZER;
    fake_gpio_context_t invalidContext = {0};
    fake_gpio_context_t context = {0};

    TEST_ASSERT(PLATFORM_ERR_OK == prepare_bound_led(
                &invalidLed,
                &invalidContext,
                PLATFORM_GPIO_LEVEL_MAX));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_led_init(&invalidLed));
    TEST_ASSERT(0U == invalidContext.configureCallCount);

    TEST_ASSERT(PLATFORM_ERR_OK ==
                prepare_bound_led(&led, &context, PLATFORM_GPIO_LEVEL_HIGH));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_led_init(&led));
    TEST_ASSERT(PLATFORM_ERR_ALREADY_INITIALIZED == platform_led_init(&led));

    return 0;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
int main(void)
{
    int result = test_zero_initialized_object_rejects_unbound_lifecycle();

    if (0 != result) {
        return result;
    }

    result = test_operations_reject_null_led();
    if (0 != result) {
        return result;
    }

    result = test_init_configures_output_and_establishes_off();
    if (0 != result) {
        return result;
    }

    result = test_active_low_on_and_off_map_to_physical_levels();
    if (0 != result) {
        return result;
    }

    result = test_toggle_changes_physical_output_from_readback();
    if (0 != result) {
        return result;
    }

    result = test_init_and_operations_propagate_gpio_errors();
    if (0 != result) {
        return result;
    }

    result = test_deinit_clears_led_hardware_state();
    if (0 != result) {
        return result;
    }

    return test_init_rejects_invalid_active_level_and_duplicate_init();
}
//******************************** Functions *********************************//
