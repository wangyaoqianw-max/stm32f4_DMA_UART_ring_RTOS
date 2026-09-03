/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file freertos_indicator_smoke.c
 * @brief 临时 FreeRTOS 目标板提示灯 Smoke Test 实现。
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "freertos_indicator_smoke.h"

#include "led/platform_bsp_led.h"
#include "platform_time.h"
#include "service_indicator.h"
#include "service_log.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define LOG_TAG                                         "indicator_smoke"
#define FREERTOS_INDICATOR_SMOKE_STOPPED_HOLD_MS    (1000U)
#define FREERTOS_INDICATOR_SMOKE_RUNNING_HOLD_MS    (2000U)
//******************************** Defines **********************************//

//******************************** Private Functions *************************//
static platform_error_t freertos_indicator_smoke_handle_stage(
    service_indicator_t *service,
    service_indicator_event_t event,
    const char *stage,
    uint32_t holdMs)
{
    platform_error_t result;

    SERVICE_LOG_I("%s", stage);

    result = service_indicator_handle_event(service, event);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return platform_time_delay_ms(holdMs);
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
void freertos_indicator_smoke_run(void)
{
    platform_led_t led = PLATFORM_LED_INITIALIZER;
    service_indicator_t indicator = SERVICE_INDICATOR_INITIALIZER;
    platform_error_t result;

    SERVICE_LOG_I("indicator smoke start");

    result = platform_bsp_led_construct_status_led(&led);
    if (result != PLATFORM_ERR_OK) {
        goto fail;
    }

    result = platform_led_init(&led);
    if (result != PLATFORM_ERR_OK) {
        goto fail;
    }

    result = service_indicator_init(&indicator, &led);
    if (result != PLATFORM_ERR_OK) {
        goto fail;
    }

    result = freertos_indicator_smoke_handle_stage(
        &indicator,
        SERVICE_INDICATOR_EVENT_STOPPED,
        "STOPPED",
        FREERTOS_INDICATOR_SMOKE_STOPPED_HOLD_MS);
    if (result != PLATFORM_ERR_OK) {
        goto fail;
    }

    result = freertos_indicator_smoke_handle_stage(
        &indicator,
        SERVICE_INDICATOR_EVENT_RUNNING,
        "RUNNING",
        FREERTOS_INDICATOR_SMOKE_RUNNING_HOLD_MS);
    if (result != PLATFORM_ERR_OK) {
        goto fail;
    }

    result = freertos_indicator_smoke_handle_stage(
        &indicator,
        SERVICE_INDICATOR_EVENT_STOPPED,
        "STOPPED",
        FREERTOS_INDICATOR_SMOKE_STOPPED_HOLD_MS);
    if (result != PLATFORM_ERR_OK) {
        goto fail;
    }

    result = freertos_indicator_smoke_handle_stage(
        &indicator,
        SERVICE_INDICATOR_EVENT_ONCE_SUCCESS,
        "ONCE_SUCCESS",
        0U);
    if (result != PLATFORM_ERR_OK) {
        goto fail;
    }

    (void)service_indicator_deinit(&indicator);
    SERVICE_LOG_I("indicator smoke pass");
    return;

fail:
    if (indicator.initialized == PLATFORM_TRUE) {
        (void)service_indicator_handle_event(
            &indicator,
            SERVICE_INDICATOR_EVENT_STOPPED);
        (void)service_indicator_deinit(&indicator);
    }

    SERVICE_LOG_E("indicator smoke fail");
}
//******************************** Functions *********************************//
