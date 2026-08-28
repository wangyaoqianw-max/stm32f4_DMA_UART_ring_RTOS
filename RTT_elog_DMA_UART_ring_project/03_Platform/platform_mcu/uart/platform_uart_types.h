/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_uart_types.h
 * @brief Platform UART public data types
 * @author YaoQian Wang
 * @date 2026-08-28
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PLATFORM_UART_TYPES_H
#define PLATFORM_UART_TYPES_H

//******************************** Includes *********************************//
#include "platform_types.h"
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define PLATFORM_UART_TIMEOUT_USE_DEFAULT (0xFFFFFFFEU)
#define PLATFORM_UART_WAIT_FOREVER        (0xFFFFFFFFU)
//******************************** Defines *********************************//

//******************************** Declaring *********************************//
typedef struct platform_uart platform_uart_t;

/*UART data bits*/
typedef enum
{
    PLATFORM_UART_DATA_BITS_7 = 7,
    PLATFORM_UART_DATA_BITS_8 = 8,
    PLATFORM_UART_DATA_BITS_9 = 9
} platform_uart_data_bits_t;

/*UART stop bits*/
typedef enum
{
    PLATFORM_UART_STOP_BITS_1 = 0,
    PLATFORM_UART_STOP_BITS_2,
    PLATFORM_UART_STOP_BITS_MAX
} platform_uart_stop_bits_t;

/*UART parity modes*/
typedef enum
{
    PLATFORM_UART_PARITY_NONE = 0,
    PLATFORM_UART_PARITY_EVEN,
    PLATFORM_UART_PARITY_ODD,
    PLATFORM_UART_PARITY_MAX
} platform_uart_parity_t;

/*UART hardware flow-control modes*/
typedef enum
{
    PLATFORM_UART_FLOW_CONTROL_NONE = 0,
    PLATFORM_UART_FLOW_CONTROL_RTS,
    PLATFORM_UART_FLOW_CONTROL_CTS,
    PLATFORM_UART_FLOW_CONTROL_RTS_CTS,
    PLATFORM_UART_FLOW_CONTROL_MAX
} platform_uart_flow_control_t;

/*UART transfer directions*/
typedef enum
{
    PLATFORM_UART_DIRECTION_TX = 0,
    PLATFORM_UART_DIRECTION_RX,
    PLATFORM_UART_DIRECTION_BOTH,
    PLATFORM_UART_DIRECTION_MAX
} platform_uart_direction_t;

/*UART asynchronous event types*/
typedef enum
{
    PLATFORM_UART_EVENT_TX_COMPLETE = 0,
    PLATFORM_UART_EVENT_RX_DATA,
    PLATFORM_UART_EVENT_ERROR,
    PLATFORM_UART_EVENT_CANCELED,
    PLATFORM_UART_EVENT_MAX
} platform_uart_event_type_t;

/*UART static configuration*/
typedef struct
{
    uint32_t baudRate;
    platform_uart_data_bits_t dataBits;
    platform_uart_stop_bits_t stopBits;
    platform_uart_parity_t parity;
    platform_uart_flow_control_t flowControl;
    uint32_t defaultTimeoutMs;
} platform_uart_config_t;

/*UART asynchronous event payload*/
typedef struct
{
    platform_uart_event_type_t type;
    platform_uart_direction_t direction;
    const uint8_t *data;
    platform_size_t dataLength;
    platform_error_t error;
} platform_uart_event_t;

/**
 * @brief Handle an asynchronous UART event
 * @param[in] uart            : UART object that emitted the event
 * @param[in] event           : Event payload, valid only during the callback
 * @param[in] callbackContext : User context registered during initialization
 * @return None
 * @warning The callback may run in interrupt context and must not block
 */
typedef void (*platform_uart_callback_t)(platform_uart_t *uart,
                                         const platform_uart_event_t *event,
                                         void *callbackContext);
//******************************** Declaring *********************************//

#endif
