/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_app_communication.c
 * @brief 验证通信 APP 对象初始化和观测接口。
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#include "app_communication.h"
#include "app_control_types.h"
#include "service_log.h"

#include <string.h>

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            return __LINE__; \
        } \
    } while (0)

typedef struct
{
    uint32_t initCallCount;
    uint32_t startCallCount;
    uint32_t stopCallCount;
    uint32_t serviceStartCallCount;
    uint32_t serviceStopCallCount;
    uint32_t waitEventCallCount;
    uint32_t readCallCount;
    uint32_t getStatusCallCount;
    platform_error_t initResult;
    platform_error_t startResult;
    platform_error_t stopResult;
    platform_error_t serviceStartResult;
    platform_error_t serviceStopResult;
    platform_error_t waitEventResult;
    platform_error_t readResult;
    platform_error_t writeResult;
    platform_error_t controlHandlerResult;
    uint32_t events;
    uint8_t readData[128];
    platform_size_t readLength;
    service_uart_status_t serviceStatus;
    uint32_t writeCallCount;
    uint8_t writtenData[64];
    platform_size_t writtenLength;
    uint32_t controlEventCount;
    app_ctrl_event_t controlEvents[8];
    uint32_t logCallCount;
    platform_log_level_t logLevel;
    const char *logTag;
    const char *logFormat;
} fake_runtime_t;

static fake_runtime_t g_fakeRuntime;

static void fake_runtime_reset(void)
{
    g_fakeRuntime = (fake_runtime_t){0};
    g_fakeRuntime.initResult = PLATFORM_ERR_OK;
    g_fakeRuntime.startResult = PLATFORM_ERR_OK;
    g_fakeRuntime.stopResult = PLATFORM_ERR_OK;
    g_fakeRuntime.serviceStartResult = PLATFORM_ERR_OK;
    g_fakeRuntime.serviceStopResult = PLATFORM_ERR_OK;
    g_fakeRuntime.waitEventResult = PLATFORM_ERR_TIMEOUT;
    g_fakeRuntime.readResult = PLATFORM_ERR_EMPTY;
    g_fakeRuntime.writeResult = PLATFORM_ERR_OK;
    g_fakeRuntime.controlHandlerResult = PLATFORM_ERR_OK;
    g_fakeRuntime.serviceStatus.state = SERVICE_UART_STATE_RUNNING;
    g_fakeRuntime.serviceStatus.lastError = PLATFORM_ERR_OK;
}

static platform_error_t fake_uart_lifecycle_init(void *self)
{
    (void)self;
    g_fakeRuntime.initCallCount++;
    return g_fakeRuntime.initResult;
}

static platform_error_t fake_uart_lifecycle_start(void *self)
{
    (void)self;
    g_fakeRuntime.startCallCount++;
    return g_fakeRuntime.startResult;
}

static platform_error_t fake_uart_lifecycle_stop(void *self)
{
    (void)self;
    g_fakeRuntime.stopCallCount++;
    return g_fakeRuntime.stopResult;
}

platform_error_t service_uart_start(service_uart_t *service)
{
    (void)service;
    g_fakeRuntime.serviceStartCallCount++;
    return g_fakeRuntime.serviceStartResult;
}

platform_error_t service_uart_stop(service_uart_t *service)
{
    (void)service;
    g_fakeRuntime.serviceStopCallCount++;
    return g_fakeRuntime.serviceStopResult;
}

platform_error_t service_uart_wait_event(service_uart_t *service,
                                         uint32_t timeoutMs,
                                         uint32_t *events)
{
    (void)service;
    (void)timeoutMs;
    g_fakeRuntime.waitEventCallCount++;
    *events = g_fakeRuntime.events;
    return g_fakeRuntime.waitEventResult;
}

