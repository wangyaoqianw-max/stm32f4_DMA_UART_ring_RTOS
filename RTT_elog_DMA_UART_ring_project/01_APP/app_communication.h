/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_communication.h
 * @brief 定义通信 APP 的运行状态和任务接口。
 * @author YaoQian Wang
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_COMMUNICATION_H
#define APP_COMMUNICATION_H

//******************************** Includes *********************************//
#include "app_control_types.h"
#include "project_config.h"
#include "service_uart.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define APP_COMMUNICATION_INITIALIZER    {0}
//******************************** Defines *********************************//

//******************************** Types ***********************************//
/**
 * @brief 通信 APP 生命周期状态
 * @note ERROR 状态下不再自动恢复，正式任务仅进入低频 idle。
 */
typedef enum
{
    APP_COMMUNICATION_STATE_UNINITIALIZED = 0,
    APP_COMMUNICATION_STATE_INITIALIZED,
    APP_COMMUNICATION_STATE_RUNNING,
    APP_COMMUNICATION_STATE_ERROR,
    APP_COMMUNICATION_STATE_MAX
} app_communication_state_t;

/**
 * @brief 通信 APP 初始化配置
 * @note uart 和 service 的对象存储由 APP Composition Root 持有。
 */
typedef struct
{
    /** APP 控制生命周期的 Platform UART。 */
    platform_uart_t *uart;
    /** APP 等待、读取和恢复的 UART Service。 */
    service_uart_t *service;
    /** 向 APP Control 提交统一控制事件的可选出口。 */
    app_control_event_handler_t controlHandler;
    /** controlHandler 的调用者持有上下文。 */
    void *controlContext;
} app_communication_config_t;

/**
 * @brief 通信 APP 当前运行上下文
 */
typedef struct
{
    /** 当前通信 APP 状态。 */
    app_communication_state_t state;
    /** 最近一次导致 APP 错误状态的错误。 */
    platform_error_t lastError;
    /** 未包含 CRLF 的当前命令行数据。 */
    uint8_t commandLine[PROJECT_COMM_COMMAND_LINE_BUFFER_SIZE];
    /** commandLine 中当前有效字节数。 */
    platform_size_t commandLength;
    /** 已收到 CR，等待 LF 确认严格行结束。 */
    platform_bool_t pendingCr;
    /** 当前行已损坏或过长，等待完整 CRLF 后恢复。 */
    platform_bool_t discardLine;
} app_communication_context_t;

/**
 * @brief 通信 APP 自有累计统计
 * @note 不复制 UART Service 的接收、丢失和高水位统计。
 */
typedef struct
{
    /** 成功读取的连续字节块数量。 */
    uint32_t processedChunkCount;
    /** 成功读取的连续字节总数。 */
    uint32_t processedByteCount;
    /** DATA_LOSS 恢复成功次数。 */
    uint32_t dataLossRecoveryCount;
    /** UART ERROR 恢复成功次数。 */
    uint32_t uartErrorRecoveryCount;
    /** 进入 ERROR 的致命错误次数。 */
    uint32_t fatalErrorCount;
    /** 接收到的完整合法命令数量。 */
    uint32_t commandReceivedCount;
    /** 非法 framing 或未知命令数量。 */
    uint32_t commandInvalidCount;
    /** 超过命令行缓冲区上限的行数量。 */
    uint32_t commandOverflowCount;
    /** 成功提交到 APP Control outlet 的事件数量。 */
    uint32_t controlEventSubmittedCount;
    /** APP Control outlet 缺失或提交失败数量。 */
    uint32_t controlEventSubmitFailureCount;
    /** 成功发出的 Communication-local 响应数量。 */
    uint32_t localResponseCount;
    /** 发出 Communication-local 响应失败数量。 */
    uint32_t localResponseFailureCount;
} app_communication_statistics_t;

/**
 * @brief 通信 APP 运行状态快照
 */
typedef struct
{
    /** 当前通信 APP 状态。 */
    app_communication_state_t state;
    /** 当前最近错误。 */
    platform_error_t lastError;
} app_communication_status_t;

/**
 * @brief 通信 APP 对象
 * @note 对象由 APP Composition Root 持有，使用前必须零初始化。
 */
typedef struct
{
    /** 静态依赖配置副本。 */
    app_communication_config_t config;
    /** 当前运行上下文。 */
    app_communication_context_t context;
    /** APP 自有累计统计。 */
    app_communication_statistics_t statistics;
} app_communication_t;
//******************************** Types ***********************************//

//******************************** Declaring *******************************//
/**
 * @brief 初始化通信 APP 并复制依赖配置
 * @param[in,out] communication : 使用 APP_COMMUNICATION_INITIALIZER 初始化的对象
 * @param[in] config : 调用者持有的 UART 与 UART Service 配置
 * @return PLATFORM_ERR_OK 成功；其他值表示参数或重复初始化错误
 * @note 本函数不启动 UART hardware 或 UART Service RX Session。
 */
platform_error_t app_communication_init(
    app_communication_t *communication,
    const app_communication_config_t *config);
/**
 * @brief 在 Communication Task 上启动 UART 和 UART Service
 * @param[in,out] communication : 已初始化的通信 APP 对象
 * @return PLATFORM_ERR_OK 成功；其他值表示 lifecycle 或 Service 启动失败
 */
platform_error_t app_communication_start(app_communication_t *communication);
/**
 * @brief 执行一次 UART Service 等待、读取和恢复周期
 * @param[in,out] communication : 正在运行的通信 APP 对象
 * @param[in] timeoutMs : 等待超时时间，单位为毫秒
 * @return PLATFORM_ERR_OK 成功或正常空闲；其他值表示不可恢复错误
 */
platform_error_t app_communication_process(app_communication_t *communication, uint32_t timeoutMs);
/**
 * @brief 获取通信 APP 当前状态快照
 * @param[in] communication : 已初始化的通信 APP 对象
 * @param[out] status : 输出状态快照
 * @return PLATFORM_ERR_OK 成功；其他值表示参数或状态错误
 */
platform_error_t app_communication_get_status(
    const app_communication_t *communication,
    app_communication_status_t *status);
/**
 * @brief 获取通信 APP 自有累计统计快照
 * @param[in] communication : 已初始化的通信 APP 对象
 * @param[out] statistics : 输出统计快照
 * @return PLATFORM_ERR_OK 成功；其他值表示参数或状态错误
 */
platform_error_t app_communication_get_statistics(
    const app_communication_t *communication,
    app_communication_statistics_t *statistics);
/**
 * @brief Communication Task 正式入口
 * @param[in] argument : 指向 app_communication_t 的 APP 对象
 * @return 无
 */
void app_communication_task_entry(void *argument);
//******************************** Declaring *******************************//

#endif
