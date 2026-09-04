/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_system.c
 * @brief APP 系统 Composition Root 实现
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_system.h"

#include "app_communication.h"
#include "project_config.h"
#include "platform_bsp_uart.h"
#include "platform_thread.h"
#include "service_log.h"
#include "service_uart.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define LOG_TAG                                "app_system"
//******************************** Defines **********************************//

//******************************** Variables *********************************//
static platform_uart_t g_communicationUart = PLATFORM_UART_INITIALIZER;
static service_uart_t g_uartService = SERVICE_UART_INITIALIZER;
static platform_thread_t g_communicationThread = PLATFORM_OS_OBJECT_INITIALIZER;
static app_communication_t g_appCommunication = APP_COMMUNICATION_INITIALIZER;
static uint8_t g_dmaRxStorage[PROJECT_COMM_DMA_RX_BUFFER_SIZE] = {0};
static uint8_t g_ringStorage[PROJECT_COMM_RING_BUFFER_STORAGE_SIZE] = {0};
static platform_bool_t g_isInitialized = PLATFORM_FALSE;

static const platform_uart_config_t g_communicationUartConfig = {
    .baudRate = PROJECT_COMM_UART_BAUD_RATE,
    .dataBits = PROJECT_COMM_UART_DATA_BITS,
    .stopBits = PROJECT_COMM_UART_STOP_BITS,
    .parity = PROJECT_COMM_UART_PARITY,
    .flowControl = PROJECT_COMM_UART_FLOW_CONTROL,
    .defaultTimeoutMs = PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS
};
//******************************** Variables *********************************//

//******************************** Functions *********************************//
platform_error_t app_system_init(void)
{
    app_communication_config_t communicationConfig = {
        .uart = &g_communicationUart,
        .service = &g_uartService
    };
    platform_thread_config_t threadConfig = {
        .name = "communication",
        .entry = app_communication_task_entry,
        .argument = &g_appCommunication,
        .stackSizeBytes = PROJECT_COMM_TASK_STACK_SIZE_BYTES,
        .priority = PROJECT_COMM_TASK_PRIORITY
    };
    service_uart_config_t serviceConfig = {
        .uart = &g_communicationUart,
        .dmaRxBuffer = g_dmaRxStorage,
        .dmaRxBufferSize = PROJECT_COMM_DMA_RX_BUFFER_SIZE,
        .ringBufferStorage = g_ringStorage,
        .ringBufferStorageSize = PROJECT_COMM_RING_BUFFER_STORAGE_SIZE,
        .ownerThread = &g_communicationThread
    };
    platform_error_t result = PLATFORM_ERR_OK;

    if (g_isInitialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    result = platform_bsp_uart_construct_communication(&g_communicationUart,
                                                        &g_communicationUartConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = app_communication_init(&g_appCommunication, &communicationConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_thread_create(&g_communicationThread, &threadConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = service_uart_init(&g_uartService, &serviceConfig);
    if (result != PLATFORM_ERR_OK) {
        (void)platform_thread_terminate(&g_communicationThread);
        return result;
    }

    g_isInitialized = PLATFORM_TRUE;
    SERVICE_LOG_I("system composition initialized");

    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
