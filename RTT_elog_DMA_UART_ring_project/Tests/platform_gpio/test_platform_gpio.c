/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_gpio.c
 * @brief 验证 Platform GPIO 对象构造契约
 * @author Codex
 * @date 2026-09-01
 * @version V1.0
 *
 *****************************************************************************/

#include <stddef.h>

#include "platform_gpio.h"

#define TEST_ASSERT(condition)       \
    do {                             \
        if (!(condition)) {          \
            return __LINE__;         \
        }                            \
    } while (0)

static const platform_gpio_ops_t g_emptyOps = {
    NULL,
    NULL,
    NULL,
    NULL
};

/**
 * @brief 验证 GPIO 对象初始化宏产生零状态
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_initializer_starts_zero(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;

    TEST_ASSERT(NULL == gpio.name);
    TEST_ASSERT(NULL == gpio.ops);
    TEST_ASSERT(NULL == gpio.implContext);
    TEST_ASSERT(0U == gpio.initialized);
    TEST_ASSERT(0U == gpio.configured);

    return 0;
}

/**
 * @brief 验证 GPIO 构造参数校验
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_init_rejects_invalid_parameters(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_init_params_t params = {
        "gpio-test",
        &g_emptyOps,
        NULL
    };

    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_gpio_init(NULL, &params));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_gpio_init(&gpio, NULL));

    params.ops = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_gpio_init(&gpio, &params));
    TEST_ASSERT(0U == gpio.initialized);
    TEST_ASSERT(0U == gpio.configured);

    return 0;
}

/**
 * @brief 验证首次构造绑定字段和轻量对象状态
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_init_binds_object(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;
    int context = 0;
    platform_gpio_init_params_t params = {
        "gpio-test",
        &g_emptyOps,
        &context
    };

    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_init(&gpio, &params));
    TEST_ASSERT(params.name == gpio.name);
    TEST_ASSERT(&g_emptyOps == gpio.ops);
    TEST_ASSERT(&context == gpio.implContext);
    TEST_ASSERT(1U == gpio.initialized);
    TEST_ASSERT(0U == gpio.configured);

    return 0;
}

/**
 * @brief 验证 GPIO 对象不允许重复构造且允许可选字段为空
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_init_rejects_duplicate_and_allows_optional_nulls(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_init_params_t params = {
        NULL,
        &g_emptyOps,
        NULL
    };
    const platform_gpio_ops_t *boundOps = params.ops;

    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_init(&gpio, &params));
    TEST_ASSERT(NULL == gpio.name);
    TEST_ASSERT(NULL == gpio.implContext);
    TEST_ASSERT(PLATFORM_ERR_ALREADY_INITIALIZED ==
                platform_gpio_init(&gpio, &params));
    TEST_ASSERT(boundOps == gpio.ops);
    TEST_ASSERT(1U == gpio.initialized);
    TEST_ASSERT(0U == gpio.configured);

    return 0;
}

/**
 * @brief 运行 Platform GPIO 对象构造测试
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
int main(void)
{
    int result = 0;

    result = test_initializer_starts_zero();
    if (0 != result) {
        return result;
    }

    result = test_init_rejects_invalid_parameters();
    if (0 != result) {
        return result;
    }

    result = test_init_binds_object();
    if (0 != result) {
        return result;
    }

    result = test_init_rejects_duplicate_and_allows_optional_nulls();
    if (0 != result) {
        return result;
    }

    return 0;
}
