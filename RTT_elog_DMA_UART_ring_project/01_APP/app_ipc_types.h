/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_ipc_types.h
 * @brief 定义 Phase 9 APP 模块间的值拷贝消息合同。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_IPC_TYPES_H
#define APP_IPC_TYPES_H

//******************************** Includes *********************************//
#include "app_control_types.h"
#include "dht20/platform_dht20.h"
#include "mpu6050/platform_mpu6050.h"
//******************************** Includes *********************************//

//******************************** Types ***********************************//
/** @brief APP Queue 中按值传递的一组完整双传感器数据。 */
typedef struct
{
    /** DHT20 温湿度测量快照。 */
    platform_dht20_measurement_t environment;
    /** MPU6050 六轴测量快照。 */
    platform_mpu6050_measurement_t motion;
} app_acquisition_data_t;

/** @brief Control Queue 消息类别。 */
typedef enum
{
    APP_CONTROL_MESSAGE_CONTROL_REQUEST = 0,
    APP_CONTROL_MESSAGE_ONCE_ACQUISITION_FAILED,
    APP_CONTROL_MESSAGE_ONCE_TX_RESULT,
    APP_CONTROL_MESSAGE_MAX
} app_control_message_type_t;

/** @brief Button 或 UART 提交给唯一 Control FSM 的请求。 */
typedef struct
{
    /** 与输入介质无关的统一控制事件。 */
    app_ctrl_event_t event;
    /** 请求来源，仅影响响应策略，不形成独立状态。 */
    app_ctrl_source_t source;
} app_control_request_t;

/** @brief Control Queue 的定长值消息。 */
typedef struct
{
    /** 决定 payload 有效成员的消息类别。 */
    app_control_message_type_t type;
    /** 与 type 对应的请求或 ONCE 完成结果。 */
    union
    {
        /** type 为 CONTROL_REQUEST 时有效。 */
        app_control_request_t request;
        /** type 为 ONCE_* completion 时有效。 */
        platform_error_t result;
    } payload;
} app_control_message_t;

/** @brief Control FSM 投递给 Acquisition Task 的命令。 */
typedef enum
{
    APP_ACQUISITION_COMMAND_START_PERIODIC = 0,
    APP_ACQUISITION_COMMAND_STOP_PERIODIC,
    APP_ACQUISITION_COMMAND_SAMPLE_ONCE,
    APP_ACQUISITION_COMMAND_MAX
} app_acquisition_command_t;

/** @brief Communication Task 负责格式化的业务控制响应。 */
typedef enum
{
    APP_CONTROL_RESPONSE_OK_START = 0,
    APP_CONTROL_RESPONSE_OK_STOP,
    APP_CONTROL_RESPONSE_ALREADY_RUNNING,
    APP_CONTROL_RESPONSE_ALREADY_STOPPED,
    APP_CONTROL_RESPONSE_BUSY,
    APP_CONTROL_RESPONSE_ACQUISITION_FAILED,
    APP_CONTROL_RESPONSE_STATUS_RUNNING,
    APP_CONTROL_RESPONSE_STATUS_STOPPED,
    APP_CONTROL_RESPONSE_MAX
} app_control_response_t;

/** @brief Communication Outbound Queue 消息类别。 */
typedef enum
{
    APP_COMM_OUTBOUND_CONTROL_RESPONSE = 0,
    APP_COMM_OUTBOUND_PERIODIC_REPORT,
    APP_COMM_OUTBOUND_ONCE_REPORT,
    APP_COMM_OUTBOUND_MAX
} app_communication_outbound_type_t;

/** @brief Communication Outbound Queue 的定长值消息。 */
typedef struct
{
    /** 决定 payload 有效成员的消息类别。 */
    app_communication_outbound_type_t type;
    /** 待格式化的控制响应或完整传感器数据。 */
    union
    {
        /** type 为 CONTROL_RESPONSE 时有效。 */
        app_control_response_t controlResponse;
        /** type 为 PERIODIC_REPORT 或 ONCE_REPORT 时有效。 */
        app_acquisition_data_t acquisition;
    } payload;
} app_communication_outbound_message_t;

/** @brief Indicator Task 消费的 LED 业务语义。 */
typedef enum
{
    APP_INDICATOR_STOPPED = 0,
    APP_INDICATOR_RUNNING,
    APP_INDICATOR_ONCE_SUCCESS,
    APP_INDICATOR_MAX
} app_indicator_command_t;
//******************************** Types ***********************************//

#endif
