/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_app_communication_outbound.c
 * @brief 验证业务响应、传感器报告和 ONCE TX 完成语义。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#include "app_communication.h"
#include "app_ipc_types.h"
#include "platform_queue.h"
#include "service_log.h"

#include <stdarg.h>
#include <string.h>

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            return __LINE__; \
        } \
    } while (0)

#define TEST_MAX_MESSAGES (16U)
#define TEST_MAX_WRITES (20U)
#define TEST_WRITE_SIZE (160U)

typedef struct
{
    platform_queue_t outboundQueue;
    platform_queue_t controlQueue;
    app_communication_outbound_message_t outbound[TEST_MAX_MESSAGES];
    app_control_message_t control[TEST_MAX_MESSAGES];
    platform_error_t writeResults[TEST_MAX_WRITES];
    char writes[TEST_MAX_WRITES][TEST_WRITE_SIZE];
    uint32_t outboundReadIndex;
    uint32_t outboundCount;
    uint32_t controlCount;
    uint32_t writeCount;
    platform_error_t controlHandlerResult;
    uint8_t rxData[32];
    platform_size_t rxLength;
    platform_bool_t rxReady;
} fake_outbound_runtime_t;

static fake_outbound_runtime_t g_fakeRuntime;

static void fake_runtime_reset(void)
{
    uint32_t index;

    memset(&g_fakeRuntime, 0, sizeof(g_fakeRuntime));
    g_fakeRuntime.outboundQueue.native = &g_fakeRuntime.outboundQueue;
    g_fakeRuntime.controlQueue.native = &g_fakeRuntime.controlQueue;
    g_fakeRuntime.controlHandlerResult = PLATFORM_ERR_OK;
    for (index = 0U; index < TEST_MAX_WRITES; index++) {
        g_fakeRuntime.writeResults[index] = PLATFORM_ERR_OK;
    }
}

static app_communication_t create_communication(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    static platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    static service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {
        .uart = &uart,
        .service = &service,
        .controlHandler = NULL,
        .controlContext = NULL,
        .outboundQueue = &g_fakeRuntime.outboundQueue,
        .controlQueue = &g_fakeRuntime.controlQueue
    };

    (void)app_communication_init(&communication, &config);
    communication.context.state = APP_COMMUNICATION_STATE_RUNNING;
    return communication;
}

static void fake_enqueue_outbound(app_communication_outbound_message_t message)
{
    g_fakeRuntime.outbound[g_fakeRuntime.outboundCount++] = message;
}

static int test_formats_all_control_responses(void)
{
    static const char *expected[] = {
        "OK START\r\n",
        "OK STOP\r\n",
        "ERR ALREADY_RUNNING\r\n",
        "ERR ALREADY_STOPPED\r\n",
        "ERR BUSY\r\n",
        "ERR ACQUISITION_FAILED\r\n",
        "STATUS RUNNING\r\n",
        "STATUS STOPPED\r\n"
    };
    app_communication_t communication;
    app_communication_outbound_message_t message = {
        .type = APP_COMM_OUTBOUND_CONTROL_RESPONSE
    };
    uint32_t index;

    fake_runtime_reset();
    communication = create_communication();
    for (index = 0U; index < (sizeof(expected) / sizeof(expected[0])); index++) {
        message.payload.controlResponse = (app_control_response_t)index;
        fake_enqueue_outbound(message);
    }

    TEST_ASSERT(app_communication_drain_outbound(&communication) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.writeCount == 8U);
    for (index = 0U; index < g_fakeRuntime.writeCount; index++) {
        TEST_ASSERT(strcmp(g_fakeRuntime.writes[index], expected[index]) == 0);
    }

    return 0;
}

