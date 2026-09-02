/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_bsp_gpio.c
 * @brief 验证 Platform BSP GPIO 板级绑定行为
 * @author Codex
 * @date 2026-09-02
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <string.h>

#include "main.h"
#include "platform_bsp_gpio.h"
#include "impl_platform_gpio.h"
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
typedef platform_error_t (*platform_bsp_gpio_constructor_t)(
    platform_gpio_t *gpio);

typedef struct
{
    uint32_t callCount;
    platform_gpio_t *gpio;
    const char *name;
    impl_platform_gpio_context_t *context;
    GPIO_TypeDef *port;
    uint16_t pin;
    platform_error_t result;
} fake_constructor_record_t;
//******************************** Types ***********************************//

//******************************** Variables ********************************//
GPIO_TypeDef g_fakePortA;
GPIO_TypeDef g_fakePortB;
GPIO_TypeDef g_fakePortC;
static fake_constructor_record_t g_fakeConstructor;
static uint32_t g_platformConfigureCallCount;
static uint32_t g_platformWriteCallCount;
static uint32_t g_platformReadCallCount;
static uint32_t g_halInitCallCount;
static uint32_t g_halWriteCallCount;
static uint32_t g_halReadCallCount;
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static void fake_constructor_reset(void)
{
    memset(&g_fakeConstructor, 0, sizeof(g_fakeConstructor));
    g_fakeConstructor.result = PLATFORM_ERR_OK;
    g_platformConfigureCallCount = 0U;
    g_platformWriteCallCount = 0U;
    g_platformReadCallCount = 0U;
    g_halInitCallCount = 0U;
    g_halWriteCallCount = 0U;
    g_halReadCallCount = 0U;
}

static int test_constructor_forwards_binding(
    platform_bsp_gpio_constructor_t constructor,
    GPIO_TypeDef *expectedPort,
    uint16_t expectedPin,
    const char *expectedName)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;

    fake_constructor_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == constructor(&gpio));
    TEST_ASSERT(1U == g_fakeConstructor.callCount);
    TEST_ASSERT(&gpio == g_fakeConstructor.gpio);
    TEST_ASSERT(0 == strcmp(expectedName, g_fakeConstructor.name));
    TEST_ASSERT(g_fakeConstructor.context != NULL);
    TEST_ASSERT(expectedPort == g_fakeConstructor.port);
    TEST_ASSERT(expectedPin == g_fakeConstructor.pin);
    TEST_ASSERT(0U == g_platformConfigureCallCount);
    TEST_ASSERT(0U == g_platformWriteCallCount);
    TEST_ASSERT(0U == g_platformReadCallCount);
    TEST_ASSERT(0U == g_halInitCallCount);
    TEST_ASSERT(0U == g_halWriteCallCount);
    TEST_ASSERT(0U == g_halReadCallCount);

    return 0;
}

static int test_construct_rejects_null_gpio(void)
{
    platform_bsp_gpio_constructor_t constructors[] = {
        platform_bsp_gpio_construct_status_led,
        platform_bsp_gpio_construct_user_key,
        platform_bsp_gpio_construct_soft_i2c_scl,
        platform_bsp_gpio_construct_soft_i2c_sda
    };
    uint32_t index;

    for (index = 0U; index < 4U; index++) {
        fake_constructor_reset();
        TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == constructors[index](NULL));
        TEST_ASSERT(0U == g_fakeConstructor.callCount);
    }

    return 0;
}

static int test_construct_forwards_status_led_binding(void)
{
    return test_constructor_forwards_binding(
        platform_bsp_gpio_construct_status_led,
        LED_OUT_GPIO_Port,
        LED_OUT_Pin,
        "status_led_gpio");
}

static int test_construct_forwards_user_key_binding(void)
{
    return test_constructor_forwards_binding(
        platform_bsp_gpio_construct_user_key,
        KEY_IN_GPIO_Port,
        KEY_IN_Pin,
        "user_key_gpio");
}

static int test_construct_forwards_soft_i2c_scl_binding(void)
{
    return test_constructor_forwards_binding(
        platform_bsp_gpio_construct_soft_i2c_scl,
        I2C_SCL_GPIO_Port,
        I2C_SCL_Pin,
        "soft_i2c_scl_gpio");
}

static int test_construct_forwards_soft_i2c_sda_binding(void)
{
    return test_constructor_forwards_binding(
        platform_bsp_gpio_construct_soft_i2c_sda,
        I2C_SDA_GPIO_Port,
        I2C_SDA_Pin,
        "soft_i2c_sda_gpio");
}

static int test_construct_propagates_constructor_error(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;

    fake_constructor_reset();
    g_fakeConstructor.result = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO ==
                platform_bsp_gpio_construct_status_led(&gpio));
    TEST_ASSERT(1U == g_fakeConstructor.callCount);

    return 0;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t impl_platform_gpio_construct(
    platform_gpio_t *gpio,
    const char *name,
    impl_platform_gpio_context_t *context)
{
    g_fakeConstructor.callCount++;
    g_fakeConstructor.gpio = gpio;
    g_fakeConstructor.name = name;
    g_fakeConstructor.context = context;
    g_fakeConstructor.port = context->port;
    g_fakeConstructor.pin = context->pin;

    return g_fakeConstructor.result;
}

platform_error_t platform_gpio_configure(
    platform_gpio_t *gpio,
    const platform_gpio_config_t *config)
{
    (void)gpio;
    (void)config;
    g_platformConfigureCallCount++;

    return PLATFORM_ERR_OK;
}

platform_error_t platform_gpio_write(
    platform_gpio_t *gpio,
    platform_gpio_level_t level)
{
    (void)gpio;
    (void)level;
    g_platformWriteCallCount++;

    return PLATFORM_ERR_OK;
}

platform_error_t platform_gpio_read(
    platform_gpio_t *gpio,
    platform_gpio_level_t *level)
{
    (void)gpio;
    (void)level;
    g_platformReadCallCount++;

    return PLATFORM_ERR_OK;
}

void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init)
{
    (void)GPIOx;
    (void)GPIO_Init;
    g_halInitCallCount++;
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    (void)GPIOx;
    (void)GPIO_Pin;
    g_halReadCallCount++;

    return GPIO_PIN_RESET;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx,
                       uint16_t GPIO_Pin,
                       GPIO_PinState PinState)
{
    (void)GPIOx;
    (void)GPIO_Pin;
    (void)PinState;
    g_halWriteCallCount++;
}

int main(void)
{
    int result = test_construct_rejects_null_gpio();

    if (0 != result) {
        return result;
    }

    result = test_construct_forwards_status_led_binding();
    if (0 != result) {
        return result;
    }

    result = test_construct_forwards_user_key_binding();
    if (0 != result) {
        return result;
    }

    result = test_construct_forwards_soft_i2c_scl_binding();
    if (0 != result) {
        return result;
    }

    result = test_construct_forwards_soft_i2c_sda_binding();
    if (0 != result) {
        return result;
    }

    return test_construct_propagates_constructor_error();
}
//******************************** Functions *********************************//
