/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_app_control.c
 * @brief 验证唯一 APP Control FSM、Button 映射和 deadline 轮询。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#include "app_control.h"

#include <string.h>

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            return __LINE__; \
        } \
    } while (0)

/** @brief 汇总 Control Task 所需 Queue、输入替身与观测记录。 */
typedef struct
{
    platform_queue_t controlQueue;
    platform_queue_t acquisitionQueue;
    platform_queue_t communicationQueue;
    platform_queue_t indicatorQueue;
    app_control_message_t receiveMessage;
    app_acquisition_command_t acquisitionMessages[8];
    app_communication_outbound_message_t communicationMessages[16];
    app_indicator_command_t indicatorMessages[8];
    platform_error_t queueSendResult;
    platform_error_t queueReceiveResult;
    platform_error_t buttonReadResult;
    platform_error_t buttonProcessResult;
    platform_error_t timeResult;
    platform_button_state_t buttonState;
    service_button_event_t buttonEvent;
    uint32_t acquisitionCount;
    uint32_t communicationCount;
    uint32_t indicatorCount;
    uint32_t receiveCallCount;
    uint32_t lastReceiveTimeoutMs;
    uint32_t nowMs;
    uint32_t buttonReadCount;
    uint32_t buttonProcessCount;
} fake_control_runtime_t;

static fake_control_runtime_t g_fakeRuntime;

/** @brief 将 Control 测试替身恢复为默认可用状态。 */
static void fake_runtime_reset(void)
{
    memset(&g_fakeRuntime, 0, sizeof(g_fakeRuntime));
    g_fakeRuntime.controlQueue.native = &g_fakeRuntime.controlQueue;
    g_fakeRuntime.acquisitionQueue.native = &g_fakeRuntime.acquisitionQueue;
    g_fakeRuntime.communicationQueue.native = &g_fakeRuntime.communicationQueue;
    g_fakeRuntime.indicatorQueue.native = &g_fakeRuntime.indicatorQueue;
    g_fakeRuntime.queueSendResult = PLATFORM_ERR_OK;
    g_fakeRuntime.queueReceiveResult = PLATFORM_ERR_TIMEOUT;
    g_fakeRuntime.buttonReadResult = PLATFORM_ERR_OK;
    g_fakeRuntime.buttonProcessResult = PLATFORM_ERR_OK;
    g_fakeRuntime.timeResult = PLATFORM_ERR_OK;
    g_fakeRuntime.buttonState = PLATFORM_BUTTON_STATE_RELEASED;
    g_fakeRuntime.buttonEvent = SERVICE_BUTTON_EVENT_NONE;
    g_fakeRuntime.nowMs = 100U;
}

/** @brief 创建依赖已就绪的 Control FSM 测试对象。 */
static app_control_t create_control(
    platform_button_t *button,
    service_button_t *buttonService)
{
    app_control_t control = APP_CONTROL_INITIALIZER;
    app_control_config_t config = {
        .button = button,
        .buttonService = buttonService,
        .controlQueue = &g_fakeRuntime.controlQueue,
        .acquisitionQueue = &g_fakeRuntime.acquisitionQueue,
        .communicationQueue = &g_fakeRuntime.communicationQueue,
        .indicatorQueue = &g_fakeRuntime.indicatorQueue
    };

    button->initialized = PLATFORM_TRUE;
    buttonService->initialized = PLATFORM_TRUE;
    (void)app_control_init(&control, &config);

    return control;
}

/** @brief 清空各下游 Queue 的已记录输出计数。 */
static void fake_clear_outputs(void)
{
    g_fakeRuntime.acquisitionCount = 0U;
    g_fakeRuntime.communicationCount = 0U;
    g_fakeRuntime.indicatorCount = 0U;
}

