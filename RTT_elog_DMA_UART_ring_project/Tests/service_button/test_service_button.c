/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_service_button.c
 * @brief 验证 Button Service 的生命周期和初始采样语义。
 * @author YaoQian Wang
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "platform_def.h"
#include "service_button.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define TEST_ASSERT(condition)       \
    do {                             \
        if (!(condition)) {          \
            return __LINE__;         \
        }                            \
    } while (0)
//******************************** Defines *********************************//

//******************************** Private Functions *************************//
static int test_lifecycle_and_input_validation_reject_invalid_calls(void)
{
    service_button_t service = SERVICE_BUTTON_INITIALIZER;
    service_button_event_t event = SERVICE_BUTTON_EVENT_MAX;

    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == service_button_init(NULL));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == service_button_deinit(NULL));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == service_button_process(
                NULL,
                PLATFORM_BUTTON_STATE_RELEASED,
                0U,
                &event));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == service_button_process(
                &service,
                PLATFORM_BUTTON_STATE_RELEASED,
                0U,
                NULL));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == service_button_process(
                &service,
                PLATFORM_BUTTON_STATE_RELEASED,
                0U,
                &event));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == service_button_deinit(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_ALREADY_INITIALIZED == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == service_button_process(
                &service,
                (platform_button_state_t)PLATFORM_BUTTON_STATE_MAX,
                0U,
                &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_deinit(&service));

    return 0;
}

static int test_first_released_sample_establishes_idle_baseline(void)
{
    service_button_t service = SERVICE_BUTTON_INITIALIZER;
    service_button_event_t event = SERVICE_BUTTON_EVENT_MAX;

    TEST_ASSERT(PLATFORM_ERR_OK == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service,
                PLATFORM_BUTTON_STATE_RELEASED,
                100U,
                &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(PLATFORM_TRUE == service.baselineValid);
    TEST_ASSERT(PLATFORM_BUTTON_STATE_RELEASED == service.rawState);
    TEST_ASSERT(PLATFORM_BUTTON_STATE_RELEASED == service.stableState);
    TEST_ASSERT(SERVICE_BUTTON_GESTURE_IDLE == service.gestureState);

    return 0;
}

static int test_first_pressed_sample_starts_first_press_and_long_timer(void)
{
    service_button_t service = SERVICE_BUTTON_INITIALIZER;
    service_button_event_t event = SERVICE_BUTTON_EVENT_MAX;

    TEST_ASSERT(PLATFORM_ERR_OK == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service,
                PLATFORM_BUTTON_STATE_PRESSED,
                100U,
                &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(PLATFORM_TRUE == service.baselineValid);
    TEST_ASSERT(SERVICE_BUTTON_GESTURE_FIRST_PRESS == service.gestureState);
    TEST_ASSERT(100U == service.pressStartedMs);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service,
                PLATFORM_BUTTON_STATE_PRESSED,
                3099U,
                &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service,
                PLATFORM_BUTTON_STATE_PRESSED,
                3100U,
                &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_LONG == event);

    return 0;
}

static int test_time_based_debounce_requires_thirty_continuous_milliseconds(void)
{
    service_button_t service = SERVICE_BUTTON_INITIALIZER;
    service_button_event_t event = SERVICE_BUTTON_EVENT_MAX;

    TEST_ASSERT(PLATFORM_ERR_OK == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_RELEASED, 0U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_PRESSED, 10U, &event));
    TEST_ASSERT(PLATFORM_BUTTON_STATE_RELEASED == service.stableState);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_RELEASED, 20U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_PRESSED, 25U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_PRESSED, 54U, &event));
    TEST_ASSERT(PLATFORM_BUTTON_STATE_RELEASED == service.stableState);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_PRESSED, 55U, &event));
    TEST_ASSERT(PLATFORM_BUTTON_STATE_PRESSED == service.stableState);
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_RELEASED, 60U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_PRESSED, 70U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_RELEASED, 80U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_RELEASED, 110U, &event));
    TEST_ASSERT(PLATFORM_BUTTON_STATE_RELEASED == service.stableState);
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);

    return 0;
}

static int test_single_is_emitted_once_only_after_double_window_expires(void)
{
    service_button_t service = SERVICE_BUTTON_INITIALIZER;
    service_button_event_t event = SERVICE_BUTTON_EVENT_MAX;

    TEST_ASSERT(PLATFORM_ERR_OK == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 0U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 10U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 40U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 50U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 80U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 380U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 381U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_SINGLE == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 700U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);

    return 0;
}

static int test_double_accepts_second_press_at_window_boundary_only(void)
{
    service_button_t service = SERVICE_BUTTON_INITIALIZER;
    service_button_event_t event = SERVICE_BUTTON_EVENT_MAX;

    TEST_ASSERT(PLATFORM_ERR_OK == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 0U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 10U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 40U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 50U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 80U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 350U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 380U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(SERVICE_BUTTON_GESTURE_SECOND_PRESS == service.gestureState);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 700U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 730U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_DOUBLE == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 1000U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);

    return 0;
}

