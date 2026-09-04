/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_app_acquisition.c
 * @brief 验证 Acquisition Task 的绝对周期、STOP 抑制和 ONCE 发布。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#include "app_acquisition.h"

#include <string.h>

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            return __LINE__; \
        } \
    } while (0)

typedef struct
{
    platform_queue_t commandQueue;
    platform_queue_t communicationQueue;
    platform_queue_t controlQueue;
    app_acquisition_command_t commands[8];
    app_communication_outbound_message_t outboundMessages[8];
    app_control_message_t controlMessages[8];
    service_acquisition_data_t sampleData;
    platform_error_t sampleResult;
    platform_error_t queueSendResult;
    uint32_t commandReadIndex;
    uint32_t commandCount;
    uint32_t outboundCount;
    uint32_t controlCount;
    uint32_t sampleCallCount;
    uint32_t receiveCallCount;
    uint32_t receiveTimeouts[8];
    uint32_t nowMs;
    uint32_t advanceDuringSampleMs;
    platform_bool_t enqueueStopDuringSample;
} fake_acquisition_task_runtime_t;

static fake_acquisition_task_runtime_t g_fakeRuntime;

static void fake_runtime_reset(void)
{
    memset(&g_fakeRuntime, 0, sizeof(g_fakeRuntime));
    g_fakeRuntime.commandQueue.native = &g_fakeRuntime.commandQueue;
    g_fakeRuntime.communicationQueue.native = &g_fakeRuntime.communicationQueue;
    g_fakeRuntime.controlQueue.native = &g_fakeRuntime.controlQueue;
    g_fakeRuntime.sampleResult = PLATFORM_ERR_OK;
    g_fakeRuntime.queueSendResult = PLATFORM_ERR_OK;
    g_fakeRuntime.sampleData.environment.temperatureC = 25.0F;
    g_fakeRuntime.sampleData.environment.humidityPercent = 50.0F;
    g_fakeRuntime.sampleData.motion.accelZG = 1.0F;
    g_fakeRuntime.nowMs = 100U;
}

static void fake_enqueue_command(app_acquisition_command_t command)
{
    g_fakeRuntime.commands[g_fakeRuntime.commandCount++] = command;
}

static app_acquisition_t create_acquisition(service_acquisition_t *service)
{
    app_acquisition_t acquisition = APP_ACQUISITION_INITIALIZER;
    app_acquisition_config_t config = {
        .service = service,
        .commandQueue = &g_fakeRuntime.commandQueue,
        .communicationQueue = &g_fakeRuntime.communicationQueue,
        .controlQueue = &g_fakeRuntime.controlQueue
    };

    service->initialized = PLATFORM_TRUE;
    (void)app_acquisition_init(&acquisition, &config);
    return acquisition;
}

static int test_init_validates_dependencies_and_starts_disabled(void)
{
    app_acquisition_t acquisition = APP_ACQUISITION_INITIALIZER;
    service_acquisition_t service = SERVICE_ACQUISITION_INITIALIZER;
    app_acquisition_config_t config = {
        .service = &service,
        .commandQueue = &g_fakeRuntime.commandQueue,
        .communicationQueue = &g_fakeRuntime.communicationQueue,
        .controlQueue = &g_fakeRuntime.controlQueue
    };

    fake_runtime_reset();
    TEST_ASSERT(app_acquisition_init(NULL, &config) == PLATFORM_ERR_NULL_POINTER);
    TEST_ASSERT(app_acquisition_init(&acquisition, NULL) == PLATFORM_ERR_NULL_POINTER);
    TEST_ASSERT(app_acquisition_init(&acquisition, &config) == PLATFORM_ERR_NOT_INITIALIZED);
    service.initialized = PLATFORM_TRUE;
    TEST_ASSERT(app_acquisition_init(&acquisition, &config) == PLATFORM_ERR_OK);
    TEST_ASSERT(acquisition.context.periodicEnabled == PLATFORM_FALSE);
    TEST_ASSERT(acquisition.context.initialized == PLATFORM_TRUE);
    TEST_ASSERT(app_acquisition_init(&acquisition, &config) == PLATFORM_ERR_ALREADY_INITIALIZED);

    return 0;
}