platform_error_t service_uart_read(
    service_uart_t *service,
    uint8_t *buffer,
    platform_size_t bufferSize,
    platform_size_t *readLength)
{
    (void)service;
    g_fakeRuntime.readCallCount++;
    if (PLATFORM_ERR_OK == g_fakeRuntime.readResult) {
        if (bufferSize < g_fakeRuntime.readLength) {
            return PLATFORM_ERR_INVALID_PARAM;
        }
        memcpy(buffer, g_fakeRuntime.readData, g_fakeRuntime.readLength);
        *readLength = g_fakeRuntime.readLength;
        g_fakeRuntime.readResult = PLATFORM_ERR_EMPTY;
        return PLATFORM_ERR_OK;
    }

    *readLength = 0U;
    return g_fakeRuntime.readResult;
}

platform_error_t service_uart_write(service_uart_t *service,
                                    const uint8_t *data,
                                    platform_size_t dataLength,
                                    uint32_t timeoutMs)
{
    (void)service;
    (void)timeoutMs;
    g_fakeRuntime.writeCallCount++;

    if (dataLength > sizeof(g_fakeRuntime.writtenData)) {
        return PLATFORM_ERR_OVERFLOW;
    }

    memcpy(g_fakeRuntime.writtenData, data, dataLength);
    g_fakeRuntime.writtenLength = dataLength;

    return g_fakeRuntime.writeResult;
}

static platform_error_t fake_control_handler(void *context, app_ctrl_event_t event)
{
    (void)context;

    if (g_fakeRuntime.controlEventCount >=
        (sizeof(g_fakeRuntime.controlEvents) / sizeof(g_fakeRuntime.controlEvents[0]))) {
        return PLATFORM_ERR_OVERFLOW;
    }

    g_fakeRuntime.controlEvents[g_fakeRuntime.controlEventCount] = event;
    g_fakeRuntime.controlEventCount++;

    return g_fakeRuntime.controlHandlerResult;
}

static void fake_runtime_queue_rx(const char *data)
{
    g_fakeRuntime.readLength = strlen(data);
    memcpy(g_fakeRuntime.readData, data, g_fakeRuntime.readLength);
    g_fakeRuntime.readResult = PLATFORM_ERR_OK;
    g_fakeRuntime.waitEventResult = PLATFORM_ERR_OK;
    g_fakeRuntime.events = SERVICE_UART_EVENT_RX_AVAILABLE;
}

platform_error_t service_uart_get_status(const service_uart_t *service,
                                         service_uart_status_t *status)
{
    (void)service;
    g_fakeRuntime.getStatusCallCount++;
    *status = g_fakeRuntime.serviceStatus;
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
    g_fakeRuntime.logCallCount++;
    g_fakeRuntime.logLevel = (platform_log_level_t)level;
    g_fakeRuntime.logTag = tag;
    (void)file;
    (void)func;
    (void)line;
    g_fakeRuntime.logFormat = format;
}

platform_log_output_fn_t platform_log_get_output_fn(void)
{
    return fake_log_output;
}

static int test_init_and_getters(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {&uart, &service, NULL, NULL};
    app_communication_status_t status = {0};
    app_communication_statistics_t statistics = {0};

    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == app_communication_init(NULL, &config));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == app_communication_init(&communication, NULL));
    config.uart = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == app_communication_init(&communication, &config));
    config.uart = &uart;
    config.service = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == app_communication_init(&communication, &config));
    config.service = &service;
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                app_communication_get_status(&communication, &status));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                app_communication_get_statistics(&communication, &statistics));
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    TEST_ASSERT(APP_COMMUNICATION_STATE_INITIALIZED == communication.context.state);
    TEST_ASSERT(&uart == communication.config.uart);
    TEST_ASSERT(&service == communication.config.service);
    TEST_ASSERT(PLATFORM_ERR_OK == communication.context.lastError);
    TEST_ASSERT(0U == communication.statistics.processedChunkCount);
    TEST_ASSERT(PLATFORM_ERR_ALREADY_INITIALIZED ==
                app_communication_init(&communication, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_get_status(&communication, &status));
    TEST_ASSERT(APP_COMMUNICATION_STATE_INITIALIZED == status.state);
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_get_statistics(&communication, &statistics));
    TEST_ASSERT(0U == statistics.fatalErrorCount);

    return 0;
}

