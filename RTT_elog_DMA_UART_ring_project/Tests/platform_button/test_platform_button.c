/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_button.c
 * @brief 验证 Platform Button 轻量对象契约
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "button/platform_button.h"
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
    platform_error_t readResult;
    platform_error_t deinitResult;
    platform_gpio_config_t configuredValue;
    platform_gpio_level_t readLevel;
    uint32_t configureCallCount;
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
    NULL,
    fake_read,
    fake_deinit
};

static platform_error_t prepare_bound_button(
    platform_button_t *button,
    fake_gpio_context_t *context,
    platform_gpio_level_t activeLevel,
    platform_gpio_pull_t pull)
{
    platform_gpio_init_params_t gpioParams = {
        "button-gpio",
        &g_fakeGpioOps,
        context
    };

    button->activeLevel = activeLevel;
    button->pull = pull;

    return platform_gpio_init(&button->gpio, &gpioParams);
}

static int test_operations_reject_null_button(void)
{
    platform_button_state_t state = PLATFORM_BUTTON_STATE_RELEASED;

    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_button_init(NULL));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_button_read(NULL, &state));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_button_deinit(NULL));

    return 0;
}

static int test_read_rejects_null_state(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    fake_gpio_context_t context = {0};

    TEST_ASSERT(PLATFORM_ERR_OK == prepare_bound_button(
                &button,
                &context,
                PLATFORM_GPIO_LEVEL_LOW,
                PLATFORM_GPIO_PULL_UP));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_button_init(&button));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_button_read(&button, NULL));

    return 0;
}

static int test_zero_initialized_object_rejects_unbound_lifecycle(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    platform_button_state_t state = PLATFORM_BUTTON_STATE_RELEASED;

    TEST_ASSERT(0U == button.gpio.initialized);
    TEST_ASSERT(0U == button.gpio.configured);
    TEST_ASSERT(0U == button.initialized);
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == platform_button_init(&button));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == platform_button_read(&button, &state));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == platform_button_deinit(&button));

    return 0;
}

static int test_init_rejects_invalid_active_level_and_pull(void)
{
    platform_button_t invalidLevelButton = PLATFORM_BUTTON_INITIALIZER;
    platform_button_t invalidPullButton = PLATFORM_BUTTON_INITIALIZER;
    fake_gpio_context_t invalidLevelContext = {0};
    fake_gpio_context_t invalidPullContext = {0};

    TEST_ASSERT(PLATFORM_ERR_OK == prepare_bound_button(
                &invalidLevelButton,
                &invalidLevelContext,
                PLATFORM_GPIO_LEVEL_MAX,
                PLATFORM_GPIO_PULL_UP));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_button_init(&invalidLevelButton));
    TEST_ASSERT(0U == invalidLevelContext.configureCallCount);

    TEST_ASSERT(PLATFORM_ERR_OK == prepare_bound_button(
                &invalidPullButton,
                &invalidPullContext,
                PLATFORM_GPIO_LEVEL_LOW,
                PLATFORM_GPIO_PULL_MAX));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_button_init(&invalidPullButton));
    TEST_ASSERT(0U == invalidPullContext.configureCallCount);

    return 0;
}

static int test_init_configures_input_with_requested_pull(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    fake_gpio_context_t context = {0};

    TEST_ASSERT(PLATFORM_ERR_OK == prepare_bound_button(
                &button,
                &context,
                PLATFORM_GPIO_LEVEL_LOW,
                PLATFORM_GPIO_PULL_UP));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_button_init(&button));
    TEST_ASSERT(1U == context.configureCallCount);
    TEST_ASSERT(PLATFORM_GPIO_DIRECTION_INPUT ==
                context.configuredValue.direction);
    TEST_ASSERT(PLATFORM_GPIO_PULL_UP == context.configuredValue.pull);
    TEST_ASSERT(PLATFORM_GPIO_OUTPUT_PUSH_PULL ==
                context.configuredValue.outputType);
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_LOW == context.configuredValue.initialLevel);
    TEST_ASSERT(1U == button.initialized);
    TEST_ASSERT(1U == button.gpio.configured);

    return 0;
}

