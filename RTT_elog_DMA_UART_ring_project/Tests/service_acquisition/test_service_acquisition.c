/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_service_acquisition.c
 * @brief 验证统一采集服务的原子提交和双传感器诊断合同。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#include "service_acquisition.h"

#include <string.h>

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            return __LINE__; \
        } \
    } while (0)

typedef struct
{
    platform_error_t dht20Result;
    platform_error_t mpu6050Result;
    platform_dht20_measurement_t environment;
    platform_mpu6050_measurement_t motion;
    uint32_t dht20CallCount;
    uint32_t mpu6050CallCount;
    uint32_t callSequence;
    uint32_t dht20Sequence;
    uint32_t mpu6050Sequence;
} fake_acquisition_runtime_t;

static fake_acquisition_runtime_t g_fakeRuntime;

static void fake_runtime_reset(void)
{
    memset(&g_fakeRuntime, 0, sizeof(g_fakeRuntime));
    g_fakeRuntime.dht20Result = PLATFORM_ERR_OK;
    g_fakeRuntime.mpu6050Result = PLATFORM_ERR_OK;
    g_fakeRuntime.environment.temperatureC = 25.5F;
    g_fakeRuntime.environment.humidityPercent = 60.25F;
    g_fakeRuntime.motion.accelXRaw = 100;
    g_fakeRuntime.motion.gyroZDps = 1.5F;
}

static service_acquisition_t create_initialized_service(
    platform_dht20_t *dht20,
    platform_mpu6050_t *mpu6050)
{
    service_acquisition_t service = SERVICE_ACQUISITION_INITIALIZER;
    service_acquisition_config_t config = {dht20, mpu6050};

    dht20->initialized = PLATFORM_TRUE;
    mpu6050->initialized = PLATFORM_TRUE;
    (void)service_acquisition_init(&service, &config);

    return service;
}

static int test_lifecycle_validates_dependencies(void)
{
    service_acquisition_t service = SERVICE_ACQUISITION_INITIALIZER;
    platform_dht20_t dht20 = PLATFORM_DHT20_INITIALIZER;
    platform_mpu6050_t mpu6050 = PLATFORM_MPU6050_INITIALIZER;
    service_acquisition_config_t config = {&dht20, &mpu6050};

    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == service_acquisition_init(NULL, &config));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == service_acquisition_init(&service, NULL));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == service_acquisition_init(&service, &config));
    dht20.initialized = PLATFORM_TRUE;
    mpu6050.initialized = PLATFORM_TRUE;
    TEST_ASSERT(PLATFORM_ERR_OK == service_acquisition_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_ALREADY_INITIALIZED == service_acquisition_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_acquisition_deinit(&service));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == service_acquisition_deinit(&service));
    TEST_ASSERT(PLATFORM_TRUE == dht20.initialized);
    TEST_ASSERT(PLATFORM_TRUE == mpu6050.initialized);

    return 0;
}

static int test_both_success_commits_complete_result(void)
{
    platform_dht20_t dht20 = PLATFORM_DHT20_INITIALIZER;
    platform_mpu6050_t mpu6050 = PLATFORM_MPU6050_INITIALIZER;
    service_acquisition_t service = create_initialized_service(&dht20, &mpu6050);
    service_acquisition_data_t data = {0};

    fake_runtime_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_acquisition_sample(&service, &data));
    TEST_ASSERT(1U == g_fakeRuntime.dht20CallCount);
    TEST_ASSERT(1U == g_fakeRuntime.mpu6050CallCount);
    TEST_ASSERT(g_fakeRuntime.dht20Sequence < g_fakeRuntime.mpu6050Sequence);
    TEST_ASSERT(25.5F == data.environment.temperatureC);
    TEST_ASSERT(60.25F == data.environment.humidityPercent);
    TEST_ASSERT(100 == data.motion.accelXRaw);
    TEST_ASSERT(1.5F == data.motion.gyroZDps);
    TEST_ASSERT(1U == service.statistics.requestCount);
    TEST_ASSERT(1U == service.statistics.successCount);
    TEST_ASSERT(0U == service.statistics.failureCount);
    TEST_ASSERT(PLATFORM_ERR_OK == service.lastDht20Result);
    TEST_ASSERT(PLATFORM_ERR_OK == service.lastMpu6050Result);

    return 0;
}

static int test_dht20_failure_still_attempts_mpu6050_and_preserves_output(void)
{
    platform_dht20_t dht20 = PLATFORM_DHT20_INITIALIZER;
    platform_mpu6050_t mpu6050 = PLATFORM_MPU6050_INITIALIZER;
    service_acquisition_t service = create_initialized_service(&dht20, &mpu6050);
    service_acquisition_data_t original = {0};
    service_acquisition_data_t data;

    fake_runtime_reset();
    original.environment.temperatureC = -20.0F;
    original.motion.accelXRaw = -123;
    data = original;
    g_fakeRuntime.dht20Result = PLATFORM_ERR_CHECKSUM;

    TEST_ASSERT(PLATFORM_ERR_CHECKSUM == service_acquisition_sample(&service, &data));
    TEST_ASSERT(1U == g_fakeRuntime.dht20CallCount);
    TEST_ASSERT(1U == g_fakeRuntime.mpu6050CallCount);
    TEST_ASSERT(0 == memcmp(&original, &data, sizeof(data)));
    TEST_ASSERT(1U == service.statistics.failureCount);
    TEST_ASSERT(1U == service.statistics.dht20FailureCount);
    TEST_ASSERT(0U == service.statistics.mpu6050FailureCount);
    TEST_ASSERT(PLATFORM_ERR_CHECKSUM == service.lastDht20Result);
    TEST_ASSERT(PLATFORM_ERR_OK == service.lastMpu6050Result);

    return 0;
}