static int test_control_event_contract_exposes_frozen_events(void)
{
    app_ctrl_event_t startEvent = APP_CTRL_START;
    app_ctrl_event_t stopEvent = APP_CTRL_STOP;
    app_ctrl_event_t onceEvent = APP_CTRL_SAMPLE_ONCE;
    app_ctrl_event_t statusEvent = APP_CTRL_GET_STATUS;

    TEST_ASSERT(startEvent != stopEvent);
    TEST_ASSERT(startEvent != onceEvent);
    TEST_ASSERT(startEvent != statusEvent);
    TEST_ASSERT(stopEvent != onceEvent);
    TEST_ASSERT(stopEvent != statusEvent);
    TEST_ASSERT(onceEvent != statusEvent);

    return 0;
}

static int test_start_runs_uart_then_service(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {&uart, &service, NULL, NULL};
    platform_lifecycle_ops_t lifecycle = {0};

    lifecycle.init = fake_uart_lifecycle_init;
    lifecycle.start = fake_uart_lifecycle_start;
    lifecycle.stop = fake_uart_lifecycle_stop;
    uart.device.lifecycle = &lifecycle;
    fake_runtime_reset();

    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_start(&communication));
    TEST_ASSERT(1U == g_fakeRuntime.initCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.startCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.serviceStartCallCount);
    TEST_ASSERT(APP_COMMUNICATION_STATE_RUNNING == communication.context.state);
    TEST_ASSERT(1U == g_fakeRuntime.logCallCount);
    TEST_ASSERT(PLATFORM_LOG_LEVEL_INFO == g_fakeRuntime.logLevel);
    TEST_ASSERT(0 == strcmp("app_comm", g_fakeRuntime.logTag));
    TEST_ASSERT(0 == strcmp("communication runtime started", g_fakeRuntime.logFormat));

    return 0;
}

static int test_start_failure_stops_following_operations(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {&uart, &service, NULL, NULL};
    platform_lifecycle_ops_t lifecycle = {0};

    lifecycle.init = fake_uart_lifecycle_init;
    lifecycle.start = fake_uart_lifecycle_start;
    lifecycle.stop = fake_uart_lifecycle_stop;
    uart.device.lifecycle = &lifecycle;
    fake_runtime_reset();
    g_fakeRuntime.initResult = PLATFORM_ERR_IO;

    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    TEST_ASSERT(PLATFORM_ERR_IO == app_communication_start(&communication));
    TEST_ASSERT(1U == g_fakeRuntime.initCallCount);
    TEST_ASSERT(0U == g_fakeRuntime.startCallCount);
    TEST_ASSERT(0U == g_fakeRuntime.serviceStartCallCount);
    TEST_ASSERT(APP_COMMUNICATION_STATE_ERROR == communication.context.state);
    TEST_ASSERT(PLATFORM_ERR_IO == communication.context.lastError);
    TEST_ASSERT(1U == communication.statistics.fatalErrorCount);

    return 0;
}

static int test_service_start_failure_rolls_back_uart(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {&uart, &service, NULL, NULL};
    platform_lifecycle_ops_t lifecycle = {0};

    lifecycle.init = fake_uart_lifecycle_init;
    lifecycle.start = fake_uart_lifecycle_start;
    lifecycle.stop = fake_uart_lifecycle_stop;
    uart.device.lifecycle = &lifecycle;
    fake_runtime_reset();
    g_fakeRuntime.serviceStartResult = PLATFORM_ERR_IO;

    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    TEST_ASSERT(PLATFORM_ERR_IO == app_communication_start(&communication));
    TEST_ASSERT(1U == g_fakeRuntime.initCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.startCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.serviceStartCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.stopCallCount);
    TEST_ASSERT(APP_COMMUNICATION_STATE_ERROR == communication.context.state);

    return 0;
}

