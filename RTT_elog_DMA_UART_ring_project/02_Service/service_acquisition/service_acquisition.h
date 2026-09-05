/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_acquisition.h
 * @brief 定义 DHT20 与 MPU6050 统一原子采集服务。
 * @author YaoQian Wang
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
/** @brief 一次完整采集产生的原子数据快照。 */
typedef struct
{
    /** DHT20 温湿度测量结果。 */
    platform_dht20_measurement_t environment;
    /** MPU6050 六轴测量结果。 */
    platform_mpu6050_measurement_t motion;
} service_acquisition_data_t;

/** @brief Unified Acquisition Service 的非拥有型依赖配置。 */
typedef struct
{
    /** 已初始化的 DHT20 对象，由 Composition Root 持有。 */
    platform_dht20_t *dht20;
    /** 已初始化的 MPU6050 对象，由 Composition Root 持有。 */
    platform_mpu6050_t *mpu6050;
} service_acquisition_config_t;

/** @brief Unified Acquisition Service 的累计诊断统计。 */
typedef struct
{
    /** 发起完整双传感器采集的次数。 */
    uint32_t requestCount;
    /** 两个传感器均成功且完成原子提交的次数。 */
    uint32_t successCount;
    /** 任一传感器失败导致完整采集失败的次数。 */
    uint32_t failureCount;
    /** DHT20 单次读取失败的累计次数。 */
    uint32_t dht20FailureCount;
    /** MPU6050 单次读取失败的累计次数。 */
    uint32_t mpu6050FailureCount;
} service_acquisition_statistics_t;

/** @brief 调用者拥有的统一采集服务上下文；不拥有传感器对象。 */
typedef struct
{
    /** 初始化时复制的非拥有型传感器引用。 */
    service_acquisition_config_t config;
    /** 由本服务维护的累计诊断统计。 */
    service_acquisition_statistics_t statistics;
    /** 最近一次 DHT20 读取结果。 */
    platform_error_t lastDht20Result;
    /** 最近一次 MPU6050 读取结果。 */
    platform_error_t lastMpu6050Result;
    /** 服务是否已完成依赖绑定。 */
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