static int test_read_maps_active_low_levels(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    fake_gpio_context_t context = {0};
    platform_button_state_t state = PLATFORM_BUTTON_STATE_RELEASED;

    TEST_ASSERT(PLATFORM_ERR_OK == prepare_bound_button(
                &button,
                &context,
                PLATFORM_GPIO_LEVEL_LOW,
                PLATFORM_GPIO_PULL_UP));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_button_init(&button));

    context.readLevel = PLATFORM_GPIO_LEVEL_LOW;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_button_read(&button, &state));
    TEST_ASSERT(PLATFORM_BUTTON_STATE_PRESSED == state);

    context.readLevel = PLATFORM_GPIO_LEVEL_HIGH;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_button_read(&button, &state));
    TEST_ASSERT(PLATFORM_BUTTON_STATE_RELEASED == state);
    TEST_ASSERT(2U == context.readCallCount);

    return 0;
}

static int test_read_maps_active_high_levels(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    fake_gpio_context_t context = {0};
    platform_button_state_t state = PLATFORM_BUTTON_STATE_RELEASED;

    TEST_ASSERT(PLATFORM_ERR_OK == prepare_bound_button(
                &button,
                &context,
                PLATFORM_GPIO_LEVEL_HIGH,
                PLATFORM_GPIO_PULL_DOWN));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_button_init(&button));

    context.readLevel = PLATFORM_GPIO_LEVEL_HIGH;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_button_read(&button, &state));
    TEST_ASSERT(PLATFORM_BUTTON_STATE_PRESSED == state);

    context.readLevel = PLATFORM_GPIO_LEVEL_LOW;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_button_read(&button, &state));
    TEST_ASSERT(PLATFORM_BUTTON_STATE_RELEASED == state);

    return 0;
}

static int test_read_before_button_init_is_rejected(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    fake_gpio_context_t context = {0};
    platform_button_state_t state = PLATFORM_BUTTON_STATE_RELEASED;

    TEST_ASSERT(PLATFORM_ERR_OK == prepare_bound_button(
                &button,
                &context,
                PLATFORM_GPIO_LEVEL_LOW,
                PLATFORM_GPIO_PULL_UP));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                platform_button_read(&button, &state));

    return 0;
}

static int test_operations_propagate_gpio_errors(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    fake_gpio_context_t context = {0};
    platform_button_state_t state = PLATFORM_BUTTON_STATE_RELEASED;

    context.configureResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_OK == prepare_bound_button(
                &button,
                &context,
                PLATFORM_GPIO_LEVEL_LOW,
                PLATFORM_GPIO_PULL_UP));
    TEST_ASSERT(PLATFORM_ERR_IO == platform_button_init(&button));
    TEST_ASSERT(0U == button.initialized);

    context.configureResult = PLATFORM_ERR_OK;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_button_init(&button));

    context.readResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == platform_button_read(&button, &state));

    context.deinitResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == platform_button_deinit(&button));
    TEST_ASSERT(1U == button.initialized);

    return 0;
}

static int test_successful_deinit_clears_button_and_preserves_binding(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    fake_gpio_context_t context = {0};
    const platform_gpio_ops_t *ops = NULL;
    void *implContext = NULL;

    TEST_ASSERT(PLATFORM_ERR_OK == prepare_bound_button(
                &button,
                &context,
                PLATFORM_GPIO_LEVEL_LOW,
                PLATFORM_GPIO_PULL_UP));
    ops = button.gpio.ops;
    implContext = button.gpio.implContext;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_button_init(&button));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_button_deinit(&button));
    TEST_ASSERT(1U == context.deinitCallCount);
    TEST_ASSERT(0U == button.initialized);
    TEST_ASSERT(0U == button.gpio.configured);
    TEST_ASSERT(1U == button.gpio.initialized);
    TEST_ASSERT(ops == button.gpio.ops);
    TEST_ASSERT(implContext == button.gpio.implContext);

    return 0;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
int main(void)
{
    int result = test_operations_reject_null_button();

    if (0 != result) {
        return result;
    }

    result = test_read_rejects_null_state();
    if (0 != result) {
        return result;
    }

    result = test_zero_initialized_object_rejects_unbound_lifecycle();
    if (0 != result) {
        return result;
    }

    result = test_init_rejects_invalid_active_level_and_pull();
    if (0 != result) {
        return result;
    }

    result = test_init_configures_input_with_requested_pull();
    if (0 != result) {
        return result;
    }

    result = test_read_maps_active_low_levels();
    if (0 != result) {
        return result;
    }

    result = test_read_maps_active_high_levels();
    if (0 != result) {
        return result;
    }

    result = test_read_before_button_init_is_rejected();
    if (0 != result) {
        return result;
    }

    result = test_operations_propagate_gpio_errors();
    if (0 != result) {
        return result;
    }

    return test_successful_deinit_clears_button_and_preserves_binding();
}
//******************************** Functions *********************************//