static int test_process_drains_rx_and_treats_timeout_as_idle(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {&uart, &service, NULL, NULL};

    fake_runtime_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    communication.context.state = APP_COMMUNICATION_STATE_RUNNING;
    g_fakeRuntime.waitEventResult = PLATFORM_ERR_OK;
    g_fakeRuntime.events = SERVICE_UART_EVENT_RX_AVAILABLE;
    g_fakeRuntime.readResult = PLATFORM_ERR_OK;
    g_fakeRuntime.readLength = 2U;
    g_fakeRuntime.readData[0] = 0x12U;
    g_fakeRuntime.readData[1] = 0x34U;

    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    TEST_ASSERT(2U == g_fakeRuntime.readCallCount);
    TEST_ASSERT(1U == communication.statistics.processedChunkCount);
    TEST_ASSERT(2U == communication.statistics.processedByteCount);

    g_fakeRuntime.waitEventResult = PLATFORM_ERR_TIMEOUT;
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    TEST_ASSERT(APP_COMMUNICATION_STATE_RUNNING == communication.context.state);

    return 0;
}

static int test_process_reassembles_fragmented_start_once(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {
        .uart = &uart,
        .service = &service,
        .controlHandler = fake_control_handler,
        .controlContext = NULL
    };

    fake_runtime_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    communication.context.state = APP_COMMUNICATION_STATE_RUNNING;

    fake_runtime_queue_rx("STA");
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    TEST_ASSERT(0U == g_fakeRuntime.controlEventCount);

    fake_runtime_queue_rx("RT\r\n");
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    TEST_ASSERT(1U == g_fakeRuntime.controlEventCount);
    TEST_ASSERT(APP_CTRL_START == g_fakeRuntime.controlEvents[0]);
    TEST_ASSERT(APP_COMMUNICATION_STATE_RUNNING == communication.context.state);

    return 0;
}

static int test_process_accepts_crlf_split_across_reads(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {
        .uart = &uart,
        .service = &service,
        .controlHandler = fake_control_handler,
        .controlContext = NULL
    };

    fake_runtime_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    communication.context.state = APP_COMMUNICATION_STATE_RUNNING;

    fake_runtime_queue_rx("STOP\r");
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    TEST_ASSERT(0U == g_fakeRuntime.controlEventCount);

    fake_runtime_queue_rx("\n");
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    TEST_ASSERT(1U == g_fakeRuntime.controlEventCount);
    TEST_ASSERT(APP_CTRL_STOP == g_fakeRuntime.controlEvents[0]);

    return 0;
}

static int test_process_accounts_for_control_and_local_response_failures(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {
        .uart = &uart,
        .service = &service,
        .controlHandler = fake_control_handler,
        .controlContext = NULL
    };

    fake_runtime_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    communication.context.state = APP_COMMUNICATION_STATE_RUNNING;
    g_fakeRuntime.controlHandlerResult = PLATFORM_ERR_IO;
    fake_runtime_queue_rx("START\r\n");

    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    TEST_ASSERT(1U == g_fakeRuntime.controlEventCount);
    TEST_ASSERT(0U == communication.statistics.controlEventSubmittedCount);
    TEST_ASSERT(1U == communication.statistics.controlEventSubmitFailureCount);

    g_fakeRuntime.writeResult = PLATFORM_ERR_IO;
    fake_runtime_queue_rx("HELP\r\n");
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    TEST_ASSERT(1U == g_fakeRuntime.writeCallCount);
    TEST_ASSERT(0U == communication.statistics.localResponseCount);
    TEST_ASSERT(1U == communication.statistics.localResponseFailureCount);

    return 0;
}

