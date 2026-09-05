/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_indicator.h
 * @brief 定义 Phase 9 Indicator Task 接口。
 * @author YaoQian Wang
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_INDICATOR_H
#define APP_INDICATOR_H

//******************************** Includes *********************************//
#include "app_ipc_types.h"
#include "platform_def.h"
#include "platform_queue.h"
#include "service_indicator.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define APP_INDICATOR_INITIALIZER {0}
//******************************** Defines *********************************//

//******************************** Types ***********************************//
/** @brief Indicator Task 的非拥有型 Service 与 Queue 依赖。 */
typedef struct
{
    /** 执行 LED 语义副作用的 Indicator Service。 */
    service_indicator_t *service;
    /** Control FSM 发来的 Indicator 命令 Queue。 */
    platform_queue_t *queue;
} app_indicator_config_t;

/** @brief 由 Composition Root 持有的完整 Indicator Task 对象。 */
typedef struct
{
    /** 初始化后保持不变的依赖配置副本。 */
    app_indicator_config_t config;
    /** 已成功执行的 LED 语义事件数。 */
    uint32_t handledEventCount;
    /** Queue、映射或 Indicator Service 失败次数。 */
    uint32_t failureCount;
    /** Indicator 对象是否完成初始化。 */
    platform_bool_t initialized;
} app_indicator_t;
//******************************** Types ***********************************//

//******************************** Declaring *******************************//
/**
 * @brief 初始化 Indicator Task 上下文。
 * @param[in,out] indicator : 调用者拥有且已清零的对象。
 * @param[in] config : Indicator Service 与 Queue 引用。
 * @return platform_error_t : 初始化结果。
 */
platform_error_t app_indicator_init(
    app_indicator_t *indicator,
    const app_indicator_config_t *config);

/**
 * @brief 阻塞接收并执行一条 LED 语义命令。
 * @param[in,out] indicator : 已初始化的对象。
 * @return platform_error_t : Queue 或 Indicator Service 结果。
 * @note ONCE_SUCCESS 的阻塞闪烁只发生在调用本函数的 Indicator Task。
 */
platform_error_t app_indicator_run_once(app_indicator_t *indicator);

/**
 * @brief Indicator Task 正式入口。
 * @param[in] argument : 指向 app_indicator_t。
 * @return 无。
 */
void app_indicator_task_entry(void *argument);
//******************************** Declaring *******************************//

#endif
