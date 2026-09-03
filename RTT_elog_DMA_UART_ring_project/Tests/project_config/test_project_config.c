/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_project_config.c
 * @brief 验证产品级静态配置
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#include "project_config.h"

_Static_assert(PROJECT_COMM_UART_BAUD_RATE == 115200U,
               "unexpected communication baud rate");
_Static_assert(PROJECT_COMM_UART_DATA_BITS == PLATFORM_UART_DATA_BITS_8,
               "unexpected communication data bits");
_Static_assert(PROJECT_COMM_UART_STOP_BITS == PLATFORM_UART_STOP_BITS_1,
               "unexpected communication stop bits");
_Static_assert(PROJECT_COMM_UART_PARITY == PLATFORM_UART_PARITY_NONE,
               "unexpected communication parity");
_Static_assert(PROJECT_COMM_UART_FLOW_CONTROL == PLATFORM_UART_FLOW_CONTROL_NONE,
               "unexpected communication flow control");
_Static_assert(PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS == 1000U,
               "unexpected communication default timeout");
_Static_assert(PROJECT_COMM_DMA_RX_BUFFER_SIZE == 128U,
               "unexpected dma rx buffer size");
_Static_assert(PROJECT_COMM_RING_BUFFER_STORAGE_SIZE == 512U,
               "unexpected ring storage size");
_Static_assert(PROJECT_COMM_READ_BUFFER_SIZE == 128U,
               "unexpected app read buffer size");
_Static_assert(PROJECT_COMM_TASK_STACK_SIZE_BYTES == 1024U,
               "unexpected communication task stack size");
_Static_assert(PROJECT_COMM_TASK_PRIORITY == PLATFORM_THREAD_PRIORITY_NORMAL,
               "unexpected communication task priority");
_Static_assert(PROJECT_COMM_WAIT_TIMEOUT_MS == 1000U,
               "unexpected communication wait timeout");
_Static_assert(PROJECT_COMM_ERROR_IDLE_DELAY_MS == 1000U,
               "unexpected communication error idle delay");
_Static_assert(PROJECT_STATUS_LED_ACTIVE_LEVEL == PLATFORM_GPIO_LEVEL_LOW,
               "unexpected status led active level");
_Static_assert(PROJECT_INDICATOR_BLINK_COUNT == 3U,
               "unexpected indicator blink count");
_Static_assert(PROJECT_INDICATOR_BLINK_ON_MS == 100U,
               "unexpected indicator blink on duration");
_Static_assert(PROJECT_INDICATOR_BLINK_OFF_MS == 100U,
               "unexpected indicator blink off duration");

int main(void)
{
    return 0;
}
