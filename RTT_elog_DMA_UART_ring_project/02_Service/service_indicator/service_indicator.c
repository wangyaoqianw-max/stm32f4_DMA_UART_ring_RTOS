/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_indicator.c
 * @brief 实现提示灯语义事件与三闪时序。
 * @author YaoQian Wang
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "service_indicator.h"

#include "project_config.h"
#include "platform_time.h"
//******************************** Includes *********************************//

//******************************** Private Functions *************************//
/* 在 Task Context 执行一次亮灭周期；延时失败时保留底层操作的首个错误。 */
static platform_error_t service_indicator_blink_once(service_indicator_t *service)
{
    platform_error_t result = platform_led_on(service->led);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_time_delay_ms(PROJECT_INDICATOR_BLINK_ON_MS);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_led_off(service->led);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return platform_time_delay_ms(PROJECT_INDICATOR_BLINK_OFF_MS);
}

/* 串行完成配置次数的亮灭周期，因此该语义事件会阻塞当前调用任务。 */
static platform_error_t service_indicator_handle_once_success(service_indicator_t *service)
{
    platform_error_t result = PLATFORM_ERR_OK;
    uint32_t blinkIndex;

    for (blinkIndex = 0U;
         blinkIndex < PROJECT_INDICATOR_BLINK_COUNT;
         blinkIndex++) {
        result = service_indicator_blink_once(service);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }
    }

    return PLATFORM_ERR_OK;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t service_indicator_init(
    service_indicator_t *service,
    platform_led_t *led)
{
    if ((service == NULL) || (led == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (service->initialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    if (led->initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    service->led = led;
    service->initialized = PLATFORM_TRUE;

    return PLATFORM_ERR_OK;
}

platform_error_t service_indicator_handle_event(
    service_indicator_t *service,
    service_indicator_event_t event)
{
    if (service == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (service->initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    if (service->led == NULL) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    switch (event) {
        case SERVICE_INDICATOR_EVENT_STOPPED:
            return platform_led_off(service->led);

        case SERVICE_INDICATOR_EVENT_RUNNING:
            return platform_led_on(service->led);

        case SERVICE_INDICATOR_EVENT_ONCE_SUCCESS:
            return service_indicator_handle_once_success(service);

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

platform_error_t service_indicator_deinit(service_indicator_t *service)
{
    if (service == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (service->initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    service->led = NULL;
    service->initialized = PLATFORM_FALSE;

    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