static int test_formats_complete_periodic_report(void)
{
    app_communication_t communication;
    app_communication_outbound_message_t message = {
        .type = APP_COMM_OUTBOUND_PERIODIC_REPORT
    };

    fake_runtime_reset();
    communication = create_communication();
    message.payload.acquisition.environment.temperatureC = 25.34F;
    message.payload.acquisition.environment.humidityPercent = 62.18F;
    message.payload.acquisition.motion.accelXG = 0.013F;
    message.payload.acquisition.motion.accelYG = -0.021F;
    message.payload.acquisition.motion.accelZG = 0.998F;
    message.payload.acquisition.motion.gyroXDps = 0.12F;
    message.payload.acquisition.motion.gyroYDps = -0.42F;
    message.payload.acquisition.motion.gyroZDps = 0.08F;
    fake_enqueue_outbound(message);

    TEST_ASSERT(app_communication_drain_outbound(&communication) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.writeCount == 2U);
    TEST_ASSERT(strcmp(g_fakeRuntime.writes[0], "ENV,T=25.34,H=62.18\r\n") == 0);
    TEST_ASSERT(strcmp(g_fakeRuntime.writes[1],
                       "IMU,AX=0.013,AY=-0.021,AZ=0.998,GX=0.12,GY=-0.42,GZ=0.08\r\n") == 0);
    TEST_ASSERT(g_fakeRuntime.controlCount == 0U);

    return 0;
}

static int test_once_posts_exactly_one_tx_result_after_complete_report(void)
{
    app_communication_t communication;
    app_communication_outbound_message_t message = {
        .type = APP_COMM_OUTBOUND_ONCE_REPORT
    };

    fake_runtime_reset();
    communication = create_communication();
    fake_enqueue_outbound(message);
    TEST_ASSERT(app_communication_drain_outbound(&communication) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.writeCount == 2U);
    TEST_ASSERT(g_fakeRuntime.controlCount == 1U);
    TEST_ASSERT(g_fakeRuntime.control[0].type == APP_CONTROL_MESSAGE_ONCE_TX_RESULT);
    TEST_ASSERT(g_fakeRuntime.control[0].payload.result == PLATFORM_ERR_OK);

    fake_runtime_reset();
    communication = create_communication();
    g_fakeRuntime.writeResults[1] = PLATFORM_ERR_TIMEOUT;
    fake_enqueue_outbound(message);
    TEST_ASSERT(app_communication_drain_outbound(&communication) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.writeCount == 2U);
    TEST_ASSERT(g_fakeRuntime.controlCount == 1U);
    TEST_ASSERT(g_fakeRuntime.control[0].payload.result == PLATFORM_ERR_TIMEOUT);

    return 0;
}

static platform_error_t fake_control_handler(void *context, app_ctrl_event_t event)
{
    (void)context;
    (void)event;
    return g_fakeRuntime.controlHandlerResult;
}

static int test_control_queue_submission_failure_responds_busy(void)
{
    app_communication_t communication;
    app_communication_config_t config;

    fake_runtime_reset();
    communication = create_communication();
    config = communication.config;
    config.controlHandler = fake_control_handler;
    communication = (app_communication_t)APP_COMMUNICATION_INITIALIZER;
    TEST_ASSERT(app_communication_init(&communication, &config) == PLATFORM_ERR_OK);
    communication.context.state = APP_COMMUNICATION_STATE_RUNNING;
    g_fakeRuntime.controlHandlerResult = PLATFORM_ERR_FULL;
    memcpy(g_fakeRuntime.rxData, "START\r\n", 7U);
    g_fakeRuntime.rxLength = 7U;
    g_fakeRuntime.rxReady = PLATFORM_TRUE;

    TEST_ASSERT(app_communication_process(&communication, 20U) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.writeCount == 1U);
    TEST_ASSERT(strcmp(g_fakeRuntime.writes[0], "ERR BUSY\r\n") == 0);
    TEST_ASSERT(communication.statistics.controlEventSubmitFailureCount == 1U);

    return 0;
}

