/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_acquisition.c
 * @brief 实现 DHT20 与 MPU6050 顺序采集和结果原子提交。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "service_acquisition.h"

#include <string.h>
//******************************** Includes *********************************//

//******************************** Functions *********************************//
platform_error_t service_acquisition_init(
    service_acquisition_t *service,
    const service_acquisition_config_t *config)
{
    if ((service == NULL) || (config == NULL) ||
        (config->dht20 == NULL) || (config->mpu6050 == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (service->initialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    if ((config->dht20->initialized != PLATFORM_TRUE) ||
        (config->mpu6050->initialized != PLATFORM_TRUE)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    service->config = *config;
    service->lastDht20Result = PLATFORM_ERR_OK;
    service->lastMpu6050Result = PLATFORM_ERR_OK;
    service->initialized = PLATFORM_TRUE;

    return PLATFORM_ERR_OK;
}

platform_error_t service_acquisition_sample(
    service_acquisition_t *service,
    service_acquisition_data_t *data)
{
    service_acquisition_data_t temporaryData = {0};
    platform_error_t dht20Result;
    platform_error_t mpu6050Result;

    if ((service == NULL) || (data == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (service->initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    if ((service->config.dht20 == NULL) || (service->config.mpu6050 == NULL)) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    service->statistics.requestCount++;
    dht20Result = platform_dht20_read(
        service->config.dht20,
        &temporaryData.environment);
    mpu6050Result = platform_mpu6050_read(
        service->config.mpu6050,
        &temporaryData.motion);
    service->lastDht20Result = dht20Result;
    service->lastMpu6050Result = mpu6050Result;

    if (dht20Result != PLATFORM_ERR_OK) {
        service->statistics.dht20FailureCount++;
    }
    if (mpu6050Result != PLATFORM_ERR_OK) {
        service->statistics.mpu6050FailureCount++;
    }

    if ((dht20Result != PLATFORM_ERR_OK) || (mpu6050Result != PLATFORM_ERR_OK)) {
        service->statistics.failureCount++;
        return (dht20Result != PLATFORM_ERR_OK) ? dht20Result : mpu6050Result;
    }

    *data = temporaryData;
    service->statistics.successCount++;

    return PLATFORM_ERR_OK;
}

platform_error_t service_acquisition_deinit(service_acquisition_t *service)
{
    if (service == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (service->initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    (void)memset(service, 0, sizeof(*service));

    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
