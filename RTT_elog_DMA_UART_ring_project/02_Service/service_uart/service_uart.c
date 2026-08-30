/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_uart.c
 * @brief UART 接收 Service 生命周期实现
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "service_uart.h"
//******************************** Includes *********************************//

//******************************** Functions *********************************//
/**
 * @brief 接收 Platform UART 异步事件
 * @param[in] uart            : 产生事件的 UART 对象
 * @param[in] event           : 异步事件
 * @param[in] callbackContext : Service 对象
 * @return 无
 */
static void service_uart_handle_platform_event(
    platform_uart_t *uart,
    const platform_uart_event_t *event,
    void *callbackContext)
{
    /**
     * Task 2 仅建立回调归属；RX 数据、错误和取消处理由后续 Task 实现。
     **/
    (void)uart;
    (void)event;
    (void)callbackContext;
}

/**
 * @brief 校验 Service 初始化配置
 * @param[in] service : Service 对象
 * @param[in] config  : Service 配置
 * @param[out] 无
 * @return platform_error_t : 参数校验结果
 */
static platform_error_t service_uart_validate_init_config(
    const service_uart_t *service,
    const service_uart_config_t *config)
{
    /**
     * 外部 UART、线程和两块调用者持有的存储都属于 Service 的必要依赖。
     **/
    if ((NULL == service) || (NULL == config) || (NULL == config->uart) ||
        (NULL == config->dmaRxBuffer) || (0U == config->dmaRxBufferSize) ||
        (NULL == config->ringBufferStorage) ||
        (2U > config->ringBufferStorageSize) ||
        (NULL == config->consumerThread)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return PLATFORM_ERR_OK;
}

/**
 * @brief 初始化 UART Service 对象并绑定 Platform UART 回调
 * @param[in,out] service : 使用 SERVICE_UART_INITIALIZER 初始化的 Service 对象
 * @param[in] config      : Service 配置
 * @param[out] 无
 * @return platform_error_t : 函数执行状态
 */
platform_error_t service_uart_init(service_uart_t *service,
                                   const service_uart_config_t *config)
{
    platform_error_t result = PLATFORM_ERR_OK;

    /**
     * 只有全部内部状态完成且回调绑定成功后，才对外发布 INITIALIZED。
     **/
    result = service_uart_validate_init_config(service, config);
    if (PLATFORM_ERR_OK != result) {
        return result;
    }

    if (SERVICE_UART_STATE_UNINITIALIZED != service->context.state) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    result = ring_buffer_init(&service->context.rxRingBuffer,
                              config->ringBufferStorage,
                              config->ringBufferStorageSize);
    if (PLATFORM_ERR_OK != result) {
        return result;
    }

    service->config = *config;
    service->context.lastError = PLATFORM_ERR_OK;
    service->context.dataLossOccurred = PLATFORM_FALSE;
    service->statistics = (service_uart_statistics_t){0};

    result = platform_uart_set_callback(service->config.uart,
                                        service_uart_handle_platform_event,
                                        service);
    if (PLATFORM_ERR_OK != result) {
        *service = (service_uart_t)SERVICE_UART_INITIALIZER;
        return result;
    }

    service->context.state = SERVICE_UART_STATE_INITIALIZED;

    return PLATFORM_ERR_OK;
}

/**
 * @brief 解绑 Platform UART 回调并清理 Service 对象
 * @param[in,out] service : 已初始化且无活动 RX Session 的 Service 对象
 * @param[out] 无
 * @return platform_error_t : 函数执行状态
 */
platform_error_t service_uart_deinit(service_uart_t *service)
{
    platform_error_t result = PLATFORM_ERR_OK;

    /**
     * 运行中的 RX Session 必须先由 stop 完结，解绑失败时保留完整 Service 状态供调用者处理。
     **/
    if (NULL == service) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (SERVICE_UART_STATE_UNINITIALIZED == service->context.state) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    if ((SERVICE_UART_STATE_INITIALIZED != service->context.state) &&
        (SERVICE_UART_STATE_STOPPED != service->context.state) &&
        (SERVICE_UART_STATE_ERROR != service->context.state)) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = platform_uart_set_callback(service->config.uart, NULL, NULL);
    if (PLATFORM_ERR_OK != result) {
        return result;
    }

    *service = (service_uart_t)SERVICE_UART_INITIALIZER;

    return PLATFORM_ERR_OK;
}