static int test_process_parses_coalesced_controls_and_handles_help_locally(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {
        .uart = &uart,
        .service = &service,
        .controlHandler = fake_control_handler,
        .controlContext = NULL
    };

    fake_runtime_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    communication.context.state = APP_COMMUNICATION_STATE_RUNNING;
    fake_runtime_queue_rx("START\r\nSTOP\r\nONCE\r\nSTATUS\r\nHELP\r\n");

    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    TEST_ASSERT(4U == g_fakeRuntime.controlEventCount);
    TEST_ASSERT(APP_CTRL_START == g_fakeRuntime.controlEvents[0]);
    TEST_ASSERT(APP_CTRL_STOP == g_fakeRuntime.controlEvents[1]);
    TEST_ASSERT(APP_CTRL_SAMPLE_ONCE == g_fakeRuntime.controlEvents[2]);
    TEST_ASSERT(APP_CTRL_GET_STATUS == g_fakeRuntime.controlEvents[3]);
    TEST_ASSERT(1U == g_fakeRuntime.writeCallCount);
    TEST_ASSERT((sizeof("HELP START STOP ONCE STATUS HELP\r\n") - 1U) ==
                g_fakeRuntime.writtenLength);
    TEST_ASSERT(0 == memcmp("HELP START STOP ONCE STATUS HELP\r\n",
                            g_fakeRuntime.writtenData,
                            g_fakeRuntime.writtenLength));
    TEST_ASSERT(5U == communication.statistics.commandReceivedCount);
    TEST_ASSERT(4U == communication.statistics.controlEventSubmittedCount);
    TEST_ASSERT(1U == communication.statistics.localResponseCount);

    return 0;
}

static int test_process_rejects_invalid_protocol_without_fatal_state(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {
        .uart = &uart,
        .service = &service,
        .controlHandler = fake_control_handler,
        .controlContext = NULL
    };

    fake_runtime_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    communication.context.state = APP_COMMUNICATION_STATE_RUNNING;

    fake_runtime_queue_rx("start\r\n");
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    fake_runtime_queue_rx(" START\r\n");
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    fake_runtime_queue_rx("START \r\n");
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    fake_runtime_queue_rx("START\n\r\n");
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    fake_runtime_queue_rx("\r\n");
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));

    TEST_ASSERT(APP_COMMUNICATION_STATE_RUNNING == communication.context.state);
    TEST_ASSERT(0U == g_fakeRuntime.controlEventCount);
    TEST_ASSERT(3U == g_fakeRuntime.writeCallCount);
    TEST_ASSERT(4U == communication.statistics.commandInvalidCount);
    TEST_ASSERT(0U == communication.statistics.fatalErrorCount);

    return 0;
}

static int test_process_recovers_after_malformed_cr_and_overflow(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {
        .uart = &uart,
        .service = &service,
        .controlHandler = fake_control_handler,
        .controlContext = NULL
    };
    char maxLengthLine[34] = {0};
    char overflowLine[41] = {0};

    fake_runtime_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    communication.context.state = APP_COMMUNICATION_STATE_RUNNING;

    fake_runtime_queue_rx("START\rX\r\nONCE\r\n");
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    TEST_ASSERT(1U == g_fakeRuntime.controlEventCount);
    TEST_ASSERT(APP_CTRL_SAMPLE_ONCE == g_fakeRuntime.controlEvents[0]);

    memset(maxLengthLine, 'A', 31U);
    maxLengthLine[31] = '\r';
    maxLengthLine[32] = '\n';
    fake_runtime_queue_rx(maxLengthLine);
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));

    memset(overflowLine, 'B', 32U);
    overflowLine[32] = '\r';
    overflowLine[33] = '\n';
    memcpy(&overflowLine[34], "STOP\r\n", 7U);
    fake_runtime_queue_rx(overflowLine);
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));

    TEST_ASSERT(2U == g_fakeRuntime.controlEventCount);
    TEST_ASSERT(APP_CTRL_STOP == g_fakeRuntime.controlEvents[1]);
    TEST_ASSERT(1U == communication.statistics.commandOverflowCount);
    TEST_ASSERT(2U == g_fakeRuntime.writeCallCount);
    TEST_ASSERT(0 == memcmp("ERR COMMAND_TOO_LONG\r\n",
                            g_fakeRuntime.writtenData,
                            g_fakeRuntime.writtenLength));
    TEST_ASSERT(APP_COMMUNICATION_STATE_RUNNING == communication.context.state);

    return 0;
}

