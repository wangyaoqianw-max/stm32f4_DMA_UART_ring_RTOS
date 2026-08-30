/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_uart.h
 * @brief UART 接收 Service 公共接口
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#ifndef SERVICE_UART_H
#define SERVICE_UART_H

//******************************** Includes *********************************//
#include "ring_buffer.h"
#include "platform_os.h"
#include "platform_uart.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define SERVICE_UART_INITIALIZER             {0}
#define SERVICE_UART_EVENT_RX_AVAILABLE      (1U << 0)
#define SERVICE_UART_EVENT_DATA_LOSS         (1U << 1)
#define SERVICE_UART_EVENT_ERROR             (1U << 2)
#define SERVICE_UART_EVENT_STOPPED           (1U << 3)
//******************************** Defines *********************************//

//******************************** Types ***********************************//
typedef enum
{
    SERVICE_UART_STATE_UNINITIALIZED = 0,
    SERVICE_UART_STATE_INITIALIZED,
    SERVICE_UART_STATE_RUNNING,
    SERVICE_UART_STATE_STOPPING,
    SERVICE_UART_STATE_STOPPED,
    SERVICE_UART_STATE_ERROR,
    SERVICE_UART_STATE_MAX
} service_uart_state_t;

typedef struct
{
    platform_uart_t *uart;
    uint8_t *dmaRxBuffer;
    platform_size_t dmaRxBufferSize;
    uint8_t *ringBufferStorage;
    platform_size_t ringBufferStorageSize;
    platform_thread_t *consumerThread;
} service_uart_config_t;

typedef struct
{
    volatile service_uart_state_t state;
    ring_buffer_t rxRingBuffer;
    volatile platform_error_t lastError;
    volatile platform_bool_t dataLossOccurred;
} service_uart_context_t;

typedef struct
{
    volatile uint32_t rxEventCount;
    volatile uint32_t rxBytesReceived;
    volatile uint32_t rxBytesBuffered;
    volatile uint32_t rxBytesRead;
    volatile uint32_t rxBytesDropped;
    volatile uint32_t ringBufferOverflowCount;
    volatile platform_size_t ringBufferHighWaterMark;
    volatile uint32_t uartErrorCount;
    volatile uint32_t cancelCount;
} service_uart_statistics_t;

typedef struct
{
    service_uart_state_t state;
    platform_error_t lastError;
    platform_bool_t dataLossOccurred;
} service_uart_status_t;

typedef struct
{
    service_uart_config_t config;
    service_uart_context_t context;
    service_uart_statistics_t statistics;
} service_uart_t;
//******************************** Types ***********************************//

//******************************** Declaring *******************************//
platform_error_t service_uart_init(service_uart_t *service,
                                   const service_uart_config_t *config);
platform_error_t service_uart_start(service_uart_t *service);
platform_error_t service_uart_stop(service_uart_t *service);
platform_error_t service_uart_deinit(service_uart_t *service);
platform_error_t service_uart_read(service_uart_t *service,
                                   uint8_t *buffer,
                                   platform_size_t bufferSize,
                                   platform_size_t *readLength);
platform_error_t service_uart_wait_event(service_uart_t *service,
                                         uint32_t timeoutMs,
                                         uint32_t *events);
platform_error_t service_uart_get_readable_size(const service_uart_t *service,
                                                platform_size_t *readableSize);
platform_error_t service_uart_get_status(const service_uart_t *service,
                                         service_uart_status_t *status);
platform_error_t service_uart_get_statistics(
    const service_uart_t *service,
    service_uart_statistics_t *statistics);
platform_error_t service_uart_clear_statistics(service_uart_t *service);
//******************************** Declaring *******************************//

#endif