static int test_start_samples_immediately_and_sets_absolute_deadline(void)
{
    service_acquisition_t service = SERVICE_ACQUISITION_INITIALIZER;
    app_acquisition_t acquisition;

    fake_runtime_reset();
    acquisition = create_acquisition(&service);
    fake_enqueue_command(APP_ACQUISITION_COMMAND_START_PERIODIC);

    TEST_ASSERT(app_acquisition_run_once(&acquisition) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.receiveTimeouts[0] == PLATFORM_OS_WAIT_FOREVER);
    TEST_ASSERT(g_fakeRuntime.sampleCallCount == 1U);
    TEST_ASSERT(acquisition.context.periodicEnabled == PLATFORM_TRUE);
    TEST_ASSERT(acquisition.context.nextSampleDeadlineMs == 2100U);
    TEST_ASSERT(g_fakeRuntime.outboundCount == 1U);
    TEST_ASSERT(g_fakeRuntime.outboundMessages[0].type == APP_COMM_OUTBOUND_PERIODIC_REPORT);
    TEST_ASSERT(g_fakeRuntime.outboundMessages[0].payload.acquisition.environment.temperatureC == 25.0F);

    return 0;
}

static int test_periodic_deadline_does_not_accumulate_sample_time(void)
{
    service_acquisition_t service = SERVICE_ACQUISITION_INITIALIZER;
    app_acquisition_t acquisition;

    fake_runtime_reset();
    acquisition = create_acquisition(&service);
    fake_enqueue_command(APP_ACQUISITION_COMMAND_START_PERIODIC);
    TEST_ASSERT(app_acquisition_run_once(&acquisition) == PLATFORM_ERR_OK);

    g_fakeRuntime.nowMs = 2100U;
    g_fakeRuntime.advanceDuringSampleMs = 500U;
    TEST_ASSERT(app_acquisition_run_once(&acquisition) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.receiveTimeouts[2] == PLATFORM_OS_NO_WAIT);
    TEST_ASSERT(g_fakeRuntime.sampleCallCount == 2U);
    TEST_ASSERT(acquisition.context.nextSampleDeadlineMs == 4100U);
    TEST_ASSERT(g_fakeRuntime.outboundCount == 2U);

    return 0;
}

static int test_overrun_skips_missed_periods_without_catch_up_burst(void)
{
    service_acquisition_t service = SERVICE_ACQUISITION_INITIALIZER;
    app_acquisition_t acquisition;

    fake_runtime_reset();
    acquisition = create_acquisition(&service);
    fake_enqueue_command(APP_ACQUISITION_COMMAND_START_PERIODIC);
    TEST_ASSERT(app_acquisition_run_once(&acquisition) == PLATFORM_ERR_OK);

    g_fakeRuntime.nowMs = 2100U;
    g_fakeRuntime.advanceDuringSampleMs = 4500U;
    TEST_ASSERT(app_acquisition_run_once(&acquisition) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.sampleCallCount == 2U);
    TEST_ASSERT(acquisition.context.nextSampleDeadlineMs == 8100U);
    TEST_ASSERT(acquisition.statistics.skippedPeriodCount == 2U);

    g_fakeRuntime.advanceDuringSampleMs = 0U;
    TEST_ASSERT(app_acquisition_run_once(&acquisition) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.receiveTimeouts[4] == 1500U);
    TEST_ASSERT(g_fakeRuntime.sampleCallCount == 2U);

    return 0;
}

static int test_stop_arriving_during_sample_discards_stale_periodic_result(void)
{
    service_acquisition_t service = SERVICE_ACQUISITION_INITIALIZER;
    app_acquisition_t acquisition;

    fake_runtime_reset();
    acquisition = create_acquisition(&service);
    fake_enqueue_command(APP_ACQUISITION_COMMAND_START_PERIODIC);
    g_fakeRuntime.enqueueStopDuringSample = PLATFORM_TRUE;

    TEST_ASSERT(app_acquisition_run_once(&acquisition) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.sampleCallCount == 1U);
    TEST_ASSERT(acquisition.context.periodicEnabled == PLATFORM_FALSE);
    TEST_ASSERT(g_fakeRuntime.outboundCount == 0U);
    TEST_ASSERT(acquisition.statistics.stalePeriodicDiscardCount == 1U);

    TEST_ASSERT(app_acquisition_run_once(&acquisition) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.receiveTimeouts[2] == PLATFORM_OS_WAIT_FOREVER);
    TEST_ASSERT(g_fakeRuntime.sampleCallCount == 1U);

    return 0;
}