static int test_process_prioritizes_error_recovery_after_drain(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {&uart, &service, NULL, NULL};

    fake_runtime_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    communication.context.state = APP_COMMUNICATION_STATE_RUNNING;
    g_fakeRuntime.waitEventResult = PLATFORM_ERR_OK;
    g_fakeRuntime.events = SERVICE_UART_EVENT_RX_AVAILABLE | SERVICE_UART_EVENT_DATA_LOSS |
                          SERVICE_UART_EVENT_ERROR;
    g_fakeRuntime.readResult = PLATFORM_ERR_OK;
    g_fakeRuntime.readLength = 2U;

    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    TEST_ASSERT(2U == g_fakeRuntime.readCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.getStatusCallCount);
    TEST_ASSERT(0U == g_fakeRuntime.serviceStopCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.serviceStartCallCount);
    TEST_ASSERT(1U == communication.statistics.uartErrorRecoveryCount);
    TEST_ASSERT(0U == communication.statistics.dataLossRecoveryCount);

    return 0;
}

static int test_process_recovers_data_loss_by_stop_and_start(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {&uart, &service, NULL, NULL};

    fake_runtime_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    communication.context.state = APP_COMMUNICATION_STATE_RUNNING;
    g_fakeRuntime.waitEventResult = PLATFORM_ERR_OK;
    g_fakeRuntime.events = SERVICE_UART_EVENT_DATA_LOSS;

    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_process(&communication, 100U));
    TEST_ASSERT(1U == g_fakeRuntime.serviceStopCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.serviceStartCallCount);
    TEST_ASSERT(1U == communication.statistics.dataLossRecoveryCount);

    return 0;
}

static int test_process_treats_stopped_as_fatal(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {&uart, &service, NULL, NULL};

    fake_runtime_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    communication.context.state = APP_COMMUNICATION_STATE_RUNNING;
    g_fakeRuntime.waitEventResult = PLATFORM_ERR_OK;
    g_fakeRuntime.events = SERVICE_UART_EVENT_STOPPED;

    TEST_ASSERT(PLATFORM_ERR_CANCELED == app_communication_process(&communication, 100U));
    TEST_ASSERT(APP_COMMUNICATION_STATE_ERROR == communication.context.state);
    TEST_ASSERT(PLATFORM_ERR_CANCELED == communication.context.lastError);
    TEST_ASSERT(1U == communication.statistics.fatalErrorCount);

    return 0;
}

int main(void)
{
    int result = test_init_and_getters();

    if (0 != result) {
        return result;
    }

    result = test_control_event_contract_exposes_frozen_events();
    if (0 != result) {
        return result;
    }

    result = test_start_runs_uart_then_service();
    if (0 != result) {
        return result;
    }

    result = test_start_failure_stops_following_operations();
    if (0 != result) {
        return result;
    }

    result = test_service_start_failure_rolls_back_uart();
    if (0 != result) {
        return result;
    }

    result = test_process_drains_rx_and_treats_timeout_as_idle();
    if (0 != result) {
        return result;
    }

    result = test_process_reassembles_fragmented_start_once();
    if (0 != result) {
        return result;
    }

    result = test_process_accepts_crlf_split_across_reads();
    if (0 != result) {
        return result;
    }

    result = test_process_accounts_for_control_and_local_response_failures();
    if (0 != result) {
        return result;
    }

    result = test_process_parses_coalesced_controls_and_handles_help_locally();
    if (0 != result) {
        return result;
    }

    result = test_process_rejects_invalid_protocol_without_fatal_state();
    if (0 != result) {
        return result;
    }

    result = test_process_recovers_after_malformed_cr_and_overflow();
    if (0 != result) {
        return result;
    }

    result = test_process_prioritizes_error_recovery_after_drain();
    if (0 != result) {
        return result;
    }

    result = test_process_recovers_data_loss_by_stop_and_start();
    if (0 != result) {
        return result;
    }

    return test_process_treats_stopped_as_fatal();
}
