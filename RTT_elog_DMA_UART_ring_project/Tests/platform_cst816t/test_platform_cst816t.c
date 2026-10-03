/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * @file test_platform_cst816t.c
 * @brief 验证 CST816T 初始化序列、原子采样与失败终止
 * @author YaoQian Wang
 * @date 2026-10-03
 * @version V1.0
 *****************************************************************************/

#include "cst816t/platform_cst816t.h"
#include "platform_time.h"
#include "platform_def.h"

#include <stdio.h>
#include <string.h>

#define TEST_ASSERT(condition) do { if (!(condition)) { return __LINE__; } } while (0)
#define TEST_RUN(fn) do { int result = fn(); count++; if (result != 0) { \
    (void)printf("FAIL %s:%d\n", #fn, result); failures++; } } while (0)

typedef struct
{
    uint32_t callCount;
    uint32_t failCall;
    uint32_t kind[12];
    uint32_t value[12];
    uint8_t chipId;
    uint8_t version;
    uint8_t frame[7];
    platform_bool_t invalidTransaction;
} test_recorder_t;

static test_recorder_t g_recorder;

static platform_error_t test_record(uint32_t kind, uint32_t value)
{
    uint32_t index = g_recorder.callCount++;

    if (index >= 12U) {
        return PLATFORM_ERR_OVERFLOW;
    }
    g_recorder.kind[index] = kind;
    g_recorder.value[index] = value;
    return (g_recorder.callCount == g_recorder.failCall) ? PLATFORM_ERR_IO : PLATFORM_ERR_OK;
}

static void test_reset(void)
{
    (void)memset(&g_recorder, 0, sizeof(g_recorder));
    g_recorder.chipId = 0xB5U;
    g_recorder.version = 0x12U;
}

platform_error_t platform_gpio_configure(platform_gpio_t *gpio, const platform_gpio_config_t *config)
{
    (void)gpio;
    if ((config->direction != PLATFORM_GPIO_DIRECTION_OUTPUT) ||
        (config->outputType != PLATFORM_GPIO_OUTPUT_PUSH_PULL) ||
        (config->pull != PLATFORM_GPIO_PULL_NONE) ||
        (config->initialLevel != PLATFORM_GPIO_LEVEL_HIGH)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }
    return test_record(1U, 0U);
}

platform_error_t platform_gpio_write(platform_gpio_t *gpio, platform_gpio_level_t level)
{
    (void)gpio;
    return test_record(2U, (uint32_t)level);
}

platform_error_t platform_time_delay_ms(uint32_t delayMs)
{
    return test_record(3U, delayMs);
}

platform_error_t platform_i2c_write_read(platform_i2c_t *i2c, uint8_t address,
    const uint8_t *txData, uint16_t txLength, uint8_t *rxData, uint16_t rxLength)
{
    platform_error_t result;

    (void)i2c;
    if ((address != 0x15U) || (txLength != 1U) ||
        (((txData[0] == 0x00U) && (rxLength != 7U)) ||
         ((txData[0] != 0x00U) && (rxLength != 1U)))) {
        g_recorder.invalidTransaction = PLATFORM_TRUE;
        return PLATFORM_ERR_INVALID_PARAM;
    }
    result = test_record(4U, txData[0]);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (txData[0] == 0xA7U) {
        rxData[0] = g_recorder.chipId;
    } else if (txData[0] == 0xA9U) {
        rxData[0] = g_recorder.version;
    } else if (txData[0] == 0x00U) {
        (void)memcpy(rxData, g_recorder.frame, 7U);
    } else {
        return PLATFORM_ERR_INVALID_PARAM;
    }
    return PLATFORM_ERR_OK;
}

platform_error_t platform_i2c_write(platform_i2c_t *i2c, uint8_t address,
    const uint8_t *data, uint16_t length)
{
    (void)i2c;
    if ((address != 0x15U) || (length != 2U)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }
    return test_record(5U, ((uint32_t)data[0] << 8U) | data[1]);
}

static int test_init_sequence(void)
{
    platform_cst816t_t device = PLATFORM_CST816T_INITIALIZER;
    platform_i2c_t i2c = PLATFORM_I2C_INITIALIZER;
    platform_gpio_t reset = PLATFORM_GPIO_INITIALIZER;
    const uint32_t kinds[] = {1U, 2U, 3U, 2U, 3U, 4U, 4U, 5U, 5U};
    const uint32_t values[] = {0U, 0U, 10U, 1U, 100U, 0xA7U, 0xA9U, 0xFA60U, 0xFE01U};
    uint32_t index;

    test_reset();
    i2c.initialized = PLATFORM_TRUE;
    reset.initialized = PLATFORM_TRUE;
    TEST_ASSERT(platform_cst816t_init(&device, &i2c, &reset) == PLATFORM_ERR_OK);
    TEST_ASSERT(device.initialized == PLATFORM_TRUE);
    TEST_ASSERT(device.i2c == &i2c && device.reset == &reset);
    TEST_ASSERT(device.chipId == 0xB5U && device.firmwareVersion == 0x12U);
    TEST_ASSERT(g_recorder.callCount == 9U);
    for (index = 0U; index < 9U; index++) {
        TEST_ASSERT(g_recorder.kind[index] == kinds[index]);
        TEST_ASSERT(g_recorder.value[index] == values[index]);
    }
    return 0;
}

static int test_init_failure_stops_sequence(void)
{
    uint32_t failCall;

    for (failCall = 1U; failCall <= 9U; failCall++) {
        platform_cst816t_t device = PLATFORM_CST816T_INITIALIZER;
        platform_i2c_t i2c = PLATFORM_I2C_INITIALIZER;
        platform_gpio_t reset = PLATFORM_GPIO_INITIALIZER;

        test_reset();
        i2c.initialized = PLATFORM_TRUE;
        reset.initialized = PLATFORM_TRUE;
        g_recorder.failCall = failCall;
        TEST_ASSERT(platform_cst816t_init(&device, &i2c, &reset) == PLATFORM_ERR_IO);
        TEST_ASSERT(device.initialized == PLATFORM_FALSE);
        TEST_ASSERT(g_recorder.callCount == failCall);
        g_recorder.failCall = 0U;
        g_recorder.callCount = 0U;
        TEST_ASSERT(platform_cst816t_init(&device, &i2c, &reset) == PLATFORM_ERR_OK);
    }
    return 0;
}

static int test_identity_mismatch(void)
{
    platform_cst816t_t device = PLATFORM_CST816T_INITIALIZER;
    platform_i2c_t i2c = PLATFORM_I2C_INITIALIZER;
    platform_gpio_t reset = PLATFORM_GPIO_INITIALIZER;

    test_reset();
    i2c.initialized = PLATFORM_TRUE;
    reset.initialized = PLATFORM_TRUE;
    g_recorder.chipId = 0xFFU;
    TEST_ASSERT(platform_cst816t_init(&device, &i2c, &reset) == PLATFORM_ERR_NOT_FOUND);
    TEST_ASSERT(device.initialized == PLATFORM_FALSE);
    TEST_ASSERT(g_recorder.callCount == 6U);
    return 0;
}

static platform_cst816t_t test_ready_device(platform_i2c_t *i2c)
{
    platform_cst816t_t device = PLATFORM_CST816T_INITIALIZER;

    i2c->initialized = PLATFORM_TRUE;
    device.i2c = i2c;
    device.initialized = PLATFORM_TRUE;
    return device;
}

static int test_sample_transaction_and_decode(void)
{
    platform_i2c_t i2c = PLATFORM_I2C_INITIALIZER;
    platform_cst816t_t device = test_ready_device(&i2c);
    platform_cst816t_sample_t sample = {0};

    test_reset();
    g_recorder.frame[2] = 1U;
    g_recorder.frame[3] = 0xAAU;
    g_recorder.frame[4] = 0xBCU;
    g_recorder.frame[5] = 0xD1U;
    g_recorder.frame[6] = 0x23U;
    TEST_ASSERT(platform_cst816t_read_sample(&device, &sample) == PLATFORM_ERR_OK);
    TEST_ASSERT(sample.pressed == PLATFORM_TRUE);
    TEST_ASSERT(sample.x == 0xABCU && sample.y == 0x123U);
    TEST_ASSERT(g_recorder.callCount == 1U && g_recorder.value[0] == 0U);
    TEST_ASSERT(g_recorder.invalidTransaction == PLATFORM_FALSE);
    return 0;
}

static int test_release_and_invalid_count(void)
{
    platform_i2c_t i2c = PLATFORM_I2C_INITIALIZER;
    platform_cst816t_t device = test_ready_device(&i2c);
    platform_cst816t_sample_t sample = {PLATFORM_TRUE, 123U, 456U};

    test_reset();
    TEST_ASSERT(platform_cst816t_read_sample(&device, &sample) == PLATFORM_ERR_OK);
    TEST_ASSERT(sample.pressed == PLATFORM_FALSE && sample.x == 0U && sample.y == 0U);
    g_recorder.frame[2] = 2U;
    sample.x = 123U;
    TEST_ASSERT(platform_cst816t_read_sample(&device, &sample) == PLATFORM_ERR_INVALID_STATE);
    TEST_ASSERT(sample.x == 123U);
    return 0;
}

static int test_sample_error_is_atomic(void)
{
    platform_i2c_t i2c = PLATFORM_I2C_INITIALIZER;
    platform_cst816t_t device = test_ready_device(&i2c);
    platform_cst816t_sample_t sample = {PLATFORM_TRUE, 123U, 456U};

    test_reset();
    g_recorder.failCall = 1U;
    TEST_ASSERT(platform_cst816t_read_sample(&device, &sample) == PLATFORM_ERR_IO);
    TEST_ASSERT(sample.pressed == PLATFORM_TRUE && sample.x == 123U && sample.y == 456U);
    return 0;
}

static int test_lifecycle(void)
{
    platform_cst816t_t device = PLATFORM_CST816T_INITIALIZER;
    platform_i2c_t i2c = PLATFORM_I2C_INITIALIZER;
    platform_gpio_t reset = PLATFORM_GPIO_INITIALIZER;
    platform_cst816t_sample_t sample = {0};

    test_reset();
    TEST_ASSERT(platform_cst816t_init(NULL, &i2c, &reset) == PLATFORM_ERR_NULL_POINTER);
    TEST_ASSERT(platform_cst816t_init(&device, NULL, &reset) == PLATFORM_ERR_NULL_POINTER);
    TEST_ASSERT(platform_cst816t_init(&device, &i2c, NULL) == PLATFORM_ERR_NULL_POINTER);
    TEST_ASSERT(platform_cst816t_read_sample(NULL, &sample) == PLATFORM_ERR_NULL_POINTER);
    TEST_ASSERT(platform_cst816t_read_sample(&device, NULL) == PLATFORM_ERR_NULL_POINTER);
    TEST_ASSERT(platform_cst816t_read_sample(&device, &sample) == PLATFORM_ERR_NOT_INITIALIZED);
    TEST_ASSERT(platform_cst816t_init(&device, &i2c, &reset) == PLATFORM_ERR_NOT_INITIALIZED);
    i2c.initialized = PLATFORM_TRUE;
    TEST_ASSERT(platform_cst816t_init(&device, &i2c, &reset) == PLATFORM_ERR_NOT_INITIALIZED);
    reset.initialized = PLATFORM_TRUE;
    TEST_ASSERT(platform_cst816t_init(&device, &i2c, &reset) == PLATFORM_ERR_OK);
    TEST_ASSERT(platform_cst816t_init(&device, &i2c, &reset) == PLATFORM_ERR_ALREADY_INITIALIZED);
    return 0;
}

int main(void)
{
    uint32_t count = 0U;
    uint32_t failures = 0U;

    TEST_RUN(test_init_sequence);
    TEST_RUN(test_init_failure_stops_sequence);
    TEST_RUN(test_identity_mismatch);
    TEST_RUN(test_sample_transaction_and_decode);
    TEST_RUN(test_release_and_invalid_count);
    TEST_RUN(test_sample_error_is_atomic);
    TEST_RUN(test_lifecycle);
    (void)printf("CST816T: %u/%u PASS\n", (unsigned)(count - failures), (unsigned)count);
    return (failures == 0U) ? 0 : 1;
}
