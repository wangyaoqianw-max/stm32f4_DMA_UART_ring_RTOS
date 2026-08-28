/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_uart.c
 * @brief Platform UART 抽象接口实现
 * @author YaoQian Wang
 * @date 2026-08-28
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_uart.h"
//******************************** Includes *********************************//

//******************************** Functions *********************************//
/**
 * @brief 校验 UART 配置的公共取值范围
 * @param[in] config : UART 配置
 * @param[out] 无
 * @return platform_error_t : 配置校验结果
 */
static platform_error_t platform_uart_validate_config(
    const platform_uart_config_t *config)
{
    /**
     * 按公共契约逐项校验，不假设枚举值必然连续。
     **/
    if ((NULL == config) || (0U == config->baudRate)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if ((PLATFORM_UART_DATA_BITS_7 != config->dataBits) &&
        (PLATFORM_UART_DATA_BITS_8 != config->dataBits) &&
        (PLATFORM_UART_DATA_BITS_9 != config->dataBits)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if ((config->stopBits < PLATFORM_UART_STOP_BITS_1) ||
        (config->stopBits >= PLATFORM_UART_STOP_BITS_MAX)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if ((config->parity < PLATFORM_UART_PARITY_NONE) ||
        (config->parity >= PLATFORM_UART_PARITY_MAX)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if ((config->flowControl < PLATFORM_UART_FLOW_CONTROL_NONE) ||
        (config->flowControl >= PLATFORM_UART_FLOW_CONTROL_MAX)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (PLATFORM_UART_TIMEOUT_USE_DEFAULT == config->defaultTimeoutMs) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return PLATFORM_ERR_OK;
}

/**
 * @brief 校验 UART 对象是否允许执行数据操作
 * @param[in] uart : UART 对象
 * @param[out] 无
 * @return platform_error_t : 对象和状态校验结果
 */
static platform_error_t platform_uart_validate_ready(const platform_uart_t *uart)
{
    /**
     * 先区分空指针、未构造对象和生命周期状态错误。
     **/
    if (NULL == uart) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if ((PLATFORM_TRUE !=
         platform_object_is_valid(&uart->device.object, PLATFORM_OBJECT_DEVICE)) ||
        (PLATFORM_DEVICE_CLASS_UART != uart->device.dev_class)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    if (PLATFORM_OBJECT_STARTED != uart->device.object.state) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    return PLATFORM_ERR_OK;
}

/**
 * @brief 将默认超时标记转换为对象配置值
 * @param[in] uart      : UART 对象
 * @param[in] timeoutMs : 调用者指定的超时值
 * @param[out] 无
 * @return 传递给 Impl 的实际超时值
 */
static uint32_t platform_uart_resolve_timeout(const platform_uart_t *uart,
                                              uint32_t timeoutMs)
{
    /**
     * 仅对显式默认标记做替换，0 和永久等待值原样传递。
     **/
    if (PLATFORM_UART_TIMEOUT_USE_DEFAULT == timeoutMs) {
        return uart->config.defaultTimeoutMs;
    }

    return timeoutMs;
}

/**
 * @brief 构造 Platform UART 对象
 * @param[out] uart  : 待构造的 UART 对象
 * @param[in] params : UART 初始化参数
 * @return platform_error_t : 函数执行状态
 */
platform_error_t platform_uart_init(platform_uart_t *uart,
                                    const platform_uart_init_params_t *params)
{
    platform_error_t result = PLATFORM_ERR_OK;

    /**
     * 对象基类和 Ops 是 UART 对象可用的必要条件。
     **/
    if ((NULL == uart) || (NULL == params) || (NULL == params->name) ||
        (NULL == params->lifecycle) || (NULL == params->ops)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    result = platform_uart_validate_config(&params->config);
    if (PLATFORM_ERR_OK != result) {
        return result;
    }

    result = platform_device_init(&uart->device,
                                  params->name,
                                  PLATFORM_DEVICE_CLASS_UART,
                                  params->caps,
                                  params->lifecycle);
    if (PLATFORM_ERR_OK != result) {
        return result;
    }

    /**
     * 复制配置以解除 UART 对象对外部配置存储期的依赖。
     **/
    uart->config = params->config;
    uart->ops = params->ops;
    uart->implContext = params->implContext;
    uart->callback = params->callback;
    uart->callbackContext = params->callbackContext;

    return PLATFORM_ERR_OK;
}

/**
 * @brief 校验并转发一次阻塞 UART 发送
 * @param[in] uart            : UART 对象
 * @param[in] data            : 发送缓冲区
 * @param[in] dataLength      : 发送长度
 * @param[in] timeoutMs       : 超时时间，单位毫秒
 * @param[out] writtenLength  : 实际完成长度
 * @return platform_error_t : 函数执行状态
 */
platform_error_t platform_uart_write(platform_uart_t *uart,
                                     const uint8_t *data,
                                     platform_size_t dataLength,
                                     uint32_t timeoutMs,
                                     platform_size_t *writtenLength)
{
    platform_error_t result = PLATFORM_ERR_OK;

    /**
     * 所有退出路径都使可用的完成长度保持明确的零值。
     **/
    if (NULL != writtenLength) {
        *writtenLength = 0U;
    }

    if ((NULL == data) || (0U == dataLength) || (NULL == writtenLength)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    result = platform_uart_validate_ready(uart);
    if (PLATFORM_ERR_OK != result) {
        return result;
    }

    if (NULL == uart->ops->write) {
        return PLATFORM_ERR_NOT_SUPPORTED;
    }

    return uart->ops->write(uart,
                            data,
                            dataLength,
                            platform_uart_resolve_timeout(uart, timeoutMs),
                            writtenLength);
}

/**
 * @brief 校验并转发一次阻塞 UART 接收
 * @param[in] uart         : UART 对象
 * @param[out] buffer      : 接收缓冲区
 * @param[in] bufferSize   : 接收缓冲区容量
 * @param[in] timeoutMs    : 超时时间，单位毫秒
 * @param[out] readLength  : 实际完成长度
 * @return platform_error_t : 函数执行状态
 */
platform_error_t platform_uart_read(platform_uart_t *uart,
                                    uint8_t *buffer,
                                    platform_size_t bufferSize,
                                    uint32_t timeoutMs,
                                    platform_size_t *readLength)
{
    platform_error_t result = PLATFORM_ERR_OK;

    /**
     * 所有退出路径都使可用的完成长度保持明确的零值。
     **/
    if (NULL != readLength) {
        *readLength = 0U;
    }

    if ((NULL == buffer) || (0U == bufferSize) || (NULL == readLength)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    result = platform_uart_validate_ready(uart);
    if (PLATFORM_ERR_OK != result) {
        return result;
    }

    if (NULL == uart->ops->read) {
        return PLATFORM_ERR_NOT_SUPPORTED;
    }

    return uart->ops->read(uart,
                           buffer,
                           bufferSize,
                           platform_uart_resolve_timeout(uart, timeoutMs),
                           readLength);
}
//******************************** Functions *********************************//
