/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_i2c.c
 * @brief 验证 Platform I2C 公共参数与对象状态合同
 * @author Codex
 * @date 2026-09-02
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "platform_i2c.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define TEST_ASSERT(condition)       \
    do {                             \
        if (!(condition)) {          \
            return __LINE__;         \
        }                            \
    } while (0)
//******************************** Defines *********************************//

//******************************** Variables *********************************//
static const platform_gpio_ops_t g_emptyGpioOps = {
    NULL,
    NULL,
    NULL,
    NULL
};
//******************************** Variables *********************************//

//******************************** Functions *********************************//
/**
 * @brief 构造一对可供 Platform I2C 绑定的 GPIO 对象
 * @param[out] scl : SCL GPIO 对象
 * @param[out] sda : SDA GPIO 对象
 * @return 断言失败行号；成功返回 0
 */
static int initialize_gpio_pair(platform_gpio_t *scl, platform_gpio_t *sda)
{
    platform_gpio_init_params_t sclParams = {
        "test_scl",
        &g_emptyGpioOps,
        NULL
    };
    platform_gpio_init_params_t sdaParams = {
        "test_sda",
        &g_emptyGpioOps,
        NULL
    };

    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_init(scl, &sclParams));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_init(sda, &sdaParams));

    return 0;
}

/**
 * @brief 验证 I2C 初始化拒绝缺失的必要对象
 * @return 成功返回 0，失败返回断言行号
 */
static int test_init_rejects_null_required_objects(void)
{
    platform_i2c_t i2c = PLATFORM_I2C_INITIALIZER;
    platform_gpio_t scl = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t sda = PLATFORM_GPIO_INITIALIZER;

    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_init(NULL, "test_i2c", &scl, &sda));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_init(&i2c, "test_i2c", NULL, &sda));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_init(&i2c, "test_i2c", &scl, NULL));

    return 0;
}

/**
 * @brief 验证 I2C 对象只允许绑定一次 GPIO 对象
 * @return 成功返回 0，失败返回断言行号
 */
static int test_init_binds_gpio_pair_and_rejects_repeat_init(void)
{
    platform_i2c_t i2c = PLATFORM_I2C_INITIALIZER;
    platform_gpio_t scl = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t sda = PLATFORM_GPIO_INITIALIZER;
    int result = initialize_gpio_pair(&scl, &sda);

    if (result != 0) {
        return result;
    }

    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_i2c_init(&i2c, "test_i2c", &scl, &sda));
    TEST_ASSERT(i2c.scl == &scl);
    TEST_ASSERT(i2c.sda == &sda);
    TEST_ASSERT(i2c.initialized != 0U);
    TEST_ASSERT(PLATFORM_ERR_ALREADY_INITIALIZED ==
                platform_i2c_init(&i2c, "test_i2c", &scl, &sda));

    return 0;
}

/**
 * @brief 验证未初始化对象拒绝 transaction 和 deinit
 * @return 成功返回 0，失败返回断言行号
 */
static int test_operations_reject_uninitialized_object(void)
{
    platform_i2c_t i2c = PLATFORM_I2C_INITIALIZER;
    uint8_t data = 0U;

    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                platform_i2c_write(&i2c, 0x38U, &data, 1U));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                platform_i2c_read(&i2c, 0x38U, &data, 1U));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                platform_i2c_write_read(&i2c,
                                        0x38U,
                                        &data,
                                        1U,
                                        &data,
                                        1U));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == platform_i2c_deinit(&i2c));

    return 0;
}

/**
 * @brief 验证 transaction 参数边界在协议执行前被拒绝
 * @return 成功返回 0，失败返回断言行号
 */
static int test_transactions_validate_address_pointer_and_length(void)
{
    platform_i2c_t i2c = PLATFORM_I2C_INITIALIZER;
    platform_gpio_t scl = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t sda = PLATFORM_GPIO_INITIALIZER;
    uint8_t txData = 0U;
    uint8_t rxData = 0U;
    int result = initialize_gpio_pair(&scl, &sda);

    if (result != 0) {
        return result;
    }

    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_i2c_init(&i2c, "test_i2c", &scl, &sda));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_write(&i2c, 0x80U, &txData, 1U));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_write(&i2c, 0x38U, NULL, 1U));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_write(&i2c, 0x38U, &txData, 0U));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_read(&i2c, 0x80U, &rxData, 1U));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_read(&i2c, 0x38U, NULL, 1U));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_read(&i2c, 0x38U, &rxData, 0U));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_write_read(&i2c,
                                        0x38U,
                                        NULL,
                                        1U,
                                        &rxData,
                                        1U));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_write_read(&i2c,
                                        0x38U,
                                        &txData,
                                        0U,
                                        &rxData,
                                        1U));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_write_read(&i2c,
                                        0x38U,
                                        &txData,
                                        1U,
                                        NULL,
                                        1U));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_i2c_write_read(&i2c,
                                        0x38U,
                                        &txData,
                                        1U,
                                        &rxData,
                                        0U));

    return 0;
}

int main(void)
{
    int result = test_init_rejects_null_required_objects();

    if (result != 0) {
        return result;
    }

    result = test_init_binds_gpio_pair_and_rejects_repeat_init();
    if (result != 0) {
        return result;
    }

    result = test_operations_reject_uninitialized_object();
    if (result != 0) {
        return result;
    }

    return test_transactions_validate_address_pointer_and_length();
}
//******************************** Functions *********************************//