platform_error_t platform_queue_receive(
    platform_queue_t *queue,
    void *item,
    uint32_t timeoutMs)
{
    TEST_ASSERT(queue == &g_fakeRuntime.outboundQueue);
    TEST_ASSERT(timeoutMs == PLATFORM_OS_NO_WAIT);
    if (g_fakeRuntime.outboundReadIndex >= g_fakeRuntime.outboundCount) {
        return PLATFORM_ERR_EMPTY;
    }
    *(app_communication_outbound_message_t *)item =
        g_fakeRuntime.outbound[g_fakeRuntime.outboundReadIndex++];
    return PLATFORM_ERR_OK;
}

platform_error_t platform_queue_send(
    platform_queue_t *queue,
    const void *item,
    uint32_t timeoutMs)
{
    TEST_ASSERT(queue == &g_fakeRuntime.controlQueue);
    TEST_ASSERT(timeoutMs == PLATFORM_OS_NO_WAIT);
    g_fakeRuntime.control[g_fakeRuntime.controlCount++] =
        *(const app_control_message_t *)item;
    return PLATFORM_ERR_OK;
}

platform_error_t service_uart_write(
    service_uart_t *service,
    const uint8_t *data,
    platform_size_t dataLength,
    uint32_t timeoutMs)
{
    uint32_t index = g_fakeRuntime.writeCount++;

    (void)service;
    (void)timeoutMs;
    TEST_ASSERT(index < TEST_MAX_WRITES);
    TEST_ASSERT(dataLength < TEST_WRITE_SIZE);
    memcpy(g_fakeRuntime.writes[index], data, dataLength);
    g_fakeRuntime.writes[index][dataLength] = '\0';
    return g_fakeRuntime.writeResults[index];
}

platform_error_t service_uart_wait_event(
    service_uart_t *service,
    uint32_t timeoutMs,
    uint32_t *events)
{
    (void)service;
    (void)timeoutMs;
    *events = g_fakeRuntime.rxReady == PLATFORM_TRUE ?
        SERVICE_UART_EVENT_RX_AVAILABLE : 0U;
    return PLATFORM_ERR_OK;
}

platform_error_t service_uart_read(
    service_uart_t *service,
    uint8_t *buffer,
    platform_size_t bufferSize,
    platform_size_t *readLength)
{
    (void)service;
    if (g_fakeRuntime.rxReady != PLATFORM_TRUE) {
        *readLength = 0U;
        return PLATFORM_ERR_EMPTY;
    }
    TEST_ASSERT(bufferSize >= g_fakeRuntime.rxLength);
    memcpy(buffer, g_fakeRuntime.rxData, g_fakeRuntime.rxLength);
    *readLength = g_fakeRuntime.rxLength;
    g_fakeRuntime.rxReady = PLATFORM_FALSE;
    return PLATFORM_ERR_OK;
}

platform_error_t service_uart_start(service_uart_t *service)
{
    (void)service;
    return PLATFORM_ERR_OK;
}

platform_error_t service_uart_stop(service_uart_t *service)
{
    (void)service;
    return PLATFORM_ERR_OK;
}

platform_error_t service_uart_get_status(
    const service_uart_t *service,
    service_uart_status_t *status)
{
    (void)service;
    status->state = SERVICE_UART_STATE_RUNNING;
    status->lastError = PLATFORM_ERR_OK;
    status->dataLossOccurred = PLATFORM_FALSE;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_time_delay_ms(uint32_t delayMs)
{
    (void)delayMs;
    return PLATFORM_ERR_OK;
}

static void fake_log_output(
    uint8_t level,
    const char *tag,
    const char *file,
    const char *func,
    long line,
    const char *format,
    ...)
{
    (void)level;
    (void)tag;
    (void)file;
    (void)func;
    (void)line;
    (void)format;
}

platform_log_output_fn_t platform_log_get_output_fn(void)
{
    return fake_log_output;
}

int main(void)
{
    int result = test_formats_all_control_responses();

    if (result != 0) {
        return result;
    }
    result = test_formats_complete_periodic_report();
    if (result != 0) {
        return result;
    }
    result = test_once_posts_exactly_one_tx_result_after_complete_report();
    if (result != 0) {
        return result;
    }
    return test_control_queue_submission_failure_responds_busy();
}
