/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file project_config.h
 * @brief 定义产品级静态配置。
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

//******************************** Includes *********************************//
#include "platform_gpio_types.h"
#include "platform_thread.h"
#include "platform_uart_types.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define PROJECT_COMM_UART_BAUD_RATE                 (115200U)
#define PROJECT_COMM_UART_DATA_BITS                 PLATFORM_UART_DATA_BITS_8
#define PROJECT_COMM_UART_STOP_BITS                 PLATFORM_UART_STOP_BITS_1
#define PROJECT_COMM_UART_PARITY                    PLATFORM_UART_PARITY_NONE
#define PROJECT_COMM_UART_FLOW_CONTROL              PLATFORM_UART_FLOW_CONTROL_NONE
#define PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS        (1000U)

#define PROJECT_COMM_DMA_RX_BUFFER_SIZE             (128U)
#define PROJECT_COMM_RING_BUFFER_STORAGE_SIZE       (512U)
#define PROJECT_COMM_READ_BUFFER_SIZE                (128U)
#define PROJECT_COMM_COMMAND_LINE_BUFFER_SIZE        (32U)

#define PROJECT_COMM_TASK_STACK_SIZE_BYTES          (1024U)
#define PROJECT_COMM_TASK_PRIORITY                   PLATFORM_THREAD_PRIORITY_NORMAL

#define PROJECT_COMM_WAIT_TIMEOUT_MS                 (1000U)
#define PROJECT_COMM_ERROR_IDLE_DELAY_MS             (1000U)

#define PROJECT_SOFT_I2C_HALF_PERIOD_US              (5U)
#define PROJECT_SOFT_I2C_SCL_TIMEOUT_US              (100U)

#define PROJECT_USER_KEY_ACTIVE_LEVEL                PLATFORM_GPIO_LEVEL_LOW
#define PROJECT_USER_KEY_PULL                        PLATFORM_GPIO_PULL_UP
#define PROJECT_BUTTON_SAMPLE_PERIOD_MS              (10U)
#define PROJECT_BUTTON_DEBOUNCE_MS                   (30U)
#define PROJECT_BUTTON_DOUBLE_CLICK_MS               (300U)
#define PROJECT_BUTTON_LONG_PRESS_MS                 (3000U)

#define PROJECT_ACQUISITION_PERIOD_MS                (2000U)

#define PROJECT_STATUS_LED_ACTIVE_LEVEL               PLATFORM_GPIO_LEVEL_LOW
#define PROJECT_INDICATOR_BLINK_COUNT                 (3U)
#define PROJECT_INDICATOR_BLINK_ON_MS                 (100U)
#define PROJECT_INDICATOR_BLINK_OFF_MS                (100U)
//******************************** Defines *********************************//

#endif
