/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_communication.h
 * @brief 定义通信 APP 的运行状态和任务接口。
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_COMMUNICATION_H
#define APP_COMMUNICATION_H

#include "service_uart.h"

#define APP_COMMUNICATION_INITIALIZER {0}

typedef enum
{
    APP_COMMUNICATION_STATE_UNINITIALIZED = 0,
    APP_COMMUNICATION_STATE_INITIALIZED,
    APP_COMMUNICATION_STATE_RUNNING,
    APP_COMMUNICATION_STATE_ERROR,
    APP_COMMUNICATION_STATE_MAX
} app_communication_state_t;

typedef struct
{
    platform_uart_t *uart;
    service_uart_t *service;
} app_communication_config_t;

typedef struct
{
    app_communication_state_t state;
    platform_error_t lastError;
} app_communication_context_t;

typedef struct
{
    uint32_t processedChunkCount;
    uint32_t processedByteCount;
    uint32_t dataLossRecoveryCount;
    uint32_t uartErrorRecoveryCount;
    uint32_t fatalErrorCount;
} app_communication_statistics_t;

typedef struct
{
    app_communication_state_t state;
    platform_error_t lastError;
} app_communication_status_t;

typedef struct
{
    app_communication_config_t config;
    app_communication_context_t context;
    app_communication_statistics_t statistics;
} app_communication_t;

/** @brief 绑定通信 APP 的 UART 与 UART Service 依赖；本函数不启动硬件。 */
platform_error_t app_communication_init(
    app_communication_t *communication,
    const app_communication_config_t *config);
/** @brief 在任务上下文启动 UART lifecycle 与 UART Service RX Session。 */
platform_error_t app_communication_start(app_communication_t *communication);
/** @brief 执行一次等待、读取和恢复周期；timeoutMs 单位为毫秒。 */
platform_error_t app_communication_process(app_communication_t *communication, uint32_t timeoutMs);
/** @brief 获取通信 APP 运行状态快照。 */
platform_error_t app_communication_get_status(
    const app_communication_t *communication,
    app_communication_status_t *status);
/** @brief 获取通信 APP 自有统计快照。 */
platform_error_t app_communication_get_statistics(
    const app_communication_t *communication,
    app_communication_statistics_t *statistics);
/** @brief 正式 Communication Task 入口。 */
void app_communication_task_entry(void *argument);

#endif
