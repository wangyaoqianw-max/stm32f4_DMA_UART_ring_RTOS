/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_control.h
 * @brief 定义 Phase 9 唯一 APP Control FSM 与 Control Task 接口。
 * @author YaoQian Wang
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

/** @brief Control Task 的非拥有型硬件、Service 与 Queue 依赖。 */
typedef struct
{
    /** Control Task 唯一轮询的 Platform Button。 */
    platform_button_t *button;
    /** 将 10 ms Button 样本转换为手势的 Button Service。 */
    service_button_t *buttonService;
    /** UART 请求和 ONCE completion 的输入 Queue。 */
    platform_queue_t *controlQueue;
    /** 发往 Acquisition Task 的命令 Queue。 */
    platform_queue_t *acquisitionQueue;
    /** 发往 Communication Task 的业务输出 Queue。 */
    platform_queue_t *communicationQueue;
    /** 发往 Indicator Task 的 LED 语义 Queue。 */
    platform_queue_t *indicatorQueue;
} app_control_config_t;

/** @brief 唯一 APP Control FSM 及 Button deadline 运行上下文。 */
typedef struct
{
    /** 系统唯一 STOPPED/RUNNING 业务状态。 */
    app_control_state_t state;
    /** STOPPED 下是否存在尚未完成的 ONCE 事务。 */
    platform_bool_t onceActive;
    /** 当前 ONCE 来源，用于决定失败时是否发送 UART 响应。 */
    app_ctrl_source_t onceSource;
    /** 下一次 Button 采样的绝对单调毫秒 deadline。 */
    uint32_t nextButtonSampleDeadlineMs;
    /** Control 对象是否完成初始化。 */
    platform_bool_t initialized;
} app_control_context_t;

/** @brief Control Task 的累计运行与故障统计。 */
typedef struct
{
    /** 已处理的统一控制事件数。 */
    uint32_t processedEventCount;
    /** 已处理的 Control Queue 消息数。 */
    uint32_t processedMessageCount;
    /** 向任一 APP Queue 投递失败的累计次数。 */
    uint32_t queueSubmitFailureCount;
    /** Platform Button 读取失败次数。 */
    uint32_t buttonReadFailureCount;
    /** Button Service 处理失败或产生非法事件的次数。 */
    uint32_t buttonProcessFailureCount;
    /** ONCE busy 等场景下被忽略的 Button 业务事件数。 */
    uint32_t ignoredButtonEventCount;
} app_control_statistics_t;

/** @brief 由 Composition Root 持有的完整 Control Task 对象。 */
typedef struct
{
    /** 初始化后保持不变的依赖配置副本。 */
    app_control_config_t config;
    /** 仅由 Control Task 修改的运行上下文。 */
    app_control_context_t context;
    /** Control Task 累计统计。 */
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