/** @brief 验证初始 STOPPED 状态及 Button deadline。 */
static int test_boots_stopped_and_initializes_deadline(void)
{
    app_control_t control = APP_CONTROL_INITIALIZER;
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    service_button_t buttonService = SERVICE_BUTTON_INITIALIZER;
    app_control_config_t config = {
        .button = &button,
        .buttonService = &buttonService,
        .controlQueue = &g_fakeRuntime.controlQueue,
        .acquisitionQueue = &g_fakeRuntime.acquisitionQueue,
        .communicationQueue = &g_fakeRuntime.communicationQueue,
        .indicatorQueue = &g_fakeRuntime.indicatorQueue
    };

    fake_runtime_reset();
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == app_control_init(NULL, &config));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == app_control_init(&control, NULL));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == app_control_init(&control, &config));
    button.initialized = PLATFORM_TRUE;
    buttonService.initialized = PLATFORM_TRUE;
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_init(&control, &config));
    TEST_ASSERT(APP_CONTROL_STATE_STOPPED == control.context.state);
    TEST_ASSERT(PLATFORM_FALSE == control.context.onceActive);
    TEST_ASSERT(110U == control.context.nextButtonSampleDeadlineMs);
    TEST_ASSERT(PLATFORM_ERR_ALREADY_INITIALIZED == app_control_init(&control, &config));

    return 0;
}

/** @brief 验证 UART START/STOP 共用唯一状态机。 */
static int test_uart_start_and_stop_share_one_state_machine(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    service_button_t buttonService = SERVICE_BUTTON_INITIALIZER;
    app_control_t control;

    fake_runtime_reset();
    control = create_control(&button, &buttonService);

    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_START, APP_CTRL_SOURCE_UART));
    TEST_ASSERT(APP_CONTROL_STATE_RUNNING == control.context.state);
    TEST_ASSERT(1U == g_fakeRuntime.acquisitionCount);
    TEST_ASSERT(APP_ACQUISITION_COMMAND_START_PERIODIC ==
                g_fakeRuntime.acquisitionMessages[0]);
    TEST_ASSERT(1U == g_fakeRuntime.indicatorCount);
    TEST_ASSERT(APP_INDICATOR_RUNNING == g_fakeRuntime.indicatorMessages[0]);
    TEST_ASSERT(APP_CONTROL_RESPONSE_OK_START ==
                g_fakeRuntime.communicationMessages[0].payload.controlResponse);

    fake_clear_outputs();
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_GET_STATUS, APP_CTRL_SOURCE_UART));
    TEST_ASSERT(APP_CONTROL_RESPONSE_STATUS_RUNNING ==
                g_fakeRuntime.communicationMessages[0].payload.controlResponse);

    fake_clear_outputs();
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_SAMPLE_ONCE, APP_CTRL_SOURCE_UART));
    TEST_ASSERT(APP_CONTROL_RESPONSE_ALREADY_RUNNING ==
                g_fakeRuntime.communicationMessages[0].payload.controlResponse);

    fake_clear_outputs();
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_START, APP_CTRL_SOURCE_UART));
    TEST_ASSERT(APP_CONTROL_STATE_RUNNING == control.context.state);
    TEST_ASSERT(0U == g_fakeRuntime.acquisitionCount);
    TEST_ASSERT(APP_CONTROL_RESPONSE_ALREADY_RUNNING ==
                g_fakeRuntime.communicationMessages[0].payload.controlResponse);

    fake_clear_outputs();
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_STOP, APP_CTRL_SOURCE_UART));
    TEST_ASSERT(APP_CONTROL_STATE_STOPPED == control.context.state);
    TEST_ASSERT(APP_ACQUISITION_COMMAND_STOP_PERIODIC ==
                g_fakeRuntime.acquisitionMessages[0]);
    TEST_ASSERT(APP_INDICATOR_STOPPED == g_fakeRuntime.indicatorMessages[0]);
    TEST_ASSERT(APP_CONTROL_RESPONSE_OK_STOP ==
                g_fakeRuntime.communicationMessages[0].payload.controlResponse);

    fake_clear_outputs();
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_STOP, APP_CTRL_SOURCE_UART));
    TEST_ASSERT(APP_CONTROL_RESPONSE_ALREADY_STOPPED ==
                g_fakeRuntime.communicationMessages[0].payload.controlResponse);

    return 0;
}

