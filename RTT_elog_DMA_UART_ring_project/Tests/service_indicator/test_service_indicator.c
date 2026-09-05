/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_service_indicator.c
 * @brief 验证 Indicator Service 事件语义与 Platform 边界。
 * @author YaoQian Wang
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>
#include <string.h>

#include "project_config.h"
#include "service_indicator.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define TEST_ASSERT(condition)       \
    do {                             \
        if (!(condition)) {          \
            return __LINE__;         \
        }                            \
    } while (0)

#define FAKE_INDICATOR_MAX_RECORDS (8U)
//******************************** Defines *********************************//

_Static_assert(PROJECT_INDICATOR_BLINK_COUNT == 3U,
               "unexpected frozen indicator blink count");
_Static_assert(PROJECT_INDICATOR_BLINK_ON_MS == 100U,
               "unexpected frozen indicator on duration");
_Static_assert(PROJECT_INDICATOR_BLINK_OFF_MS == 100U,
               "unexpected frozen indicator off duration");

//******************************** Types ***********************************//
typedef struct
{
    platform_error_t ledOnResult;
    platform_error_t ledOffResult;
    platform_error_t delayResult;
    uint32_t ledOnCallCount;
    uint32_t ledOffCallCount;
    uint32_t delayCallCount;
    uint32_t delayValues[FAKE_INDICATOR_MAX_RECORDS];
    uint32_t failDelayCall;
    platform_bool_t isLedOn;
} fake_indicator_platform_t;
//******************************** Types ***********************************//

//******************************** Variables ********************************//
static fake_indicator_platform_t g_fakeIndicatorPlatform;
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static void fake_indicator_platform_reset(void)
{
    memset(&g_fakeIndicatorPlatform, 0, sizeof(g_fakeIndicatorPlatform));
    g_fakeIndicatorPlatform.ledOnResult = PLATFORM_ERR_OK;
    g_fakeIndicatorPlatform.ledOffResult = PLATFORM_ERR_OK;
    g_fakeIndicatorPlatform.delayResult = PLATFORM_ERR_OK;
}

static int test_init_validates_lifecycle_and_binds_initialized_led(void)
{
    service_indicator_t service = SERVICE_INDICATOR_INITIALIZER;
    platform_led_t initializedLed = PLATFORM_LED_INITIALIZER;
    platform_led_t uninitializedLed = PLATFORM_LED_INITIALIZER;

    initializedLed.initialized = PLATFORM_TRUE;

    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == service_indicator_init(NULL, &initializedLed));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == service_indicator_init(&service, NULL));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                service_indicator_init(&service, &uninitializedLed));
    TEST_ASSERT(PLATFORM_ERR_OK == service_indicator_init(&service, &initializedLed));
    TEST_ASSERT(&initializedLed == service.led);
    TEST_ASSERT(PLATFORM_TRUE == service.initialized);
    TEST_ASSERT(PLATFORM_ERR_ALREADY_INITIALIZED ==
                service_indicator_init(&service, &initializedLed));

    return 0;
}

static int test_deinit_validates_lifecycle_and_releases_only_service_binding(void)
{
    service_indicator_t service = SERVICE_INDICATOR_INITIALIZER;
    platform_led_t led = PLATFORM_LED_INITIALIZER;

    led.initialized = PLATFORM_TRUE;

    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == service_indicator_deinit(NULL));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == service_indicator_deinit(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_indicator_init(&service, &led));
    TEST_ASSERT(PLATFORM_ERR_OK == service_indicator_deinit(&service));
    TEST_ASSERT(NULL == service.led);
    TEST_ASSERT(PLATFORM_FALSE == service.initialized);
    TEST_ASSERT(PLATFORM_TRUE == led.initialized);

    return 0;
}

