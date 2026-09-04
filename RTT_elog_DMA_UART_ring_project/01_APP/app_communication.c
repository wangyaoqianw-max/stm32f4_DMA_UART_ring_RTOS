/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_communication.c
 * @brief 实现通信 APP 的对象初始化与可观测性接口。
 * @author YaoQian Wang
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_communication.h"
#include "project_config.h"
#include "service_log.h"
#include "platform_time.h"

#include <stdio.h>
#include <string.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define LOG_TAG                                "app_comm"
#define APP_COMM_ENV_REPORT_BUFFER_SIZE        (64U)
#define APP_COMM_IMU_REPORT_BUFFER_SIZE        (128U)
//******************************** Defines **********************************//

//******************************** Types ***********************************//
typedef enum
{
    APP_COMMUNICATION_COMMAND_INVALID = 0,
    APP_COMMUNICATION_COMMAND_START,
    APP_COMMUNICATION_COMMAND_STOP,
    APP_COMMUNICATION_COMMAND_ONCE,
    APP_COMMUNICATION_COMMAND_STATUS,
    APP_COMMUNICATION_COMMAND_HELP
} app_communication_command_t;
//******************************** Types ***********************************//

//******************************** Constants *******************************//
static const uint8_t g_helpResponse[] = "HELP START STOP ONCE STATUS HELP\r\n";
static const uint8_t g_unknownCommandResponse[] = "ERR UNKNOWN_COMMAND\r\n";
static const uint8_t g_commandTooLongResponse[] = "ERR COMMAND_TOO_LONG\r\n";
static const uint8_t g_okStartResponse[] = "OK START\r\n";
static const uint8_t g_okStopResponse[] = "OK STOP\r\n";
static const uint8_t g_alreadyRunningResponse[] = "ERR ALREADY_RUNNING\r\n";
static const uint8_t g_alreadyStoppedResponse[] = "ERR ALREADY_STOPPED\r\n";
static const uint8_t g_busyResponse[] = "ERR BUSY\r\n";
static const uint8_t g_acquisitionFailedResponse[] = "ERR ACQUISITION_FAILED\r\n";
static const uint8_t g_statusRunningResponse[] = "STATUS RUNNING\r\n";
static const uint8_t g_statusStoppedResponse[] = "STATUS STOPPED\r\n";
//******************************** Constants *******************************//

//******************************** Functions *********************************//
static platform_error_t app_communication_set_error(
    app_communication_t *communication,
    platform_error_t error)
{
    communication->context.state = APP_COMMUNICATION_STATE_ERROR;
    communication->context.lastError = error;
    communication->statistics.fatalErrorCount++;

    return error;
}

static void app_communication_send_local_response(app_communication_t *communication,
                                                  const uint8_t *response,
                                                  platform_size_t responseLength)
{
    platform_error_t result = PLATFORM_ERR_OK;

    result = service_uart_write(communication->config.service,
                                response,
                                responseLength,
                                PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS);
    if (result == PLATFORM_ERR_OK) {
        communication->statistics.localResponseCount++;
    } else {
        communication->statistics.localResponseFailureCount++;
    }
}

static app_communication_command_t app_communication_parse_command(
    const uint8_t *line,
    platform_size_t lineLength)
{
    if ((lineLength == 5U) && (memcmp(line, "START", lineLength) == 0)) {
        return APP_COMMUNICATION_COMMAND_START;
    }

    if ((lineLength == 4U) && (memcmp(line, "STOP", lineLength) == 0)) {
        return APP_COMMUNICATION_COMMAND_STOP;
    }

    if ((lineLength == 4U) && (memcmp(line, "ONCE", lineLength) == 0)) {
        return APP_COMMUNICATION_COMMAND_ONCE;
    }

    if ((lineLength == 6U) && (memcmp(line, "STATUS", lineLength) == 0)) {
        return APP_COMMUNICATION_COMMAND_STATUS;
    }

    if ((lineLength == 4U) && (memcmp(line, "HELP", lineLength) == 0)) {
        return APP_COMMUNICATION_COMMAND_HELP;
    }

    return APP_COMMUNICATION_COMMAND_INVALID;
}