/** @brief 验证 ONCE 占用、状态查询及采集失败完成路径。 */
static int test_once_busy_status_and_failure_completion(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    service_button_t buttonService = SERVICE_BUTTON_INITIALIZER;
    app_control_t control;
    app_control_message_t completion = {0};

    fake_runtime_reset();
    control = create_control(&button, &buttonService);

    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_SAMPLE_ONCE, APP_CTRL_SOURCE_UART));
    TEST_ASSERT(APP_CONTROL_STATE_STOPPED == control.context.state);
    TEST_ASSERT(PLATFORM_TRUE == control.context.onceActive);
    TEST_ASSERT(APP_CTRL_SOURCE_UART == control.context.onceSource);
    TEST_ASSERT(APP_ACQUISITION_COMMAND_SAMPLE_ONCE ==
                g_fakeRuntime.acquisitionMessages[0]);
    TEST_ASSERT(0U == g_fakeRuntime.communicationCount);

    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_START, APP_CTRL_SOURCE_UART));
    TEST_ASSERT(APP_CONTROL_RESPONSE_BUSY ==
                g_fakeRuntime.communicationMessages[0].payload.controlResponse);
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_STOP, APP_CTRL_SOURCE_UART));
    TEST_ASSERT(APP_CONTROL_RESPONSE_BUSY ==
                g_fakeRuntime.communicationMessages[1].payload.controlResponse);
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_SAMPLE_ONCE, APP_CTRL_SOURCE_UART));
    TEST_ASSERT(APP_CONTROL_RESPONSE_BUSY ==
                g_fakeRuntime.communicationMessages[2].payload.controlResponse);
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_GET_STATUS, APP_CTRL_SOURCE_UART));
    TEST_ASSERT(APP_CONTROL_RESPONSE_STATUS_STOPPED ==
                g_fakeRuntime.communicationMessages[3].payload.controlResponse);

    fake_clear_outputs();
    completion.type = APP_CONTROL_MESSAGE_ONCE_ACQUISITION_FAILED;
    completion.payload.result = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_message(&control, &completion));
    TEST_ASSERT(PLATFORM_FALSE == control.context.onceActive);
    TEST_ASSERT(APP_CONTROL_RESPONSE_ACQUISITION_FAILED ==
                g_fakeRuntime.communicationMessages[0].payload.controlResponse);
    TEST_ASSERT(0U == g_fakeRuntime.indicatorCount);

    return 0;
}

/** @brief 验证 Queue 失败可观测且不会误改 FSM 状态。 */
static int test_queue_failure_is_observable_and_does_not_change_state(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    service_button_t buttonService = SERVICE_BUTTON_INITIALIZER;
    app_control_t control;

    fake_runtime_reset();
    control = create_control(&button, &buttonService);
    g_fakeRuntime.queueSendResult = PLATFORM_ERR_FULL;

    TEST_ASSERT(PLATFORM_ERR_FULL == app_control_process_event(
                &control, APP_CTRL_START, APP_CTRL_SOURCE_UART));
    TEST_ASSERT(APP_CONTROL_STATE_STOPPED == control.context.state);
    TEST_ASSERT(1U == control.statistics.queueSubmitFailureCount);

    return 0;
}