static int test_once_success_and_failure_publish_to_correct_queues(void)
{
    service_acquisition_t service = SERVICE_ACQUISITION_INITIALIZER;
    app_acquisition_t acquisition;

    fake_runtime_reset();
    acquisition = create_acquisition(&service);
    fake_enqueue_command(APP_ACQUISITION_COMMAND_SAMPLE_ONCE);
    TEST_ASSERT(app_acquisition_run_once(&acquisition) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.outboundCount == 1U);
    TEST_ASSERT(g_fakeRuntime.outboundMessages[0].type == APP_COMM_OUTBOUND_ONCE_REPORT);
    TEST_ASSERT(g_fakeRuntime.controlCount == 0U);
    TEST_ASSERT(acquisition.context.periodicEnabled == PLATFORM_FALSE);

    g_fakeRuntime.sampleResult = PLATFORM_ERR_CHECKSUM;
    fake_enqueue_command(APP_ACQUISITION_COMMAND_SAMPLE_ONCE);
    TEST_ASSERT(app_acquisition_run_once(&acquisition) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.controlCount == 1U);
    TEST_ASSERT(g_fakeRuntime.controlMessages[0].type ==
                APP_CONTROL_MESSAGE_ONCE_ACQUISITION_FAILED);
    TEST_ASSERT(g_fakeRuntime.controlMessages[0].payload.result == PLATFORM_ERR_CHECKSUM);
    TEST_ASSERT(g_fakeRuntime.outboundCount == 1U);

    return 0;
}

platform_error_t platform_queue_receive(
    platform_queue_t *queue,
    void *item,
    uint32_t timeoutMs)
{
    TEST_ASSERT(queue == &g_fakeRuntime.commandQueue);
    g_fakeRuntime.receiveTimeouts[g_fakeRuntime.receiveCallCount++] = timeoutMs;
    if (g_fakeRuntime.commandReadIndex < g_fakeRuntime.commandCount) {
        *(app_acquisition_command_t *)item =
            g_fakeRuntime.commands[g_fakeRuntime.commandReadIndex++];
        return PLATFORM_ERR_OK;
    }
    return (timeoutMs == PLATFORM_OS_NO_WAIT) ? PLATFORM_ERR_EMPTY : PLATFORM_ERR_TIMEOUT;
}

platform_error_t platform_queue_send(
    platform_queue_t *queue,
    const void *item,
    uint32_t timeoutMs)
{
    TEST_ASSERT(timeoutMs == PLATFORM_OS_NO_WAIT);
    if (g_fakeRuntime.queueSendResult != PLATFORM_ERR_OK) {
        return g_fakeRuntime.queueSendResult;
    }
    if (queue == &g_fakeRuntime.communicationQueue) {
        g_fakeRuntime.outboundMessages[g_fakeRuntime.outboundCount++] =
            *(const app_communication_outbound_message_t *)item;
    } else if (queue == &g_fakeRuntime.controlQueue) {
        g_fakeRuntime.controlMessages[g_fakeRuntime.controlCount++] =
            *(const app_control_message_t *)item;
    } else {
        return PLATFORM_ERR_INVALID_PARAM;
    }
    return PLATFORM_ERR_OK;
}

platform_error_t service_acquisition_sample(
    service_acquisition_t *service,
    service_acquisition_data_t *data)
{
    (void)service;
    g_fakeRuntime.sampleCallCount++;
    g_fakeRuntime.nowMs += g_fakeRuntime.advanceDuringSampleMs;
    if (g_fakeRuntime.enqueueStopDuringSample == PLATFORM_TRUE) {
        fake_enqueue_command(APP_ACQUISITION_COMMAND_STOP_PERIODIC);
        g_fakeRuntime.enqueueStopDuringSample = PLATFORM_FALSE;
    }
    if (g_fakeRuntime.sampleResult == PLATFORM_ERR_OK) {
        *data = g_fakeRuntime.sampleData;
    }
    return g_fakeRuntime.sampleResult;
}

platform_error_t platform_time_get_ms(uint32_t *timeMs)
{
    *timeMs = g_fakeRuntime.nowMs;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_time_delay_ms(uint32_t delayMs)
{
    (void)delayMs;
    return PLATFORM_ERR_OK;
}

int main(void)
{
    int result = test_init_validates_dependencies_and_starts_disabled();

    if (result != 0) {
        return result;
    }
    result = test_start_samples_immediately_and_sets_absolute_deadline();
    if (result != 0) {
        return result;
    }
    result = test_periodic_deadline_does_not_accumulate_sample_time();
    if (result != 0) {
        return result;
    }
    result = test_overrun_skips_missed_periods_without_catch_up_burst();
    if (result != 0) {
        return result;
    }
    result = test_stop_arriving_during_sample_discards_stale_periodic_result();
    if (result != 0) {
        return result;
    }
    return test_once_success_and_failure_publish_to_correct_queues();
}