static int test_mpu6050_failure_preserves_output(void)
{
    platform_dht20_t dht20 = PLATFORM_DHT20_INITIALIZER;
    platform_mpu6050_t mpu6050 = PLATFORM_MPU6050_INITIALIZER;
    service_acquisition_t service = create_initialized_service(&dht20, &mpu6050);
    service_acquisition_data_t original = {0};
    service_acquisition_data_t data;

    fake_runtime_reset();
    original.environment.temperatureC = -10.0F;
    original.motion.accelYRaw = -321;
    data = original;
    g_fakeRuntime.mpu6050Result = PLATFORM_ERR_IO;

    TEST_ASSERT(PLATFORM_ERR_IO == service_acquisition_sample(&service, &data));
    TEST_ASSERT(1U == g_fakeRuntime.dht20CallCount);
    TEST_ASSERT(1U == g_fakeRuntime.mpu6050CallCount);
    TEST_ASSERT(0 == memcmp(&original, &data, sizeof(data)));
    TEST_ASSERT(1U == service.statistics.failureCount);
    TEST_ASSERT(0U == service.statistics.dht20FailureCount);
    TEST_ASSERT(1U == service.statistics.mpu6050FailureCount);

    return 0;
}

static int test_both_failure_records_both_diagnostics(void)
{
    platform_dht20_t dht20 = PLATFORM_DHT20_INITIALIZER;
    platform_mpu6050_t mpu6050 = PLATFORM_MPU6050_INITIALIZER;
    service_acquisition_t service = create_initialized_service(&dht20, &mpu6050);
    service_acquisition_data_t original = {0};
    service_acquisition_data_t data;

    fake_runtime_reset();
    original.environment.temperatureC = -5.0F;
    original.motion.gyroXRaw = -456;
    data = original;
    g_fakeRuntime.dht20Result = PLATFORM_ERR_TIMEOUT;
    g_fakeRuntime.mpu6050Result = PLATFORM_ERR_IO;

    TEST_ASSERT(PLATFORM_ERR_TIMEOUT == service_acquisition_sample(&service, &data));
    TEST_ASSERT(1U == g_fakeRuntime.dht20CallCount);
    TEST_ASSERT(1U == g_fakeRuntime.mpu6050CallCount);
    TEST_ASSERT(0 == memcmp(&original, &data, sizeof(data)));
    TEST_ASSERT(1U == service.statistics.requestCount);
    TEST_ASSERT(0U == service.statistics.successCount);
    TEST_ASSERT(1U == service.statistics.failureCount);
    TEST_ASSERT(1U == service.statistics.dht20FailureCount);
    TEST_ASSERT(1U == service.statistics.mpu6050FailureCount);
    TEST_ASSERT(PLATFORM_ERR_TIMEOUT == service.lastDht20Result);
    TEST_ASSERT(PLATFORM_ERR_IO == service.lastMpu6050Result);

    return 0;
}

platform_error_t platform_dht20_read(
    platform_dht20_t *dht20,
    platform_dht20_measurement_t *measurement)
{
    (void)dht20;
    g_fakeRuntime.dht20CallCount++;
    g_fakeRuntime.dht20Sequence = ++g_fakeRuntime.callSequence;
    if (g_fakeRuntime.dht20Result == PLATFORM_ERR_OK) {
        *measurement = g_fakeRuntime.environment;
    }
    return g_fakeRuntime.dht20Result;
}

platform_error_t platform_mpu6050_read(
    platform_mpu6050_t *mpu6050,
    platform_mpu6050_measurement_t *measurement)
{
    (void)mpu6050;
    g_fakeRuntime.mpu6050CallCount++;
    g_fakeRuntime.mpu6050Sequence = ++g_fakeRuntime.callSequence;
    if (g_fakeRuntime.mpu6050Result == PLATFORM_ERR_OK) {
        *measurement = g_fakeRuntime.motion;
    }
    return g_fakeRuntime.mpu6050Result;
}

int main(void)
{
    int result = test_lifecycle_validates_dependencies();

    if (result != 0) {
        return result;
    }
    result = test_both_success_commits_complete_result();
    if (result != 0) {
        return result;
    }
    result = test_dht20_failure_still_attempts_mpu6050_and_preserves_output();
    if (result != 0) {
        return result;
    }
    result = test_mpu6050_failure_preserves_output();
    if (result != 0) {
        return result;
    }
    return test_both_failure_records_both_diagnostics();
}