/** @brief 验证 ONCE 发送完成结果决定成功指示。 */
static int test_once_tx_completion_controls_success_indicator(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    service_button_t buttonService = SERVICE_BUTTON_INITIALIZER;
    app_control_t control;
    app_control_message_t completion = {0};

    fake_runtime_reset();
    control = create_control(&button, &buttonService);
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_SAMPLE_ONCE, APP_CTRL_SOURCE_BUTTON));

    fake_clear_outputs();
    completion.type = APP_CONTROL_MESSAGE_ONCE_TX_RESULT;
    completion.payload.result = PLATFORM_ERR_TIMEOUT;
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_message(&control, &completion));
    TEST_ASSERT(PLATFORM_FALSE == control.context.onceActive);
    TEST_ASSERT(0U == g_fakeRuntime.indicatorCount);

    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_event(
                &control, APP_CTRL_SAMPLE_ONCE, APP_CTRL_SOURCE_BUTTON));
    fake_clear_outputs();
    completion.payload.result = PLATFORM_ERR_OK;
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_process_message(&control, &completion));
    TEST_ASSERT(PLATFORM_FALSE == control.context.onceActive);
    TEST_ASSERT(1U == g_fakeRuntime.indicatorCount);
    TEST_ASSERT(APP_INDICATOR_ONCE_SUCCESS == g_fakeRuntime.indicatorMessages[0]);

    return 0;
}

/** @brief 验证 Button 手势映射到同一控制 FSM。 */
static int test_button_gestures_map_into_same_fsm(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    service_button_t buttonService = SERVICE_BUTTON_INITIALIZER;
    app_control_t control;

    fake_runtime_reset();
    control = create_control(&button, &buttonService);

    g_fakeRuntime.buttonEvent = SERVICE_BUTTON_EVENT_SINGLE;
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_sample_button(&control, 110U));
    TEST_ASSERT(APP_CONTROL_STATE_RUNNING == control.context.state);
    TEST_ASSERT(APP_ACQUISITION_COMMAND_START_PERIODIC ==
                g_fakeRuntime.acquisitionMessages[0]);
    TEST_ASSERT(0U == g_fakeRuntime.communicationCount);

    fake_clear_outputs();
    g_fakeRuntime.buttonEvent = SERVICE_BUTTON_EVENT_LONG;
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_sample_button(&control, 120U));
    TEST_ASSERT(APP_CONTROL_STATE_STOPPED == control.context.state);
    TEST_ASSERT(APP_ACQUISITION_COMMAND_STOP_PERIODIC ==
                g_fakeRuntime.acquisitionMessages[0]);

    fake_clear_outputs();
    g_fakeRuntime.buttonEvent = SERVICE_BUTTON_EVENT_DOUBLE;
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_sample_button(&control, 130U));
    TEST_ASSERT(PLATFORM_TRUE == control.context.onceActive);
    TEST_ASSERT(APP_ACQUISITION_COMMAND_SAMPLE_ONCE ==
                g_fakeRuntime.acquisitionMessages[0]);

    fake_clear_outputs();
    g_fakeRuntime.buttonEvent = SERVICE_BUTTON_EVENT_SINGLE;
    TEST_ASSERT(PLATFORM_ERR_OK == app_control_sample_button(&control, 140U));
    TEST_ASSERT(APP_CONTROL_STATE_STOPPED == control.context.state);
    TEST_ASSERT(PLATFORM_TRUE == control.context.onceActive);
    TEST_ASSERT(0U == g_fakeRuntime.acquisitionCount);
    TEST_ASSERT(1U == control.statistics.ignoredButtonEventCount);

    return 0;
}

/** @brief 验证 Control 循环以 Button deadline 计算等待时间。 */
static int test_run_once_uses_deadline_as_queue_timeout(void)
{
    platform_button_t button = PLATFORM_BUTTON_INITIALIZER;
    service_button_t buttonService = SERVICE_BUTTON_INITIALIZER;
    app_control_t control;

    fake_runtime_reset();
    control = create_control(&button, &buttonService);
    g_fakeRuntime.nowMs = 110U;
    g_fakeRuntime.buttonEvent = SERVICE_BUTTON_EVENT_NONE;

    TEST_ASSERT(PLATFORM_ERR_OK == app_control_run_once(&control));
    TEST_ASSERT(1U == g_fakeRuntime.receiveCallCount);
    TEST_ASSERT(0U == g_fakeRuntime.lastReceiveTimeoutMs);
    TEST_ASSERT(1U == g_fakeRuntime.buttonReadCount);
    TEST_ASSERT(1U == g_fakeRuntime.buttonProcessCount);
    TEST_ASSERT(120U == control.context.nextButtonSampleDeadlineMs);

    return 0;
}

