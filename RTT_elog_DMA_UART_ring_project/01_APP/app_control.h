/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_control.h
 * @brief 定义 Phase 9 唯一 APP Control FSM 与 Control Task 接口。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_CONTROL_H
#define APP_CONTROL_H

//******************************** Includes *********************************//
#include "app_ipc_types.h"
#include "button/platform_button.h"
#include "platform_def.h"
#include "platform_queue.h"
#include "service_button.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define APP_CONTROL_INITIALIZER {0}
//******************************** Defines *********************************//

//******************************** Types ***********************************//
typedef enum
{
    APP_CONTROL_STATE_STOPPED = 0,
    APP_CONTROL_STATE_RUNNING,
    APP_CONTROL_STATE_MAX
} app_control_state_t;

typedef struct
{
    platform_button_t *button;
    service_button_t *buttonService;
    platform_queue_t *controlQueue;
    platform_queue_t *acquisitionQueue;
    platform_queue_t *communicationQueue;
    platform_queue_t *indicatorQueue;
} app_control_config_t;

typedef struct
{
    app_control_state_t state;
    platform_bool_t onceActive;
    app_ctrl_source_t onceSource;
    uint32_t nextButtonSampleDeadlineMs;
    platform_bool_t initialized;
} app_control_context_t;

typedef struct
{
    uint32_t processedEventCount;
    uint32_t processedMessageCount;
    uint32_t queueSubmitFailureCount;
    uint32_t buttonReadFailureCount;
    uint32_t buttonProcessFailureCount;
    uint32_t ignoredButtonEventCount;
} app_control_statistics_t;

typedef struct
{
    app_control_config_t config;
    app_control_context_t context;
    app_control_statistics_t statistics;
} app_control_t;
//******************************** Types ***********************************//

//******************************** Declaring *******************************//
/**
 * @brief 初始化唯一 APP Control FSM 及其静态依赖。
 * @param[in,out] control : 调用者拥有且已清零的 Control 对象。
 * @param[in] config : Button、Service 与四条 APP Queue 的引用。
 * @return platform_error_t : 初始化结果。
 */
platform_error_t app_control_init(
    app_control_t *control,
    const app_control_config_t *config);

/**
 * @brief 在唯一 Control execution context 中处理统一控制事件。
 * @param[in,out] control : 已初始化的 Control 对象。
 * @param[in] event : Button 与 UART 共用的控制事件。
 * @param[in] source : 事件来源，只影响响应行为。
 * @return platform_error_t : 业务编排消息的投递结果。
 */
platform_error_t app_control_process_event(
    app_control_t *control,
    app_ctrl_event_t event,
    app_ctrl_source_t source);

/**
 * @brief 处理 Control Queue 中的一条请求或 ONCE 完成消息。
 * @param[in,out] control : 已初始化的 Control 对象。
 * @param[in] message : 由 Queue 按值复制的 Control 消息。
 * @return platform_error_t : 处理结果。
 */
platform_error_t app_control_process_message(
    app_control_t *control,
    const app_control_message_t *message);

/**
 * @brief 读取一次 Button 并将手势映射到统一 Control FSM。
 * @param[in,out] control : 已初始化的 Control 对象。
 * @param[in] nowMs : 本次 10 ms 采样的单调时间戳。
 * @return platform_error_t : 读取、手势识别或业务编排结果。
 */
platform_error_t app_control_sample_button(app_control_t *control, uint32_t nowMs);

/**
 * @brief 执行一次 deadline 驱动的 Queue 等待、消息处理和 Button 采样。
 * @param[in,out] control : 已初始化的 Control 对象。
 * @return platform_error_t : 本轮处理结果。
 */
platform_error_t app_control_run_once(app_control_t *control);

/**
 * @brief Control Task 正式入口。
 * @param[in] argument : 指向 app_control_t。
 * @return 无。
 */
void app_control_task_entry(void *argument);
//******************************** Declaring *******************************//

#endif
