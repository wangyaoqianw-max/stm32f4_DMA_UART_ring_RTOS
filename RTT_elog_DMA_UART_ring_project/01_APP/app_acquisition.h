/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_acquisition.h
 * @brief 定义 Phase 9 Acquisition Task 调度与发布接口。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_ACQUISITION_H
#define APP_ACQUISITION_H

//******************************** Includes *********************************//
#include "app_ipc_types.h"
#include "platform_def.h"
#include "platform_queue.h"
#include "service_acquisition.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define APP_ACQUISITION_INITIALIZER {0}
//******************************** Defines *********************************//

//******************************** Types ***********************************//
typedef struct
{
    service_acquisition_t *service;
    platform_queue_t *commandQueue;
    platform_queue_t *communicationQueue;
    platform_queue_t *controlQueue;
} app_acquisition_config_t;

typedef struct
{
    platform_bool_t periodicEnabled;
    uint32_t nextSampleDeadlineMs;
    platform_bool_t initialized;
} app_acquisition_context_t;

typedef struct
{
    uint32_t periodicSampleCount;
    uint32_t onceSampleCount;
    uint32_t sampleFailureCount;
    uint32_t periodicPublishCount;
    uint32_t oncePublishCount;
    uint32_t queueSubmitFailureCount;
    uint32_t stalePeriodicDiscardCount;
    uint32_t skippedPeriodCount;
} app_acquisition_statistics_t;

typedef struct
{
    app_acquisition_config_t config;
    app_acquisition_context_t context;
    app_acquisition_statistics_t statistics;
} app_acquisition_t;
//******************************** Types ***********************************//

//******************************** Declaring *******************************//
/**
 * @brief 初始化 Acquisition Task 上下文及其静态依赖。
 * @param[in,out] acquisition : 调用者拥有且已清零的对象。
 * @param[in] config : Acquisition Service 与 APP Queue 引用。
 * @return platform_error_t : 初始化结果。
 */
platform_error_t app_acquisition_init(
    app_acquisition_t *acquisition,
    const app_acquisition_config_t *config);

/**
 * @brief 执行一次 command/deadline 驱动的 Acquisition 调度。
 * @param[in,out] acquisition : 已初始化的对象。
 * @return platform_error_t : Queue 或时间基础设施错误；传感器业务失败会转为完成消息。
 */
platform_error_t app_acquisition_run_once(app_acquisition_t *acquisition);

/**
 * @brief Acquisition Task 正式入口。
 * @param[in] argument : 指向 app_acquisition_t。
 * @return 无。
 */
void app_acquisition_task_entry(void *argument);
//******************************** Declaring *******************************//

#endif