platform_error_t platform_queue_send(
    platform_queue_t *queue,
    const void *item,
    uint32_t timeoutMs)
{
    TEST_ASSERT(PLATFORM_OS_NO_WAIT == timeoutMs);
    if (g_fakeRuntime.queueSendResult != PLATFORM_ERR_OK) {
        return g_fakeRuntime.queueSendResult;
    }
    if (queue == &g_fakeRuntime.acquisitionQueue) {
        g_fakeRuntime.acquisitionMessages[g_fakeRuntime.acquisitionCount++] =
            *(const app_acquisition_command_t *)item;
    } else if (queue == &g_fakeRuntime.communicationQueue) {
        g_fakeRuntime.communicationMessages[g_fakeRuntime.communicationCount++] =
            *(const app_communication_outbound_message_t *)item;
    } else if (queue == &g_fakeRuntime.indicatorQueue) {
        g_fakeRuntime.indicatorMessages[g_fakeRuntime.indicatorCount++] =
            *(const app_indicator_command_t *)item;
    } else {
        return PLATFORM_ERR_INVALID_PARAM;
    }
    return PLATFORM_ERR_OK;
}

platform_error_t platform_queue_receive(
    platform_queue_t *queue,
    void *item,
    uint32_t timeoutMs)
{
    TEST_ASSERT(queue == &g_fakeRuntime.controlQueue);
    g_fakeRuntime.receiveCallCount++;
    g_fakeRuntime.lastReceiveTimeoutMs = timeoutMs;
    if (g_fakeRuntime.queueReceiveResult == PLATFORM_ERR_OK) {
        *(app_control_message_t *)item = g_fakeRuntime.receiveMessage;
    }
    return g_fakeRuntime.queueReceiveResult;
}

platform_error_t platform_button_read(
    platform_button_t *button,
    platform_button_state_t *state)
{
    (void)button;
    g_fakeRuntime.buttonReadCount++;
    *state = g_fakeRuntime.buttonState;
    return g_fakeRuntime.buttonReadResult;
}

platform_error_t service_button_process(
    service_button_t *service,
    platform_button_state_t buttonState,
    uint32_t nowMs,
    service_button_event_t *event)
{
    (void)service;
    (void)buttonState;
    (void)nowMs;
    g_fakeRuntime.buttonProcessCount++;
    *event = g_fakeRuntime.buttonEvent;
    return g_fakeRuntime.buttonProcessResult;
}

platform_error_t platform_time_get_ms(uint32_t *timeMs)
{
    *timeMs = g_fakeRuntime.nowMs;
    return g_fakeRuntime.timeResult;
}

platform_error_t platform_time_delay_ms(uint32_t delayMs)
{
    (void)delayMs;
    return PLATFORM_ERR_OK;
}

int main(void)
{
    int result = test_boots_stopped_and_initializes_deadline();

    if (result != 0) {
        return result;
    }
    result = test_uart_start_and_stop_share_one_state_machine();
    if (result != 0) {
        return result;
    }
    result = test_once_busy_status_and_failure_completion();
    if (result != 0) {
        return result;
    }
    result = test_once_tx_completion_controls_success_indicator();
    if (result != 0) {
        return result;
    }
    result = test_queue_failure_is_observable_and_does_not_change_state();
    if (result != 0) {
        return result;
    }
    result = test_button_gestures_map_into_same_fsm();
    if (result != 0) {
        return result;
    }
    return test_run_once_uses_deadline_as_queue_timeout();
}