static int test_handle_event_validates_service_lifecycle_and_event(void)
{
    service_indicator_t service = SERVICE_INDICATOR_INITIALIZER;
    platform_led_t led = PLATFORM_LED_INITIALIZER;

    led.initialized = PLATFORM_TRUE;

    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER ==
                service_indicator_handle_event(NULL, SERVICE_INDICATOR_EVENT_STOPPED));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                service_indicator_handle_event(&service, SERVICE_INDICATOR_EVENT_STOPPED));
    TEST_ASSERT(PLATFORM_ERR_OK == service_indicator_init(&service, &led));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == service_indicator_handle_event(
                &service,
                (service_indicator_event_t)3));

    return 0;
}

static int test_stopped_turns_led_off(void)
{
    service_indicator_t service = SERVICE_INDICATOR_INITIALIZER;
    platform_led_t led = PLATFORM_LED_INITIALIZER;

    led.initialized = PLATFORM_TRUE;
    fake_indicator_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_indicator_init(&service, &led));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                service_indicator_handle_event(&service, SERVICE_INDICATOR_EVENT_STOPPED));
    TEST_ASSERT(0U == g_fakeIndicatorPlatform.ledOnCallCount);
    TEST_ASSERT(1U == g_fakeIndicatorPlatform.ledOffCallCount);
    TEST_ASSERT(0U == g_fakeIndicatorPlatform.delayCallCount);

    return 0;
}

static int test_running_turns_led_on_without_owning_app_state(void)
{
    service_indicator_t service = SERVICE_INDICATOR_INITIALIZER;
    platform_led_t led = PLATFORM_LED_INITIALIZER;

    led.initialized = PLATFORM_TRUE;
    fake_indicator_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_indicator_init(&service, &led));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                service_indicator_handle_event(&service, SERVICE_INDICATOR_EVENT_RUNNING));
    TEST_ASSERT(1U == g_fakeIndicatorPlatform.ledOnCallCount);
    TEST_ASSERT(0U == g_fakeIndicatorPlatform.ledOffCallCount);
    TEST_ASSERT(0U == g_fakeIndicatorPlatform.delayCallCount);
    TEST_ASSERT(PLATFORM_ERR_OK ==
                service_indicator_handle_event(&service, SERVICE_INDICATOR_EVENT_STOPPED));
    TEST_ASSERT(1U == g_fakeIndicatorPlatform.ledOnCallCount);
    TEST_ASSERT(1U == g_fakeIndicatorPlatform.ledOffCallCount);

    return 0;
}

static int test_once_success_requests_exactly_three_on_off_phases_and_configured_delays(void)
{
    service_indicator_t service = SERVICE_INDICATOR_INITIALIZER;
    platform_led_t led = PLATFORM_LED_INITIALIZER;
    uint32_t delayIndex;

    led.initialized = PLATFORM_TRUE;
    fake_indicator_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_indicator_init(&service, &led));
    TEST_ASSERT(PLATFORM_ERR_OK == service_indicator_handle_event(
                &service,
                SERVICE_INDICATOR_EVENT_ONCE_SUCCESS));
    TEST_ASSERT(PROJECT_INDICATOR_BLINK_COUNT == g_fakeIndicatorPlatform.ledOnCallCount);
    TEST_ASSERT(PROJECT_INDICATOR_BLINK_COUNT == g_fakeIndicatorPlatform.ledOffCallCount);
    TEST_ASSERT((PROJECT_INDICATOR_BLINK_COUNT * 2U) ==
                g_fakeIndicatorPlatform.delayCallCount);
    TEST_ASSERT(PLATFORM_FALSE == g_fakeIndicatorPlatform.isLedOn);

    for (delayIndex = 0U;
         delayIndex < g_fakeIndicatorPlatform.delayCallCount;
         delayIndex++) {
        if ((delayIndex % 2U) == 0U) {
            TEST_ASSERT(PROJECT_INDICATOR_BLINK_ON_MS ==
                        g_fakeIndicatorPlatform.delayValues[delayIndex]);
        } else {
            TEST_ASSERT(PROJECT_INDICATOR_BLINK_OFF_MS ==
                        g_fakeIndicatorPlatform.delayValues[delayIndex]);
        }
    }

    return 0;
}

