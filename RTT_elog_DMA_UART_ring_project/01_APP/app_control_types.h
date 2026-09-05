/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_control_types.h
 * @brief 定义 APP 控制输入的公共事件合同。
 * @author YaoQian Wang
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_CONTROL_TYPES_H
#define APP_CONTROL_TYPES_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Types ***********************************//
/**
 * @brief APP 层接收的统一控制事件
 */
typedef enum
{
    APP_CTRL_START = 0,
    APP_CTRL_STOP,
    APP_CTRL_SAMPLE_ONCE,
    APP_CTRL_GET_STATUS,
    APP_CTRL_EVENT_MAX
} app_ctrl_event_t;

/**
 * @brief APP 控制事件来源
 */
typedef enum
{
    APP_CTRL_SOURCE_BUTTON = 0,
    APP_CTRL_SOURCE_UART,
    APP_CTRL_SOURCE_MAX
} app_ctrl_source_t;

/**
 * @brief 向 APP Control 提交控制事件
 * @note 返回值只表示提交或投递结果，不表示业务执行结果。
 */
typedef platform_error_t (*app_control_event_handler_t)(void *context,
                                                        app_ctrl_event_t event);
//******************************** Types ***********************************//

#endif
