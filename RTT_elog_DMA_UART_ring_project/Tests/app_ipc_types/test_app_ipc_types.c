/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_app_ipc_types.c
 * @brief 验证 Phase 9 APP IPC 值拷贝合同。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#include "app_ipc_types.h"

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            return __LINE__; \
        } \
    } while (0)

_Static_assert(APP_CTRL_SOURCE_BUTTON != APP_CTRL_SOURCE_UART,
               "control sources must be distinct");
_Static_assert(APP_CONTROL_MESSAGE_CONTROL_REQUEST != APP_CONTROL_MESSAGE_ONCE_TX_RESULT,
               "control message kinds must be distinct");
_Static_assert(APP_ACQUISITION_COMMAND_START_PERIODIC != APP_ACQUISITION_COMMAND_SAMPLE_ONCE,
               "acquisition commands must be distinct");
_Static_assert(APP_COMM_OUTBOUND_PERIODIC_REPORT != APP_COMM_OUTBOUND_ONCE_REPORT,
               "outbound report kinds must be distinct");
_Static_assert(APP_INDICATOR_STOPPED != APP_INDICATOR_ONCE_SUCCESS,
               "indicator commands must be distinct");

int main(void)
{
    app_control_message_t controlMessage = {0};
    app_communication_outbound_message_t outboundMessage = {0};

    controlMessage.type = APP_CONTROL_MESSAGE_CONTROL_REQUEST;
    controlMessage.payload.request.event = APP_CTRL_START;
    controlMessage.payload.request.source = APP_CTRL_SOURCE_UART;
    TEST_ASSERT(controlMessage.payload.request.event == APP_CTRL_START);
    TEST_ASSERT(controlMessage.payload.request.source == APP_CTRL_SOURCE_UART);

    outboundMessage.type = APP_COMM_OUTBOUND_ONCE_REPORT;
    outboundMessage.payload.acquisition.environment.temperatureC = 25.0F;
    outboundMessage.payload.acquisition.motion.accelZG = 1.0F;
    TEST_ASSERT(outboundMessage.payload.acquisition.environment.temperatureC == 25.0F);
    TEST_ASSERT(outboundMessage.payload.acquisition.motion.accelZG == 1.0F);

    return 0;
}
