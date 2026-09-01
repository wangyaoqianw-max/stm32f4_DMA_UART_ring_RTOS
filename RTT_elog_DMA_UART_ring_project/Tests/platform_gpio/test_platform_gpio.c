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

typedef struct
{
    platform_gpio_t *gpio;
    platform_gpio_config_t config;
    platform_error_t result;
    uint32_t callCount;
    platform_gpio_level_t writtenLevel;
    platform_gpio_level_t readLevel;
    platform_error_t writeResult;
    platform_error_t readResult;
    uint32_t writeCallCount;
    uint32_t readCallCount;
} fake_gpio_context_t;

/**
 * @brief 记录一次 GPIO 配置请求
 * @param[in] gpio   : GPIO 对象
 * @param[in] config : GPIO 配置
 * @return 预设的假实现结果
 */
static platform_error_t fake_configure(
    platform_gpio_t *gpio,
    const platform_gpio_config_t *config)
{
    fake_gpio_context_t *context = (fake_gpio_context_t *)gpio->implContext;

    context->gpio = gpio;
    context->config = *config;
    context->callCount++;

    return context->result;
}

/**
 * @brief 记录一次 GPIO 写请求
 * @param[in] gpio  : GPIO 对象
 * @param[in] level : 待写入的逻辑电平
 * @return 预设的假实现结果
 */
static platform_error_t fake_write(
    platform_gpio_t *gpio,
    platform_gpio_level_t level)
{
    fake_gpio_context_t *context = (fake_gpio_context_t *)gpio->implContext;

    context->gpio = gpio;
    context->writtenLevel = level;
    context->writeCallCount++;

    return context->writeResult;
}

/**
 * @brief 记录一次 GPIO 读请求并返回预设电平
 * @param[in] gpio  : GPIO 对象
 * @param[out] level : 读取到的逻辑电平
 * @return 预设的假实现结果
 */
static platform_error_t fake_read(
    platform_gpio_t *gpio,
    platform_gpio_level_t *level)
{
    fake_gpio_context_t *context = (fake_gpio_context_t *)gpio->implContext;

    context->gpio = gpio;
    context->readCallCount++;
    *level = context->readLevel;

    return context->readResult;
}

static const platform_gpio_ops_t g_configureOps = {
    fake_configure,
    NULL,
    NULL,
    NULL
};

static const platform_gpio_ops_t g_writeOps = {
    fake_configure,
    fake_write,
    NULL,
    NULL
};

static const platform_gpio_ops_t g_readOps = {
    fake_configure,
    NULL,
    fake_read,
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
 * @brief 验证 configure 的对象和配置指针状态校验
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_configure_validates_state_and_pointer(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;
    fake_gpio_context_t context = {0};
    platform_gpio_init_params_t params = {
        "gpio-test",
        &g_configureOps,
        &context
    };
    platform_gpio_config_t config = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_LOW
    };

    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                platform_gpio_configure(&gpio, &config));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER ==
                platform_gpio_configure(NULL, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_init(&gpio, &params));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER ==
                platform_gpio_configure(&gpio, NULL));
    TEST_ASSERT(0U == context.callCount);

    return 0;
}