static int test_once_success_propagates_led_and_time_errors(void)
{
    service_indicator_t service = SERVICE_INDICATOR_INITIALIZER;
    platform_led_t led = PLATFORM_LED_INITIALIZER;

    led.initialized = PLATFORM_TRUE;
    fake_indicator_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_indicator_init(&service, &led));
    g_fakeIndicatorPlatform.ledOnResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == service_indicator_handle_event(
                &service,
                SERVICE_INDICATOR_EVENT_ONCE_SUCCESS));
    TEST_ASSERT(1U == g_fakeIndicatorPlatform.ledOnCallCount);
    TEST_ASSERT(0U == g_fakeIndicatorPlatform.delayCallCount);

    fake_indicator_platform_reset();
    g_fakeIndicatorPlatform.failDelayCall = 1U;
    g_fakeIndicatorPlatform.delayResult = PLATFORM_ERR_TIMEOUT;
    TEST_ASSERT(PLATFORM_ERR_TIMEOUT == service_indicator_handle_event(
                &service,
                SERVICE_INDICATOR_EVENT_ONCE_SUCCESS));
    TEST_ASSERT(1U == g_fakeIndicatorPlatform.ledOnCallCount);
    TEST_ASSERT(0U == g_fakeIndicatorPlatform.ledOffCallCount);
    TEST_ASSERT(1U == g_fakeIndicatorPlatform.delayCallCount);

    fake_indicator_platform_reset();
    g_fakeIndicatorPlatform.ledOffResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == service_indicator_handle_event(
                &service,
                SERVICE_INDICATOR_EVENT_ONCE_SUCCESS));
    TEST_ASSERT(1U == g_fakeIndicatorPlatform.ledOnCallCount);
    TEST_ASSERT(1U == g_fakeIndicatorPlatform.ledOffCallCount);
    TEST_ASSERT(1U == g_fakeIndicatorPlatform.delayCallCount);

    return 0;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t platform_led_on(platform_led_t *led)
{
    (void)led;
    g_fakeIndicatorPlatform.ledOnCallCount++;

    if (g_fakeIndicatorPlatform.ledOnResult == PLATFORM_ERR_OK) {
        g_fakeIndicatorPlatform.isLedOn = PLATFORM_TRUE;
    }

    return g_fakeIndicatorPlatform.ledOnResult;
}

platform_error_t platform_led_off(platform_led_t *led)
{
    (void)led;
    g_fakeIndicatorPlatform.ledOffCallCount++;

    if (g_fakeIndicatorPlatform.ledOffResult == PLATFORM_ERR_OK) {
        g_fakeIndicatorPlatform.isLedOn = PLATFORM_FALSE;
    }

    return g_fakeIndicatorPlatform.ledOffResult;
}

platform_error_t platform_time_delay_ms(uint32_t delayMs)
{
    uint32_t callIndex = g_fakeIndicatorPlatform.delayCallCount;

    if (callIndex < FAKE_INDICATOR_MAX_RECORDS) {
        g_fakeIndicatorPlatform.delayValues[callIndex] = delayMs;
    }
    g_fakeIndicatorPlatform.delayCallCount++;

    if ((g_fakeIndicatorPlatform.failDelayCall != 0U) &&
        (g_fakeIndicatorPlatform.failDelayCall == g_fakeIndicatorPlatform.delayCallCount)) {
        return g_fakeIndicatorPlatform.delayResult;
    }

    return PLATFORM_ERR_OK;
}

int main(void)
{
    int result = test_init_validates_lifecycle_and_binds_initialized_led();

    if (0 != result) {
        return result;
    }

    result = test_deinit_validates_lifecycle_and_releases_only_service_binding();
    if (0 != result) {
        return result;
    }

    result = test_handle_event_validates_service_lifecycle_and_event();
    if (0 != result) {
        return result;
    }

    result = test_stopped_turns_led_off();
    if (0 != result) {
        return result;
    }

    result = test_running_turns_led_on_without_owning_app_state();
    if (0 != result) {
        return result;
    }

    result = test_once_success_requests_exactly_three_on_off_phases_and_configured_delays();
    if (0 != result) {
        return result;
    }

    return test_once_success_propagates_led_and_time_errors();
}
//******************************** Functions *********************************//
