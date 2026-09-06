/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_display.h
 * @brief 定义 Display Task 的启动、缓存和事件驱动刷新接口
 * @author YaoQian Wang
 * @date 2026-09-06
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

//******************************** Includes *********************************//
#include "app_ipc_types.h"
#include "platform_queue.h"
#include "platform_st7789.h"
#include "project_config.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define APP_DISPLAY_INITIALIZER {0}
//******************************** Defines *********************************//

//******************************** Types ***********************************//
typedef struct
{
    platform_st7789_t *display;
    platform_spi_bus_t *spiBus;
    platform_queue_t *queue;
} app_display_config_t;

typedef struct
{
    app_control_state_t systemState;
    app_acquisition_data_t latestMeasurement;
    platform_bool_t initialized;
    platform_bool_t available;
    platform_bool_t systemStateValid;
    platform_bool_t measurementValid;
    platform_bool_t stateDirty;
    platform_bool_t measurementDirty;
} app_display_context_t;

typedef struct
{
    uint32_t processedMessageCount;
    uint32_t coalescedMessageCount;
    uint32_t startupFailureCount;
    uint32_t renderFailureCount;
} app_display_statistics_t;

typedef struct
{
    app_display_config_t config;
    app_display_context_t context;
    app_display_statistics_t statistics;
} app_display_t;
//******************************** Types ***********************************//

//******************************** Functions *******************************//
/** @brief 绑定 Display Task 的静态依赖，不访问显示硬件。 */
platform_error_t app_display_init(
    app_display_t *appDisplay,
    const app_display_config_t *config);

/** @brief 在 Display Task Context 执行 Boot 到 Main UI 的启动流程。 */
platform_error_t app_display_start(app_display_t *appDisplay);

/** @brief 阻塞接收首条 Display 消息，排空积压并刷新最新缓存。 */
platform_error_t app_display_run_once(app_display_t *appDisplay);

/** @brief Display Task 正式入口。 */
void app_display_task_entry(void *argument);
//******************************** Functions *******************************//

#endif
