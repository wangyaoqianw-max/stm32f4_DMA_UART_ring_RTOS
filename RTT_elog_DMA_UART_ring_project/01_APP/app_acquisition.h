/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_acquisition.h
 * @brief 定义 Phase 9 Acquisition Task 调度与发布接口。
 * @author YaoQian Wang
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
/** @brief Acquisition Task 的非拥有型 Service 与 Queue 依赖。 */
typedef struct
{
    /** 执行完整双传感器采集的 Unified Acquisition Service。 */
    service_acquisition_t *service;
    /** Control FSM 发来的周期/ONCE 命令 Queue。 */
    platform_queue_t *commandQueue;
    /** 完整传感器结果的 Communication Outbound Queue。 */
    platform_queue_t *communicationQueue;
    /** ONCE 失败完成消息回传到 Control FSM 的 Queue。 */
    platform_queue_t *controlQueue;
} app_acquisition_config_t;

/** @brief Acquisition Task 的周期执行上下文。 */
typedef struct
{
    /** 是否允许新的周期采集；不是 APP RUNNING 状态副本。 */
    platform_bool_t periodicEnabled;
    /** 下一次周期采集的绝对单调毫秒 deadline。 */
    uint32_t nextSampleDeadlineMs;
    /** Acquisition 对象是否完成初始化。 */
    platform_bool_t initialized;
} app_acquisition_context_t;

/** @brief Acquisition Task 的累计调度、发布和故障统计。 */
typedef struct
{
    /** 已触发的周期完整采集次数。 */
    uint32_t periodicSampleCount;
    /** 已触发的 ONCE 完整采集次数。 */
    uint32_t onceSampleCount;
    /** Unified Acquisition Service 返回失败的次数。 */
    uint32_t sampleFailureCount;
    /** 成功投递到 Communication Queue 的周期报告数。 */
    uint32_t periodicPublishCount;
    /** 成功投递到 Communication Queue 的 ONCE 报告数。 */
    uint32_t oncePublishCount;
    /** 向 Communication 或 Control Queue 投递失败次数。 */
    uint32_t queueSubmitFailureCount;
    /** 因采样期间观察到 STOP 而丢弃的周期结果数。 */
    uint32_t stalePeriodicDiscardCount;
    /** 超期后跳过而未补采的历史周期数。 */
    uint32_t skippedPeriodCount;
} app_acquisition_statistics_t;

/** @brief 由 Composition Root 持有的完整 Acquisition Task 对象。 */
typedef struct
{
    /** 初始化后保持不变的依赖配置副本。 */
    app_acquisition_config_t config;
    /** 仅由 Acquisition Task 修改的运行上下文。 */
    app_acquisition_context_t context;
    /** Acquisition Task 累计统计。 */
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
