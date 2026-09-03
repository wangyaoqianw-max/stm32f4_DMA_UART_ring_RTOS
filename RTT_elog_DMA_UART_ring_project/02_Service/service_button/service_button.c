/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_button.c
 * @brief 实现纯时间驱动的 Button 手势识别 Service。
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "service_button.h"

#include <string.h>

#include "project_config.h"
#include "platform_def.h"
//******************************** Includes *********************************//

//******************************** Private Functions *************************//
static platform_bool_t service_button_is_state_valid(platform_button_state_t state)
{
    return ((state == PLATFORM_BUTTON_STATE_RELEASED) ||
            (state == PLATFORM_BUTTON_STATE_PRESSED)) ? PLATFORM_TRUE : PLATFORM_FALSE;
}

static void service_button_establish_baseline(service_button_t *service,
                                              platform_button_state_t buttonState,
                                              uint32_t nowMs)
{
    service->rawState = buttonState;
    service->stableState = buttonState;
    service->rawChangedMs = nowMs;
    service->baselineValid = PLATFORM_TRUE;

    if (buttonState == PLATFORM_BUTTON_STATE_PRESSED) {
        service->gestureState = SERVICE_BUTTON_GESTURE_FIRST_PRESS;
        service->pressStartedMs = nowMs;
    } else {
        service->gestureState = SERVICE_BUTTON_GESTURE_IDLE;
    }
}

static void service_button_handle_stable_edge(service_button_t *service,
                                              uint32_t nowMs,
                                              service_button_event_t *event)
{
    if (service->stableState == PLATFORM_BUTTON_STATE_PRESSED) {
        if (service->gestureState == SERVICE_BUTTON_GESTURE_IDLE) {
            service->gestureState = SERVICE_BUTTON_GESTURE_FIRST_PRESS;
            service->pressStartedMs = nowMs;
        } else if (service->gestureState == SERVICE_BUTTON_GESTURE_WAIT_SECOND) {
            if ((uint32_t)(nowMs - service->firstReleaseMs) <= PROJECT_BUTTON_DOUBLE_CLICK_MS) {
                service->gestureState = SERVICE_BUTTON_GESTURE_SECOND_PRESS;
                service->pressStartedMs = nowMs;
            } else {
                *event = SERVICE_BUTTON_EVENT_SINGLE;
                service->gestureState = SERVICE_BUTTON_GESTURE_FIRST_PRESS;
                service->pressStartedMs = nowMs;
            }
        }
    } else if (service->gestureState == SERVICE_BUTTON_GESTURE_FIRST_PRESS) {
        service->gestureState = SERVICE_BUTTON_GESTURE_WAIT_SECOND;
        service->firstReleaseMs = nowMs;
    } else if (service->gestureState == SERVICE_BUTTON_GESTURE_SECOND_PRESS) {
        service->gestureState = SERVICE_BUTTON_GESTURE_IDLE;
        *event = SERVICE_BUTTON_EVENT_DOUBLE;
    } else if (service->gestureState == SERVICE_BUTTON_GESTURE_LONG_HOLD) {
        service->gestureState = SERVICE_BUTTON_GESTURE_IDLE;
    }
}

static void service_button_handle_timeouts(service_button_t *service,
                                           uint32_t nowMs,
                                           service_button_event_t *event)
{
    if ((*event == SERVICE_BUTTON_EVENT_NONE) &&
        ((service->gestureState == SERVICE_BUTTON_GESTURE_FIRST_PRESS) ||
         (service->gestureState == SERVICE_BUTTON_GESTURE_SECOND_PRESS)) &&
        ((uint32_t)(nowMs - service->pressStartedMs) >= PROJECT_BUTTON_LONG_PRESS_MS)) {
        service->gestureState = SERVICE_BUTTON_GESTURE_LONG_HOLD;
        *event = SERVICE_BUTTON_EVENT_LONG;
    } else if ((*event == SERVICE_BUTTON_EVENT_NONE) &&
               (service->gestureState == SERVICE_BUTTON_GESTURE_WAIT_SECOND) &&
               (service->rawState != PLATFORM_BUTTON_STATE_PRESSED) &&
               ((uint32_t)(nowMs - service->firstReleaseMs) > PROJECT_BUTTON_DOUBLE_CLICK_MS)) {
        service->gestureState = SERVICE_BUTTON_GESTURE_IDLE;
        *event = SERVICE_BUTTON_EVENT_SINGLE;
    }
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t service_button_init(service_button_t *service)
{
    if (service == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (service->initialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    service->initialized = PLATFORM_TRUE;

    return PLATFORM_ERR_OK;
}

platform_error_t service_button_process(service_button_t *service,
                                        platform_button_state_t buttonState,
                                        uint32_t nowMs,
                                        service_button_event_t *event)
{
    if ((service == NULL) || (event == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (service->initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    if (service_button_is_state_valid(buttonState) != PLATFORM_TRUE) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    *event = SERVICE_BUTTON_EVENT_NONE;

    if (service->baselineValid != PLATFORM_TRUE) {
        service_button_establish_baseline(service, buttonState, nowMs);
        return PLATFORM_ERR_OK;
    }

    if (buttonState != service->rawState) {
        service->rawState = buttonState;
        service->rawChangedMs = nowMs;
    }

    if ((service->rawState != service->stableState) &&
        ((uint32_t)(nowMs - service->rawChangedMs) >= PROJECT_BUTTON_DEBOUNCE_MS)) {
        service->stableState = service->rawState;
        service_button_handle_stable_edge(service, nowMs, event);
    }

    service_button_handle_timeouts(service, nowMs, event);

    return PLATFORM_ERR_OK;
}

platform_error_t service_button_deinit(service_button_t *service)
{
    if (service == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (service->initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    (void)memset(service, 0, sizeof(*service));

    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
