/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_impl_platform_gpio.c
 * @brief 验证 STM32 GPIO Impl Context 绑定和构造行为
 * @author Codex
 * @date 2026-09-01
 * @version V1.0
 *
 *****************************************************************************/

#include <stddef.h>

#include "impl_platform_gpio.h"

#define TEST_ASSERT(condition)       \
    do {                             \
        if (!(condition)) {          \
            return __LINE__;         \
        }                            \
    } while (0)

GPIO_TypeDef g_fakePort;
uint32_t g_fakeHalInitCount;
uint32_t g_fakeHalWriteCount;
uint32_t g_fakeHalReadCount;
uint32_t g_fakeHalDeinitCount;

void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init)
{
    (void)GPIOx;
    (void)GPIO_Init;
    g_fakeHalInitCount++;
}

void HAL_GPIO_DeInit(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin)
{
    (void)GPIOx;
    (void)GPIO_Pin;
    g_fakeHalDeinitCount++;
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    (void)GPIOx;
    (void)GPIO_Pin;
    g_fakeHalReadCount++;

    return GPIO_PIN_RESET;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx,
                       uint16_t GPIO_Pin,
                       GPIO_PinState PinState)
{
    (void)GPIOx;
    (void)GPIO_Pin;
    (void)PinState;
    g_fakeHalWriteCount++;
}

#include "../../04_Impl/impl_mcu/impl_platform_gpio.c"

static void reset_fake_hal(void)
{
    g_fakeHalInitCount = 0U;
    g_fakeHalWriteCount = 0U;
    g_fakeHalReadCount = 0U;
    g_fakeHalDeinitCount = 0U;
}

static int test_construct_rejects_invalid_contexts(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;
    impl_platform_gpio_context_t context = {
        &g_fakePort,
        GPIO_PIN_3
    };

    reset_fake_hal();
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                impl_platform_gpio_construct(NULL, "test_gpio", &context));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                impl_platform_gpio_construct(&gpio, "test_gpio", NULL));

    context.port = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                impl_platform_gpio_construct(&gpio, "test_gpio", &context));

    context.port = &g_fakePort;
    context.pin = 0U;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                impl_platform_gpio_construct(&gpio, "test_gpio", &context));

    context.pin = (uint16_t)(GPIO_PIN_0 | GPIO_PIN_1);
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                impl_platform_gpio_construct(&gpio, "test_gpio", &context));
    TEST_ASSERT(0U == gpio.initialized);

    return 0;
}

static int test_construct_binds_valid_single_pin_without_hal_operation(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;
    impl_platform_gpio_context_t context = {
        &g_fakePort,
        GPIO_PIN_3
    };

    reset_fake_hal();
    TEST_ASSERT(PLATFORM_ERR_OK ==
                impl_platform_gpio_construct(&gpio, "test_gpio", &context));
    TEST_ASSERT(1U == gpio.initialized);
    TEST_ASSERT(0U == gpio.configured);
    TEST_ASSERT(&context == gpio.implContext);
    TEST_ASSERT(0U == g_fakeHalInitCount);
    TEST_ASSERT(0U == g_fakeHalWriteCount);
    TEST_ASSERT(0U == g_fakeHalReadCount);
    TEST_ASSERT(0U == g_fakeHalDeinitCount);

    return 0;
}

static int test_construct_accepts_highest_single_pin_and_rejects_masks(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;
    impl_platform_gpio_context_t context = {
        &g_fakePort,
        GPIO_PIN_15
    };

    TEST_ASSERT(PLATFORM_ERR_OK ==
                impl_platform_gpio_construct(&gpio, "test_gpio", &context));

    gpio = (platform_gpio_t)PLATFORM_GPIO_INITIALIZER;
    context.pin = GPIO_PIN_All;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                impl_platform_gpio_construct(&gpio, "test_gpio", &context));

    return 0;
}

int main(void)
{
    int result = test_construct_rejects_invalid_contexts();

    if (0 != result) {
        return result;
    }

    result = test_construct_binds_valid_single_pin_without_hal_operation();
    if (0 != result) {
        return result;
    }

    return test_construct_accepts_highest_single_pin_and_rejects_masks();
}