static int test_double_accepts_second_press_at_two_hundred_ninety_nine_ms(void)
{
    service_button_t service = SERVICE_BUTTON_INITIALIZER;
    service_button_event_t event = SERVICE_BUTTON_EVENT_MAX;

    TEST_ASSERT(PLATFORM_ERR_OK == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 0U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 10U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 40U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 50U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 80U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 349U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 379U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(SERVICE_BUTTON_GESTURE_SECOND_PRESS == service.gestureState);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 400U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 430U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_DOUBLE == event);

    return 0;
}

static int test_expired_second_press_emits_old_single_and_preserves_new_press(void)
{
    service_button_t service = SERVICE_BUTTON_INITIALIZER;
    service_button_event_t event = SERVICE_BUTTON_EVENT_MAX;

    TEST_ASSERT(PLATFORM_ERR_OK == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 0U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 10U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 40U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 50U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 80U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 400U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 430U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_SINGLE == event);
    TEST_ASSERT(SERVICE_BUTTON_GESTURE_FIRST_PRESS == service.gestureState);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 440U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 470U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 771U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_SINGLE == event);

    return 0;
}

static int test_long_emits_once_and_suppresses_release_and_second_click(void)
{
    service_button_t service = SERVICE_BUTTON_INITIALIZER;
    service_button_event_t event = SERVICE_BUTTON_EVENT_MAX;

    TEST_ASSERT(PLATFORM_ERR_OK == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 0U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 2999U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 3000U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_LONG == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 5000U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 5010U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 5040U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);

    return 0;
}

static int test_second_valid_press_held_long_emits_only_long(void)
{
    service_button_t service = SERVICE_BUTTON_INITIALIZER;
    service_button_event_t event = SERVICE_BUTTON_EVENT_MAX;

    TEST_ASSERT(PLATFORM_ERR_OK == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 0U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 10U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 40U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 50U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 80U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 100U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 130U, &event));
    TEST_ASSERT(SERVICE_BUTTON_GESTURE_SECOND_PRESS == service.gestureState);
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 3129U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_PRESSED, 3130U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_LONG == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 3140U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(&service, PLATFORM_BUTTON_STATE_RELEASED, 3170U, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(SERVICE_BUTTON_GESTURE_IDLE == service.gestureState);

    return 0;
}

static int test_wraparound_double_timeout_preserves_exact_and_first_expired_boundaries(void)
{
    service_button_t service = SERVICE_BUTTON_INITIALIZER;
    service_button_event_t event = SERVICE_BUTTON_EVENT_MAX;

    TEST_ASSERT(PLATFORM_ERR_OK == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_RELEASED, 0xFFFFFF00U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_PRESSED, 0xFFFFFF10U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_PRESSED, 0xFFFFFF30U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_RELEASED, 0xFFFFFF40U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_RELEASED, 0xFFFFFF60U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_RELEASED, 0x0000008CU, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_NONE == event);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_RELEASED, 0x0000008DU, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_SINGLE == event);

    return 0;
}

static int test_wraparound_preserves_debounce_double_and_long_elapsed_checks(void)
{
    service_button_t service = SERVICE_BUTTON_INITIALIZER;
    service_button_event_t event = SERVICE_BUTTON_EVENT_MAX;

    TEST_ASSERT(PLATFORM_ERR_OK == service_button_init(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_RELEASED, 0xFFFFFFE0U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_PRESSED, 0xFFFFFFF0U, &event));
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_PRESSED, 0x0000000EU, &event));
    TEST_ASSERT(PLATFORM_BUTTON_STATE_PRESSED == service.stableState);
    TEST_ASSERT(PLATFORM_ERR_OK == service_button_process(
                &service, PLATFORM_BUTTON_STATE_PRESSED, 0x00000BCEU, &event));
    TEST_ASSERT(SERVICE_BUTTON_EVENT_LONG == event);

    return 0;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
int main(void)
{
    int result = test_lifecycle_and_input_validation_reject_invalid_calls();

    if (0 != result) {
        return result;
    }

    result = test_first_released_sample_establishes_idle_baseline();
    if (0 != result) {
        return result;
    }

    result = test_first_pressed_sample_starts_first_press_and_long_timer();
    if (0 != result) {
        return result;
    }

    result = test_time_based_debounce_requires_thirty_continuous_milliseconds();
    if (0 != result) {
        return result;
    }

    result = test_single_is_emitted_once_only_after_double_window_expires();
    if (0 != result) {
        return result;
    }

    result = test_double_accepts_second_press_at_window_boundary_only();
    if (0 != result) {
        return result;
    }

    result = test_double_accepts_second_press_at_two_hundred_ninety_nine_ms();
    if (0 != result) {
        return result;
    }

    result = test_expired_second_press_emits_old_single_and_preserves_new_press();
    if (0 != result) {
        return result;
    }

    result = test_long_emits_once_and_suppresses_release_and_second_click();
    if (0 != result) {
        return result;
    }

    result = test_second_valid_press_held_long_emits_only_long();
    if (0 != result) {
        return result;
    }

    result = test_wraparound_double_timeout_preserves_exact_and_first_expired_boundaries();
    if (0 != result) {
        return result;
    }

    return test_wraparound_preserves_debounce_double_and_long_elapsed_checks();
}
//******************************** Functions *********************************//