static void app_communication_submit_control_event(app_communication_t *communication,
                                                   app_ctrl_event_t event)
{
    platform_error_t result = PLATFORM_ERR_OK;

    if (communication->config.controlHandler == NULL) {
        communication->statistics.controlEventSubmitFailureCount++;
        result = service_uart_write(communication->config.service,
                                    g_busyResponse,
                                    sizeof(g_busyResponse) - 1U,
                                    PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS);
        if (result == PLATFORM_ERR_OK) {
            communication->statistics.controlResponseCount++;
        } else {
            communication->statistics.controlResponseFailureCount++;
        }
        return;
    }

    result = communication->config.controlHandler(communication->config.controlContext, event);
    if (result == PLATFORM_ERR_OK) {
        communication->statistics.controlEventSubmittedCount++;
    } else {
        communication->statistics.controlEventSubmitFailureCount++;
        result = service_uart_write(communication->config.service,
                                    g_busyResponse,
                                    sizeof(g_busyResponse) - 1U,
                                    PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS);
        if (result == PLATFORM_ERR_OK) {
            communication->statistics.controlResponseCount++;
        } else {
            communication->statistics.controlResponseFailureCount++;
        }
    }
}

static platform_error_t app_communication_get_control_response(
    app_control_response_t response,
    const uint8_t **data,
    platform_size_t *dataLength)
{
    switch (response) {
        case APP_CONTROL_RESPONSE_OK_START:
            *data = g_okStartResponse;
            *dataLength = sizeof(g_okStartResponse) - 1U;
            return PLATFORM_ERR_OK;

        case APP_CONTROL_RESPONSE_OK_STOP:
            *data = g_okStopResponse;
            *dataLength = sizeof(g_okStopResponse) - 1U;
            return PLATFORM_ERR_OK;

        case APP_CONTROL_RESPONSE_ALREADY_RUNNING:
            *data = g_alreadyRunningResponse;
            *dataLength = sizeof(g_alreadyRunningResponse) - 1U;
            return PLATFORM_ERR_OK;

        case APP_CONTROL_RESPONSE_ALREADY_STOPPED:
            *data = g_alreadyStoppedResponse;
            *dataLength = sizeof(g_alreadyStoppedResponse) - 1U;
            return PLATFORM_ERR_OK;

        case APP_CONTROL_RESPONSE_BUSY:
            *data = g_busyResponse;
            *dataLength = sizeof(g_busyResponse) - 1U;
            return PLATFORM_ERR_OK;

        case APP_CONTROL_RESPONSE_ACQUISITION_FAILED:
            *data = g_acquisitionFailedResponse;
            *dataLength = sizeof(g_acquisitionFailedResponse) - 1U;
            return PLATFORM_ERR_OK;

        case APP_CONTROL_RESPONSE_STATUS_RUNNING:
            *data = g_statusRunningResponse;
            *dataLength = sizeof(g_statusRunningResponse) - 1U;
            return PLATFORM_ERR_OK;

        case APP_CONTROL_RESPONSE_STATUS_STOPPED:
            *data = g_statusStoppedResponse;
            *dataLength = sizeof(g_statusStoppedResponse) - 1U;
            return PLATFORM_ERR_OK;

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

static platform_error_t app_communication_send_control_response(
    app_communication_t *communication,
    app_control_response_t response)
{
    const uint8_t *data = NULL;
    platform_size_t dataLength = 0U;
    platform_error_t result = app_communication_get_control_response(
        response, &data, &dataLength);

    if (result == PLATFORM_ERR_OK) {
        result = service_uart_write(communication->config.service,
                                    data,
                                    dataLength,
                                    PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS);
    }
    if (result == PLATFORM_ERR_OK) {
        communication->statistics.controlResponseCount++;
    } else {
        communication->statistics.controlResponseFailureCount++;
    }
    return result;
}

static platform_error_t app_communication_format_report(
    const app_acquisition_data_t *data,
    uint8_t *environmentBuffer,
    platform_size_t environmentBufferSize,
    platform_size_t *environmentLength,
    uint8_t *motionBuffer,
    platform_size_t motionBufferSize,
    platform_size_t *motionLength)
{
    int writtenLength = snprintf(
        (char *)environmentBuffer,
        environmentBufferSize,
        "ENV,T=%.2f,H=%.2f\r\n",
        (double)data->environment.temperatureC,
        (double)data->environment.humidityPercent);

    if ((writtenLength < 0) || ((platform_size_t)writtenLength >= environmentBufferSize)) {
        return PLATFORM_ERR_OVERFLOW;
    }
    *environmentLength = (platform_size_t)writtenLength;

    writtenLength = snprintf(
        (char *)motionBuffer,
        motionBufferSize,
        "IMU,AX=%.3f,AY=%.3f,AZ=%.3f,GX=%.2f,GY=%.2f,GZ=%.2f\r\n",
        (double)data->motion.accelXG,
        (double)data->motion.accelYG,
        (double)data->motion.accelZG,
        (double)data->motion.gyroXDps,
        (double)data->motion.gyroYDps,
        (double)data->motion.gyroZDps);
    if ((writtenLength < 0) || ((platform_size_t)writtenLength >= motionBufferSize)) {
        return PLATFORM_ERR_OVERFLOW;
    }
    *motionLength = (platform_size_t)writtenLength;

    return PLATFORM_ERR_OK;
}

static platform_error_t app_communication_send_report(
    app_communication_t *communication,
    const app_acquisition_data_t *data)
{
    uint8_t environmentBuffer[APP_COMM_ENV_REPORT_BUFFER_SIZE] = {0};
    uint8_t motionBuffer[APP_COMM_IMU_REPORT_BUFFER_SIZE] = {0};
    platform_size_t environmentLength = 0U;
    platform_size_t motionLength = 0U;
    platform_error_t result = app_communication_format_report(
        data,
        environmentBuffer,
        sizeof(environmentBuffer),
        &environmentLength,
        motionBuffer,
        sizeof(motionBuffer),
        &motionLength);

    if (result == PLATFORM_ERR_OK) {
        result = service_uart_write(communication->config.service,
                                    environmentBuffer,
                                    environmentLength,
                                    PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS);
    }
    if (result == PLATFORM_ERR_OK) {
        result = service_uart_write(communication->config.service,
                                    motionBuffer,
                                    motionLength,
                                    PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS);
    }
    if (result == PLATFORM_ERR_OK) {
        communication->statistics.reportCount++;
    } else {
        communication->statistics.reportFailureCount++;
    }
    return result;
}

static void app_communication_submit_once_result(
    app_communication_t *communication,
    platform_error_t txResult)
{
    app_control_message_t message = {
        .type = APP_CONTROL_MESSAGE_ONCE_TX_RESULT,
        .payload.result = txResult
    };

    if (platform_queue_send(
            communication->config.controlQueue,
            &message,
            PLATFORM_OS_NO_WAIT) != PLATFORM_ERR_OK) {
        communication->statistics.onceCompletionSubmitFailureCount++;
    }
}

static platform_error_t app_communication_handle_outbound(
    app_communication_t *communication,
    const app_communication_outbound_message_t *message)
{
    platform_error_t result;

    switch (message->type) {
        case APP_COMM_OUTBOUND_CONTROL_RESPONSE:
            (void)app_communication_send_control_response(
                communication, message->payload.controlResponse);
            return PLATFORM_ERR_OK;

        case APP_COMM_OUTBOUND_PERIODIC_REPORT:
            (void)app_communication_send_report(
                communication, &message->payload.acquisition);
            return PLATFORM_ERR_OK;

        case APP_COMM_OUTBOUND_ONCE_REPORT:
            result = app_communication_send_report(
                communication, &message->payload.acquisition);
            app_communication_submit_once_result(communication, result);
            return PLATFORM_ERR_OK;

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

static void app_communication_process_complete_line(app_communication_t *communication)
{
    app_communication_command_t command = APP_COMMUNICATION_COMMAND_INVALID;

    if (communication->context.commandLength == 0U) {
        return;
    }

    command = app_communication_parse_command(communication->context.commandLine,
                                              communication->context.commandLength);
    if (command == APP_COMMUNICATION_COMMAND_INVALID) {
        communication->statistics.commandInvalidCount++;
        app_communication_send_local_response(communication,
                                              g_unknownCommandResponse,
                                              sizeof(g_unknownCommandResponse) - 1U);
        return;
    }

    communication->statistics.commandReceivedCount++;
    switch (command) {
        case APP_COMMUNICATION_COMMAND_START:
            app_communication_submit_control_event(communication, APP_CTRL_START);
            break;

        case APP_COMMUNICATION_COMMAND_STOP:
            app_communication_submit_control_event(communication, APP_CTRL_STOP);
            break;

        case APP_COMMUNICATION_COMMAND_ONCE:
            app_communication_submit_control_event(communication, APP_CTRL_SAMPLE_ONCE);
            break;

        case APP_COMMUNICATION_COMMAND_STATUS:
            app_communication_submit_control_event(communication, APP_CTRL_GET_STATUS);
            break;

        case APP_COMMUNICATION_COMMAND_HELP:
            app_communication_send_local_response(communication,
                                                  g_helpResponse,
                                                  sizeof(g_helpResponse) - 1U);
            break;

        default:
            break;
    }
}

static void app_communication_discard_current_line(app_communication_t *communication,
                                                    platform_bool_t overflowed)
{
    communication->context.commandLength = 0U;
    communication->context.pendingCr = PLATFORM_FALSE;
    communication->context.discardLine = PLATFORM_TRUE;
    communication->statistics.commandInvalidCount++;
    if (overflowed == PLATFORM_TRUE) {
        communication->statistics.commandOverflowCount++;
        app_communication_send_local_response(communication,
                                              g_commandTooLongResponse,
                                              sizeof(g_commandTooLongResponse) - 1U);
    }
}

static void app_communication_feed_rx_byte(app_communication_t *communication, uint8_t data)
{
    if (communication->context.discardLine == PLATFORM_TRUE) {
        if (communication->context.pendingCr == PLATFORM_TRUE) {
            if (data == '\n') {
                communication->context.pendingCr = PLATFORM_FALSE;
                communication->context.discardLine = PLATFORM_FALSE;
            } else {
                communication->context.pendingCr = (data == '\r') ? PLATFORM_TRUE : PLATFORM_FALSE;
            }
        } else if (data == '\r') {
            communication->context.pendingCr = PLATFORM_TRUE;
        }

        return;
    }

    if (communication->context.pendingCr == PLATFORM_TRUE) {
        if (data == '\n') {
            communication->context.pendingCr = PLATFORM_FALSE;
            communication->context.commandLine[communication->context.commandLength] = '\0';
            app_communication_process_complete_line(communication);
            communication->context.commandLength = 0U;
        } else {
            app_communication_discard_current_line(communication, PLATFORM_FALSE);
            if (data == '\r') {
                communication->context.pendingCr = PLATFORM_TRUE;
            }
        }

        return;
    }

    if (data == '\r') {
        communication->context.pendingCr = PLATFORM_TRUE;
        return;
    }

    if (data == '\n') {
        app_communication_discard_current_line(communication, PLATFORM_FALSE);
        return;
    }

    if (communication->context.commandLength >=
        (PROJECT_COMM_COMMAND_LINE_BUFFER_SIZE - 1U)) {
        app_communication_discard_current_line(communication, PLATFORM_TRUE);
        return;
    }

    communication->context.commandLine[communication->context.commandLength] = data;
    communication->context.commandLength++;
}

static platform_error_t app_communication_drain_rx(app_communication_t *communication)
{
    uint8_t buffer[PROJECT_COMM_READ_BUFFER_SIZE] = {0};
    platform_error_t result = PLATFORM_ERR_OK;
    platform_size_t readLength = 0U;
    platform_size_t index = 0U;

    for (;;) {
        result = service_uart_read(communication->config.service,
                                   buffer,
                                   sizeof(buffer),
                                   &readLength);
        if (result == PLATFORM_ERR_EMPTY) {
            return PLATFORM_ERR_OK;
        }

        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        communication->statistics.processedChunkCount++;
        communication->statistics.processedByteCount += readLength;
        for (index = 0U; index < readLength; index++) {
            app_communication_feed_rx_byte(communication, buffer[index]);
        }
    }
}

platform_error_t app_communication_init(
    app_communication_t *communication,
    const app_communication_config_t *config)
{
    /* 依赖对象必须在复制前有效，避免后续启动阶段发生空指针解引用。 */
    if ((communication == NULL) || (config == NULL) || (config->uart == NULL) ||
        (config->service == NULL)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (communication->context.state != APP_COMMUNICATION_STATE_UNINITIALIZED) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    communication->config = *config;
    communication->context.state = APP_COMMUNICATION_STATE_INITIALIZED;
    communication->context.lastError = PLATFORM_ERR_OK;
    communication->statistics = (app_communication_statistics_t){0};

    return PLATFORM_ERR_OK;
}

platform_error_t app_communication_get_status(
    const app_communication_t *communication,
    app_communication_status_t *status)
{
    /* 未初始化对象不向调用者暴露零初始化存储的伪状态。 */
    if ((communication == NULL) || (status == NULL)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (communication->context.state == APP_COMMUNICATION_STATE_UNINITIALIZED) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    status->state = communication->context.state;
    status->lastError = communication->context.lastError;

    return PLATFORM_ERR_OK;
}

platform_error_t app_communication_get_statistics(
    const app_communication_t *communication,
    app_communication_statistics_t *statistics)
{
    /* 统计仅通过快照返回，调用者不能修改 APP 内部累计值。 */
    if ((communication == NULL) || (statistics == NULL)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (communication->context.state == APP_COMMUNICATION_STATE_UNINITIALIZED) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    *statistics = communication->statistics;

    return PLATFORM_ERR_OK;
}

platform_error_t app_communication_start(app_communication_t *communication)
{
    platform_error_t result = PLATFORM_ERR_OK;

    if (communication == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (communication->context.state == APP_COMMUNICATION_STATE_UNINITIALIZED) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    if (communication->context.state != APP_COMMUNICATION_STATE_INITIALIZED) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((communication->config.uart == NULL) || (communication->config.service == NULL) ||
        (communication->config.uart->device.lifecycle == NULL) ||
        (communication->config.uart->device.lifecycle->init == NULL) ||
        (communication->config.uart->device.lifecycle->start == NULL) ||
        (communication->config.uart->device.lifecycle->stop == NULL)) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = communication->config.uart->device.lifecycle->init(communication->config.uart);
    if (result != PLATFORM_ERR_OK) {
        communication->context.state = APP_COMMUNICATION_STATE_ERROR;
        communication->context.lastError = result;
        communication->statistics.fatalErrorCount++;
        return result;
    }

    result = communication->config.uart->device.lifecycle->start(communication->config.uart);
    if (result != PLATFORM_ERR_OK) {
        communication->context.state = APP_COMMUNICATION_STATE_ERROR;
        communication->context.lastError = result;
        communication->statistics.fatalErrorCount++;
        return result;
    }

    result = service_uart_start(communication->config.service);
    if (result != PLATFORM_ERR_OK) {
        (void)communication->config.uart->device.lifecycle->stop(communication->config.uart);
        communication->context.state = APP_COMMUNICATION_STATE_ERROR;
        communication->context.lastError = result;
        communication->statistics.fatalErrorCount++;
        return result;
    }

    communication->context.state = APP_COMMUNICATION_STATE_RUNNING;
    communication->context.lastError = PLATFORM_ERR_OK;
    SERVICE_LOG_I("communication runtime started");

    return PLATFORM_ERR_OK;
}

platform_error_t app_communication_process(app_communication_t *communication, uint32_t timeoutMs)
{
    platform_error_t result = PLATFORM_ERR_OK;
    uint32_t events = 0U;
    service_uart_status_t serviceStatus = {
        .state = SERVICE_UART_STATE_UNINITIALIZED,
        .lastError = PLATFORM_ERR_OK,
        .dataLossOccurred = PLATFORM_FALSE
    };

    if (communication == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (communication->context.state == APP_COMMUNICATION_STATE_UNINITIALIZED) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    if (communication->context.state != APP_COMMUNICATION_STATE_RUNNING) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = service_uart_wait_event(communication->config.service, timeoutMs, &events);
    if (result == PLATFORM_ERR_TIMEOUT) {
        return PLATFORM_ERR_OK;
    }

    if (result != PLATFORM_ERR_OK) {
        return app_communication_set_error(communication, result);
    }

    if ((events & SERVICE_UART_EVENT_RX_AVAILABLE) != 0U) {
        result = app_communication_drain_rx(communication);
        if (result != PLATFORM_ERR_OK) {
            return app_communication_set_error(communication, result);
        }
    }

    if ((events & SERVICE_UART_EVENT_ERROR) != 0U) {
        result = service_uart_get_status(communication->config.service, &serviceStatus);
        if (result != PLATFORM_ERR_OK) {
            return app_communication_set_error(communication, result);
        }

        result = service_uart_start(communication->config.service);
        if (result != PLATFORM_ERR_OK) {
            return app_communication_set_error(communication, result);
        }

        communication->statistics.uartErrorRecoveryCount++;
        return PLATFORM_ERR_OK;
    }

    if ((events & SERVICE_UART_EVENT_DATA_LOSS) != 0U) {
        result = service_uart_stop(communication->config.service);
        if (result != PLATFORM_ERR_OK) {
            result = service_uart_get_status(communication->config.service, &serviceStatus);
            if ((result != PLATFORM_ERR_OK) ||
                (serviceStatus.state != SERVICE_UART_STATE_STOPPED)) {
                return app_communication_set_error(communication, result);
            }
        }

        result = service_uart_start(communication->config.service);
        if (result != PLATFORM_ERR_OK) {
            return app_communication_set_error(communication, result);
        }

        communication->statistics.dataLossRecoveryCount++;
        return PLATFORM_ERR_OK;
    }

    if ((events & SERVICE_UART_EVENT_STOPPED) != 0U) {
        return app_communication_set_error(communication, PLATFORM_ERR_CANCELED);
    }

    return PLATFORM_ERR_OK;
}

platform_error_t app_communication_drain_outbound(app_communication_t *communication)
{
    app_communication_outbound_message_t message = {0};
    platform_error_t result;

    if (communication == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }
    if (communication->context.state != APP_COMMUNICATION_STATE_RUNNING) {
        return PLATFORM_ERR_INVALID_STATE;
    }
    if ((communication->config.outboundQueue == NULL) ||
        (communication->config.controlQueue == NULL)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    for (;;) {
        result = platform_queue_receive(
            communication->config.outboundQueue,
            &message,
            PLATFORM_OS_NO_WAIT);
        if ((result == PLATFORM_ERR_EMPTY) || (result == PLATFORM_ERR_TIMEOUT)) {
            return PLATFORM_ERR_OK;
        }
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        communication->statistics.outboundMessageCount++;
        result = app_communication_handle_outbound(communication, &message);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }
    }
}

void app_communication_task_entry(void *argument)
{
    app_communication_t *communication = (app_communication_t *)argument;
    platform_error_t result = PLATFORM_ERR_OK;

    if (communication == NULL) {
        for (;;) {
            (void)platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS);
        }
    }

    result = app_communication_start(communication);
    while (result == PLATFORM_ERR_OK) {
        result = app_communication_drain_outbound(communication);
        if (result == PLATFORM_ERR_OK) {
            result = app_communication_process(
                communication, PROJECT_COMM_WAIT_TIMEOUT_MS);
        }
        if (result == PLATFORM_ERR_OK) {
            result = app_communication_drain_outbound(communication);
        }
    }

    SERVICE_LOG_E("communication fatal error: %d", (int)communication->context.lastError);
    for (;;) {
        (void)platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS);
    }
}
//******************************** Functions *********************************//
