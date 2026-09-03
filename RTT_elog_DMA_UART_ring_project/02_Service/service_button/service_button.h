/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_button.h
 * @brief 提供纯时间驱动的 Button 手势识别 Service 公共接口。
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

#ifndef SERVICE_BUTTON_H
#define SERVICE_BUTTON_H

//******************************** Includes *********************************//
#include "button/platform_button.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define SERVICE_BUTTON_INITIALIZER {0}
//******************************** Defines *********************************//

//******************************** Declaring *********************************//
typedef enum
{
    SERVICE_BUTTON_EVENT_NONE = 0,
    SERVICE_BUTTON_EVENT_SINGLE,
    SERVICE_BUTTON_EVENT_DOUBLE,
    SERVICE_BUTTON_EVENT_LONG,
    SERVICE_BUTTON_EVENT_MAX
} service_button_event_t;

typedef enum
{
    SERVICE_BUTTON_GESTURE_IDLE = 0,
    SERVICE_BUTTON_GESTURE_FIRST_PRESS,
    SERVICE_BUTTON_GESTURE_WAIT_SECOND,
    SERVICE_BUTTON_GESTURE_SECOND_PRESS,
    SERVICE_BUTTON_GESTURE_LONG_HOLD
} service_button_gesture_state_t;

typedef struct
{
    platform_button_state_t rawState;
    platform_button_state_t stableState;
    service_button_gesture_state_t gestureState;
    uint32_t rawChangedMs;
    uint32_t pressStartedMs;
    uint32_t firstReleaseMs;
    platform_bool_t baselineValid;
    platform_bool_t initialized;
} service_button_t;

/**
 * @brief 初始化调用者持有的 Button Service 上下文。
 * @param[in,out] service : 零初始化或已 deinit 的 Service 对象。
 * @return platform_error_t : 初始化结果。
 */
platform_error_t service_button_init(service_button_t *service);

/**
 * @brief 输入逻辑按键状态并处理消抖和手势。
 * @param[in,out] service : 已初始化的 Service 对象。
 * @param[in] buttonState : 当前逻辑 PRESSED 或 RELEASED 状态。
 * @param[in] nowMs : 调用者提供的单调毫秒时间戳。
 * @param[out] event : 本次调用产生的唯一事件；无事件时为 NONE。
 * @return platform_error_t : 处理结果。
 */
platform_error_t service_button_process(service_button_t *service,
                                        platform_button_state_t buttonState,
                                        uint32_t nowMs,
                                        service_button_event_t *event);

/**
 * @brief 释放 Button Service 生命周期状态。
 * @param[in,out] service : 已初始化的 Service 对象。
 * @return platform_error_t : 反初始化结果。
 */
platform_error_t service_button_deinit(service_button_t *service);
//******************************** Declaring *********************************//

#endif