/**
 * @brief 验证 configure 拒绝四类非法枚举值
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_configure_rejects_invalid_enum_values(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;
    fake_gpio_context_t context = {0};
    platform_gpio_init_params_t params = {
        "gpio-test",
        &g_configureOps,
        &context
    };
    platform_gpio_config_t config = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_LOW
    };

    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_init(&gpio, &params));

    config.direction = (platform_gpio_direction_t)PLATFORM_GPIO_DIRECTION_MAX;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_gpio_configure(&gpio, &config));
    config.direction = PLATFORM_GPIO_DIRECTION_OUTPUT;

    config.pull = (platform_gpio_pull_t)PLATFORM_GPIO_PULL_MAX;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_gpio_configure(&gpio, &config));
    config.pull = PLATFORM_GPIO_PULL_NONE;

    config.outputType =
        (platform_gpio_output_type_t)PLATFORM_GPIO_OUTPUT_MAX;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_gpio_configure(&gpio, &config));
    config.outputType = PLATFORM_GPIO_OUTPUT_PUSH_PULL;

    config.initialLevel = (platform_gpio_level_t)PLATFORM_GPIO_LEVEL_MAX;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_gpio_configure(&gpio, &config));
    TEST_ASSERT(0U == context.callCount);

    return 0;
}

/**
 * @brief 验证 configure 成功时精确转发并更新对象配置状态
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_configure_forwards_and_updates_state(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;
    fake_gpio_context_t context = {0};
    platform_gpio_init_params_t params = {
        "gpio-test",
        &g_configureOps,
        &context
    };
    platform_gpio_config_t config = {
        PLATFORM_GPIO_DIRECTION_INPUT,
        PLATFORM_GPIO_PULL_UP,
        PLATFORM_GPIO_OUTPUT_OPEN_DRAIN,
        PLATFORM_GPIO_LEVEL_HIGH
    };

    context.result = PLATFORM_ERR_OK;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_init(&gpio, &params));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_configure(&gpio, &config));
    TEST_ASSERT(1U == context.callCount);
    TEST_ASSERT(&gpio == context.gpio);
    TEST_ASSERT(config.direction == context.config.direction);
    TEST_ASSERT(config.pull == context.config.pull);
    TEST_ASSERT(config.outputType == context.config.outputType);
    TEST_ASSERT(config.initialLevel == context.config.initialLevel);
    TEST_ASSERT(config.direction == gpio.config.direction);
    TEST_ASSERT(config.pull == gpio.config.pull);
    TEST_ASSERT(config.outputType == gpio.config.outputType);
    TEST_ASSERT(config.initialLevel == gpio.config.initialLevel);
    TEST_ASSERT(1U == gpio.configured);

    return 0;
}

/**
 * @brief 验证缺失 configure Ops 返回不支持
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_configure_missing_op_is_not_supported(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;
    fake_gpio_context_t context = {0};
    platform_gpio_init_params_t params = {
        "gpio-test",
        &g_emptyOps,
        &context
    };
    platform_gpio_config_t config = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_DOWN,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_HIGH
    };

    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_init(&gpio, &params));
    TEST_ASSERT(PLATFORM_ERR_NOT_SUPPORTED ==
                platform_gpio_configure(&gpio, &config));
    TEST_ASSERT(0U == gpio.configured);

    return 0;
}

/**
 * @brief 验证 configure 失败时保持首次和既有成功状态
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_configure_failure_preserves_state(void)
{
    platform_gpio_t firstGpio = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t configuredGpio = PLATFORM_GPIO_INITIALIZER;
    fake_gpio_context_t firstContext = {0};
    fake_gpio_context_t configuredContext = {0};
    platform_gpio_init_params_t firstParams = {
        "first-gpio",
        &g_configureOps,
        &firstContext
    };
    platform_gpio_init_params_t configuredParams = {
        "configured-gpio",
        &g_configureOps,
        &configuredContext
    };
    platform_gpio_config_t firstConfig = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_LOW
    };
    platform_gpio_config_t oldConfig = {
        PLATFORM_GPIO_DIRECTION_INPUT,
        PLATFORM_GPIO_PULL_UP,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_HIGH
    };
    platform_gpio_config_t failedConfig = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_DOWN,
        PLATFORM_GPIO_OUTPUT_OPEN_DRAIN,
        PLATFORM_GPIO_LEVEL_LOW
    };

    firstContext.result = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_init(&firstGpio, &firstParams));
    TEST_ASSERT(PLATFORM_ERR_IO ==
                platform_gpio_configure(&firstGpio, &firstConfig));
    TEST_ASSERT(0U == firstGpio.configured);
    TEST_ASSERT(0U == firstGpio.config.direction);
    TEST_ASSERT(0U == firstGpio.config.pull);
    TEST_ASSERT(0U == firstGpio.config.outputType);
    TEST_ASSERT(0U == firstGpio.config.initialLevel);

    configuredContext.result = PLATFORM_ERR_OK;
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_init(&configuredGpio, &configuredParams));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_configure(&configuredGpio, &oldConfig));
    configuredContext.result = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO ==
                platform_gpio_configure(&configuredGpio, &failedConfig));
    TEST_ASSERT(1U == configuredGpio.configured);
    TEST_ASSERT(oldConfig.direction == configuredGpio.config.direction);
    TEST_ASSERT(oldConfig.pull == configuredGpio.config.pull);
    TEST_ASSERT(oldConfig.outputType == configuredGpio.config.outputType);
    TEST_ASSERT(oldConfig.initialLevel == configuredGpio.config.initialLevel);
    TEST_ASSERT(2U == configuredContext.callCount);
    TEST_ASSERT(failedConfig.direction == configuredContext.config.direction);
    TEST_ASSERT(failedConfig.pull == configuredContext.config.pull);
    TEST_ASSERT(failedConfig.outputType == configuredContext.config.outputType);
    TEST_ASSERT(failedConfig.initialLevel == configuredContext.config.initialLevel);

    return 0;
}

/**
 * @brief 验证 write 的状态、方向和电平校验
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_write_validates_state_and_parameters(void)
{
    platform_gpio_t uninitializedGpio = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t inputGpio = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t outputGpio = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t missingWriteGpio = PLATFORM_GPIO_INITIALIZER;
    fake_gpio_context_t inputContext = {0};
    fake_gpio_context_t outputContext = {0};
    fake_gpio_context_t missingWriteContext = {0};
    platform_gpio_init_params_t inputParams = {
        "input-gpio",
        &g_writeOps,
        &inputContext
    };
    platform_gpio_init_params_t outputParams = {
        "output-gpio",
        &g_writeOps,
        &outputContext
    };
    platform_gpio_init_params_t missingWriteParams = {
        "missing-write-gpio",
        &g_readOps,
        &missingWriteContext
    };
    platform_gpio_config_t inputConfig = {
        PLATFORM_GPIO_DIRECTION_INPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_LOW
    };
    platform_gpio_config_t outputConfig = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_LOW
    };

    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                platform_gpio_write(&uninitializedGpio,
                                    PLATFORM_GPIO_LEVEL_LOW));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER ==
                platform_gpio_write(NULL, PLATFORM_GPIO_LEVEL_LOW));

    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_init(&inputGpio, &inputParams));
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE ==
                platform_gpio_write(&inputGpio, PLATFORM_GPIO_LEVEL_LOW));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_configure(&inputGpio, &inputConfig));
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE ==
                platform_gpio_write(&inputGpio, PLATFORM_GPIO_LEVEL_LOW));

    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_init(&outputGpio, &outputParams));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_configure(&outputGpio, &outputConfig));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_gpio_write(
                    &outputGpio,
                    (platform_gpio_level_t)PLATFORM_GPIO_LEVEL_MAX));
    TEST_ASSERT(0U == outputContext.writeCallCount);

    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_init(&missingWriteGpio, &missingWriteParams));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_configure(&missingWriteGpio, &outputConfig));
    TEST_ASSERT(PLATFORM_ERR_NOT_SUPPORTED ==
                platform_gpio_write(&missingWriteGpio,
                                    PLATFORM_GPIO_LEVEL_HIGH));

    return 0;
}

/**
 * @brief 验证 write 精确转发且原样传播 Impl 错误
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_write_forwards_and_propagates_error(void)
{
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;
    fake_gpio_context_t context = {0};
    platform_gpio_init_params_t params = {
        "gpio-test",
        &g_writeOps,
        &context
    };
    platform_gpio_config_t config = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_LOW
    };

    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_init(&gpio, &params));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_configure(&gpio, &config));
    context.writeResult = PLATFORM_ERR_OK;
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_write(&gpio, PLATFORM_GPIO_LEVEL_HIGH));
    TEST_ASSERT(1U == context.writeCallCount);
    TEST_ASSERT(&gpio == context.gpio);
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_HIGH == context.writtenLevel);
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_LOW == gpio.config.initialLevel);

    context.writeResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO ==
                platform_gpio_write(&gpio, PLATFORM_GPIO_LEVEL_LOW));
    TEST_ASSERT(2U == context.writeCallCount);

    return 0;
}

/**
 * @brief 验证 read 在输入和输出模式均可用
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_read_allows_input_and_output(void)
{
    platform_gpio_t inputGpio = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t outputGpio = PLATFORM_GPIO_INITIALIZER;
    fake_gpio_context_t inputContext = {0};
    fake_gpio_context_t outputContext = {0};
    platform_gpio_init_params_t inputParams = {
        "input-gpio",
        &g_readOps,
        &inputContext
    };
    platform_gpio_init_params_t outputParams = {
        "output-gpio",
        &g_readOps,
        &outputContext
    };
    platform_gpio_config_t inputConfig = {
        PLATFORM_GPIO_DIRECTION_INPUT,
        PLATFORM_GPIO_PULL_UP,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_LOW
    };
    platform_gpio_config_t outputConfig = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_HIGH
    };
    platform_gpio_level_t level = PLATFORM_GPIO_LEVEL_LOW;

    inputContext.readLevel = PLATFORM_GPIO_LEVEL_HIGH;
    outputContext.readLevel = PLATFORM_GPIO_LEVEL_LOW;
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_init(&inputGpio, &inputParams));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_configure(&inputGpio, &inputConfig));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_read(&inputGpio, &level));
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_HIGH == level);
    TEST_ASSERT(1U == inputContext.readCallCount);

    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_init(&outputGpio, &outputParams));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_configure(&outputGpio, &outputConfig));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_read(&outputGpio, &level));
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_LOW == level);
    TEST_ASSERT(1U == outputContext.readCallCount);

    return 0;
}

/**
 * @brief 验证 read 的状态、输出指针、Ops 和错误传播
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_read_validates_and_propagates_error(void)
{
    platform_gpio_t uninitializedGpio = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t gpio = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t missingReadGpio = PLATFORM_GPIO_INITIALIZER;
    fake_gpio_context_t context = {0};
    fake_gpio_context_t missingReadContext = {0};
    platform_gpio_init_params_t params = {
        "gpio-test",
        &g_readOps,
        &context
    };
    platform_gpio_init_params_t missingReadParams = {
        "missing-read-gpio",
        &g_writeOps,
        &missingReadContext
    };
    platform_gpio_config_t config = {
        PLATFORM_GPIO_DIRECTION_INPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_LOW
    };
    platform_gpio_level_t level = PLATFORM_GPIO_LEVEL_LOW;

    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                platform_gpio_read(&uninitializedGpio, &level));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER ==
                platform_gpio_read(NULL, &level));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_init(&gpio, &params));
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE ==
                platform_gpio_read(&gpio, &level));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_gpio_configure(&gpio, &config));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_gpio_read(&gpio, NULL));

    context.readResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == platform_gpio_read(&gpio, &level));
    TEST_ASSERT(1U == context.readCallCount);

    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_init(&missingReadGpio, &missingReadParams));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_gpio_configure(&missingReadGpio, &config));
    TEST_ASSERT(PLATFORM_ERR_NOT_SUPPORTED ==
                platform_gpio_read(&missingReadGpio, &level));

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

    result = test_configure_validates_state_and_pointer();
    if (0 != result) {
        return result;
    }

    result = test_configure_rejects_invalid_enum_values();
    if (0 != result) {
        return result;
    }

    result = test_configure_forwards_and_updates_state();
    if (0 != result) {
        return result;
    }

    result = test_configure_missing_op_is_not_supported();
    if (0 != result) {
        return result;
    }

    result = test_configure_failure_preserves_state();
    if (0 != result) {
        return result;
    }

    result = test_write_validates_state_and_parameters();
    if (0 != result) {
        return result;
    }

    result = test_write_forwards_and_propagates_error();
    if (0 != result) {
        return result;
    }

    result = test_read_allows_input_and_output();
    if (0 != result) {
        return result;
    }

    result = test_read_validates_and_propagates_error();
    if (0 != result) {
        return result;
    }

    return 0;
}
