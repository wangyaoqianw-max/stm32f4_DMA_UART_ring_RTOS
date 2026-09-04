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
typedef struct
{
    platform_dht20_measurement_t environment;
    platform_mpu6050_measurement_t motion;
} app_acquisition_data_t;

typedef enum
{
    APP_CONTROL_MESSAGE_CONTROL_REQUEST = 0,
    APP_CONTROL_MESSAGE_ONCE_ACQUISITION_FAILED,
    APP_CONTROL_MESSAGE_ONCE_TX_RESULT,
    APP_CONTROL_MESSAGE_MAX
} app_control_message_type_t;

typedef struct
{
    app_ctrl_event_t event;
    app_ctrl_source_t source;
} app_control_request_t;

typedef struct
{
    app_control_message_type_t type;
    union
    {
        app_control_request_t request;
        platform_error_t result;
    } payload;
} app_control_message_t;

typedef enum
{
    APP_ACQUISITION_COMMAND_START_PERIODIC = 0,
    APP_ACQUISITION_COMMAND_STOP_PERIODIC,
    APP_ACQUISITION_COMMAND_SAMPLE_ONCE,
    APP_ACQUISITION_COMMAND_MAX
} app_acquisition_command_t;

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

typedef enum
{
    APP_COMM_OUTBOUND_CONTROL_RESPONSE = 0,
    APP_COMM_OUTBOUND_PERIODIC_REPORT,
    APP_COMM_OUTBOUND_ONCE_REPORT,
    APP_COMM_OUTBOUND_MAX
} app_communication_outbound_type_t;

typedef struct
{
    app_communication_outbound_type_t type;
    union
    {
        app_control_response_t controlResponse;
        app_acquisition_data_t acquisition;
    } payload;
} app_communication_outbound_message_t;

typedef enum
{
    APP_INDICATOR_STOPPED = 0,
    APP_INDICATOR_RUNNING,
    APP_INDICATOR_ONCE_SUCCESS,
    APP_INDICATOR_MAX
} app_indicator_command_t;
//******************************** Types ***********************************//

#endif
