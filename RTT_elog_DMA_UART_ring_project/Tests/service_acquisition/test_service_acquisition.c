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

/** @brief 记录双传感器桩行为、返回值和调用顺序。 */
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

/** @brief 将统一采集测试替身恢复为默认成功场景。 */
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

/** @brief 创建已绑定两个传感器替身的采集服务。 */
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

/** @brief 验证服务初始化、重复初始化和反初始化约束。 */
static int test_lifecycle_validates_dependencies(void)
{
    service_acquisition_t service = SERVICE_ACQUISITION_INITIALIZER;
    platform_dht20_t dht20 = PLATFORM_DHT20_INITIALIZER;
    platform_mpu6050_t mpu6050 = PLATFORM_MPU6050_INITIALIZER;
    service_acquisition_config_t config = {&dht20, &mpu6050};

    TEST_ASSERT(service_acquisition_init(NULL, &config) == PLATFORM_ERR_NULL_POINTER);
    TEST_ASSERT(service_acquisition_init(&service, NULL) == PLATFORM_ERR_NULL_POINTER);
    TEST_ASSERT(service_acquisition_init(&service, &config) == PLATFORM_ERR_NOT_INITIALIZED);
    dht20.initialized = PLATFORM_TRUE;
    mpu6050.initialized = PLATFORM_TRUE;
    TEST_ASSERT(service_acquisition_init(&service, &config) == PLATFORM_ERR_OK);
    TEST_ASSERT(service_acquisition_init(&service, &config) == PLATFORM_ERR_ALREADY_INITIALIZED);
    TEST_ASSERT(service_acquisition_deinit(&service) == PLATFORM_ERR_OK);
    TEST_ASSERT(service_acquisition_deinit(&service) == PLATFORM_ERR_NOT_INITIALIZED);
    TEST_ASSERT(dht20.initialized == PLATFORM_TRUE);
    TEST_ASSERT(mpu6050.initialized == PLATFORM_TRUE);

    return 0;
}

/** @brief 验证双传感器成功时原子提交完整结果。 */
static int test_both_success_commits_complete_result(void)
{
    platform_dht20_t dht20 = PLATFORM_DHT20_INITIALIZER;
    platform_mpu6050_t mpu6050 = PLATFORM_MPU6050_INITIALIZER;
    service_acquisition_t service = create_initialized_service(&dht20, &mpu6050);
    service_acquisition_data_t data = {0};

    fake_runtime_reset();
    TEST_ASSERT(service_acquisition_sample(&service, &data) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.dht20CallCount == 1U);
    TEST_ASSERT(g_fakeRuntime.mpu6050CallCount == 1U);
    TEST_ASSERT(g_fakeRuntime.dht20Sequence < g_fakeRuntime.mpu6050Sequence);
    TEST_ASSERT(data.environment.temperatureC == 25.5F);
    TEST_ASSERT(data.environment.humidityPercent == 60.25F);
    TEST_ASSERT(data.motion.accelXRaw == 100);
    TEST_ASSERT(data.motion.gyroZDps == 1.5F);
    TEST_ASSERT(service.statistics.requestCount == 1U);
    TEST_ASSERT(service.statistics.successCount == 1U);
    TEST_ASSERT(service.statistics.failureCount == 0U);
    TEST_ASSERT(service.lastDht20Result == PLATFORM_ERR_OK);
    TEST_ASSERT(service.lastMpu6050Result == PLATFORM_ERR_OK);

    return 0;
}

/** @brief 验证 DHT20 失败后仍尝试 MPU6050 且不污染输出。 */
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

    TEST_ASSERT(service_acquisition_sample(&service, &data) == PLATFORM_ERR_CHECKSUM);
    TEST_ASSERT(g_fakeRuntime.dht20CallCount == 1U);
    TEST_ASSERT(g_fakeRuntime.mpu6050CallCount == 1U);
    TEST_ASSERT(memcmp(&original, &data, sizeof(data)) == 0);
    TEST_ASSERT(service.statistics.failureCount == 1U);
    TEST_ASSERT(service.statistics.dht20FailureCount == 1U);
    TEST_ASSERT(service.statistics.mpu6050FailureCount == 0U);
    TEST_ASSERT(service.lastDht20Result == PLATFORM_ERR_CHECKSUM);
    TEST_ASSERT(service.lastMpu6050Result == PLATFORM_ERR_OK);

    return 0;
}

/** @brief 验证 MPU6050 失败时不提交半成品结果。 */
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

    TEST_ASSERT(service_acquisition_sample(&service, &data) == PLATFORM_ERR_IO);
    TEST_ASSERT(g_fakeRuntime.dht20CallCount == 1U);
    TEST_ASSERT(g_fakeRuntime.mpu6050CallCount == 1U);
    TEST_ASSERT(memcmp(&original, &data, sizeof(data)) == 0);
    TEST_ASSERT(service.statistics.failureCount == 1U);
    TEST_ASSERT(service.statistics.dht20FailureCount == 0U);
    TEST_ASSERT(service.statistics.mpu6050FailureCount == 1U);

    return 0;
}

/** @brief 验证双失败场景保留两路诊断统计。 */
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

    TEST_ASSERT(service_acquisition_sample(&service, &data) == PLATFORM_ERR_TIMEOUT);
    TEST_ASSERT(g_fakeRuntime.dht20CallCount == 1U);
    TEST_ASSERT(g_fakeRuntime.mpu6050CallCount == 1U);
    TEST_ASSERT(memcmp(&original, &data, sizeof(data)) == 0);
    TEST_ASSERT(service.statistics.requestCount == 1U);
    TEST_ASSERT(service.statistics.successCount == 0U);
    TEST_ASSERT(service.statistics.failureCount == 1U);
    TEST_ASSERT(service.statistics.dht20FailureCount == 1U);
    TEST_ASSERT(service.statistics.mpu6050FailureCount == 1U);
    TEST_ASSERT(service.lastDht20Result == PLATFORM_ERR_TIMEOUT);
    TEST_ASSERT(service.lastMpu6050Result == PLATFORM_ERR_IO);

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
