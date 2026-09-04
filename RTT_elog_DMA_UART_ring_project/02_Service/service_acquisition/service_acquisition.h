/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_acquisition.h
 * @brief 定义 DHT20 与 MPU6050 统一原子采集服务。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#ifndef SERVICE_ACQUISITION_H
#define SERVICE_ACQUISITION_H

//******************************** Includes *********************************//
#include "platform_def.h"
#include "dht20/platform_dht20.h"
#include "mpu6050/platform_mpu6050.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define SERVICE_ACQUISITION_INITIALIZER {0}
//******************************** Defines *********************************//

//******************************** Types ***********************************//
typedef struct
{
    platform_dht20_measurement_t environment;
    platform_mpu6050_measurement_t motion;
} service_acquisition_data_t;

typedef struct
{
    platform_dht20_t *dht20;
    platform_mpu6050_t *mpu6050;
} service_acquisition_config_t;

typedef struct
{
    uint32_t requestCount;
    uint32_t successCount;
    uint32_t failureCount;
    uint32_t dht20FailureCount;
    uint32_t mpu6050FailureCount;
} service_acquisition_statistics_t;

/** @brief 调用者拥有的统一采集服务上下文；不拥有传感器对象。 */
typedef struct
{
    service_acquisition_config_t config;
    service_acquisition_statistics_t statistics;
    platform_error_t lastDht20Result;
    platform_error_t lastMpu6050Result;
    platform_bool_t initialized;
} service_acquisition_t;
//******************************** Types ***********************************//

//******************************** Declaring *******************************//
/**
 * @brief 绑定两个已初始化的 Platform 传感器对象。
 * @param[in,out] service : 调用者拥有且已清零的服务对象。
 * @param[in] config : 传感器非拥有型引用配置。
 * @return platform_error_t : 初始化结果。
 * @warning 本服务不拥有传感器及共享 Software I2C 生命周期。
 */
platform_error_t service_acquisition_init(
    service_acquisition_t *service,
    const service_acquisition_config_t *config);

/**
 * @brief 按 DHT20、MPU6050 顺序同步执行一次完整采集。
 * @param[in,out] service : 已初始化的服务对象。
 * @param[out] data : 仅在两个传感器均成功时提交的完整结果。
 * @return platform_error_t : 两个传感器均成功时返回 OK，否则返回首个失败结果。
 * @note 即使 DHT20 失败，仍会尝试 MPU6050；失败时不修改 data。
 * @warning 仅允许由唯一 Acquisition Task 在 Task Context 调用。
 */
platform_error_t service_acquisition_sample(
    service_acquisition_t *service,
    service_acquisition_data_t *data);

/**
 * @brief 解除服务与传感器对象的引用关系。
 * @param[in,out] service : 已初始化的服务对象。
 * @return platform_error_t : 反初始化结果。
 * @note 不反初始化传感器或共享 Software I2C。
 */
platform_error_t service_acquisition_deinit(service_acquisition_t *service);
//******************************** Declaring *******************************//

#endif
