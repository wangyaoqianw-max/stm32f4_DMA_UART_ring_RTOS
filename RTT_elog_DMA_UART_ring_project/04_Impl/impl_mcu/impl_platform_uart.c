/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file impl_platform_uart.c
 * @brief STM32 USART1 的 Platform UART 阻塞式实现
 * @author YaoQian Wang
 * @date 2026-08-29
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "impl_platform_uart.h"

#include "platform_device.h"
#include "platform_object.h"
#include "usart.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define STM32_UART_HAL_MAX_TRANSFER_SIZE ((platform_size_t)0xFFFFU)
//******************************** Defines *********************************//

//******************************** Declaring *********************************//
typedef struct
{
    UART_HandleTypeDef *halUart;
} stm32_uart_impl_context_t;
//******************************** Declaring *********************************//

//******************************** Variables *********************************//
static stm32_uart_impl_context_t g_usart1Context = {&huart1};
//******************************** Variables *********************************//

//******************************** Private Functions *************************//
static platform_error_t stm32_uart_map_hal_status(HAL_StatusTypeDef halStatus);
static platform_error_t stm32_uart_get_context(platform_uart_t *uart,
                                               stm32_uart_impl_context_t **context);
static platform_error_t stm32_uart_validate_object(const platform_uart_t *uart);
static platform_error_t stm32_uart_apply_config(UART_HandleTypeDef *halUart,
                                                const platform_uart_config_t *config);
static platform_error_t stm32_uart_map_word_length(const platform_uart_config_t *config,
                                                   uint32_t *wordLength);
static platform_error_t stm32_uart_map_stop_bits(platform_uart_stop_bits_t stopBits,
                                                 uint32_t *halStopBits);
static platform_error_t stm32_uart_map_parity(platform_uart_parity_t parity,
                                              uint32_t *halParity);
static platform_error_t stm32_uart_lifecycle_init(void *self);
static platform_error_t stm32_uart_lifecycle_start(void *self);
static platform_error_t stm32_uart_lifecycle_process(void *self);
static platform_error_t stm32_uart_lifecycle_stop(void *self);
static platform_error_t stm32_uart_lifecycle_deinit(void *self);
static platform_error_t stm32_uart_write(platform_uart_t *uart,
                                         const uint8_t *data,
                                         platform_size_t dataLength,
                                         uint32_t timeoutMs,
                                         platform_size_t *writtenLength);
static platform_error_t stm32_uart_read(platform_uart_t *uart,
                                        uint8_t *buffer,
                                        platform_size_t bufferSize,
                                        uint32_t timeoutMs,
                                        platform_size_t *readLength);
//******************************** Private Functions *************************//

//******************************** Constants *********************************//
static const platform_lifecycle_ops_t g_stm32UartLifecycleOps = {
    stm32_uart_lifecycle_init,
    stm32_uart_lifecycle_start,
    stm32_uart_lifecycle_process,
    stm32_uart_lifecycle_stop,
    stm32_uart_lifecycle_deinit
};

static const platform_uart_ops_t g_stm32UartOps = {
    stm32_uart_write,
    stm32_uart_read,
    NULL,
    NULL,
    NULL
};
//******************************** Constants *********************************//

//******************************** Private Functions *************************//
static platform_error_t stm32_uart_map_hal_status(HAL_StatusTypeDef halStatus)
{
    switch (halStatus) {
        case HAL_OK:
            return PLATFORM_ERR_OK;

        case HAL_BUSY:
            return PLATFORM_ERR_BUSY;

        case HAL_TIMEOUT:
            return PLATFORM_ERR_TIMEOUT;

        case HAL_ERROR:
            return PLATFORM_ERR_IO;

        default:
            return PLATFORM_ERR_UNKNOWN;
    }
}

static platform_error_t stm32_uart_get_context(platform_uart_t *uart,
                                               stm32_uart_impl_context_t **context)
{
    if ((uart == NULL) || (context == NULL) || (uart->implContext == NULL)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    *context = (stm32_uart_impl_context_t *)uart->implContext;
    if ((*context)->halUart == NULL) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t stm32_uart_validate_object(const platform_uart_t *uart)
{
    if (uart == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if ((platform_object_is_valid(&uart->device.object, PLATFORM_OBJECT_DEVICE) !=
         PLATFORM_TRUE) ||
        (uart->device.dev_class != PLATFORM_DEVICE_CLASS_UART)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t stm32_uart_apply_config(UART_HandleTypeDef *halUart,
                                                const platform_uart_config_t *config)
{
    platform_error_t result = PLATFORM_ERR_OK;
    uint32_t wordLength = 0U;
    uint32_t halStopBits = 0U;
    uint32_t halParity = 0U;

    if ((halUart == NULL) || (config == NULL)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    result = stm32_uart_map_word_length(config, &wordLength);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = stm32_uart_map_stop_bits(config->stopBits, &halStopBits);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = stm32_uart_map_parity(config->parity, &halParity);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (config->flowControl != PLATFORM_UART_FLOW_CONTROL_NONE) {
        return PLATFORM_ERR_NOT_SUPPORTED;
    }

    halUart->Init.BaudRate = config->baudRate;
    halUart->Init.WordLength = wordLength;
    halUart->Init.StopBits = halStopBits;
    halUart->Init.Parity = halParity;
    halUart->Init.Mode = UART_MODE_TX_RX;
    halUart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    halUart->Init.OverSampling = UART_OVERSAMPLING_16;

    return PLATFORM_ERR_OK;
}

static platform_error_t stm32_uart_map_word_length(const platform_uart_config_t *config,
                                                   uint32_t *wordLength)
{
    if ((config == NULL) || (wordLength == NULL)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    switch (config->dataBits) {
        case PLATFORM_UART_DATA_BITS_7:
            if (config->parity == PLATFORM_UART_PARITY_NONE) {
                return PLATFORM_ERR_NOT_SUPPORTED;
            }
            *wordLength = UART_WORDLENGTH_8B;
            return PLATFORM_ERR_OK;

        case PLATFORM_UART_DATA_BITS_8:
            if (config->parity == PLATFORM_UART_PARITY_NONE) {
                *wordLength = UART_WORDLENGTH_8B;
            } else {
                *wordLength = UART_WORDLENGTH_9B;
            }
            return PLATFORM_ERR_OK;

        case PLATFORM_UART_DATA_BITS_9:
            /* HAL 在 9-bit 无校验模式按 uint16_t 元素访问 Buffer，
             * 与 Platform UART 的 uint8_t 字节流契约不兼容。 */
            return PLATFORM_ERR_NOT_SUPPORTED;

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

static platform_error_t stm32_uart_map_stop_bits(platform_uart_stop_bits_t stopBits,
                                                 uint32_t *halStopBits)
{
    if (halStopBits == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    switch (stopBits) {
        case PLATFORM_UART_STOP_BITS_1:
            *halStopBits = UART_STOPBITS_1;
            return PLATFORM_ERR_OK;

        case PLATFORM_UART_STOP_BITS_2:
            *halStopBits = UART_STOPBITS_2;
            return PLATFORM_ERR_OK;

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

static platform_error_t stm32_uart_map_parity(platform_uart_parity_t parity,
                                              uint32_t *halParity)
{
    if (halParity == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    switch (parity) {
        case PLATFORM_UART_PARITY_NONE:
            *halParity = UART_PARITY_NONE;
            return PLATFORM_ERR_OK;

        case PLATFORM_UART_PARITY_EVEN:
            *halParity = UART_PARITY_EVEN;
            return PLATFORM_ERR_OK;

        case PLATFORM_UART_PARITY_ODD:
            *halParity = UART_PARITY_ODD;
            return PLATFORM_ERR_OK;

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

static platform_error_t stm32_uart_lifecycle_init(void *self)
{
    platform_error_t result = PLATFORM_ERR_OK;
    stm32_uart_impl_context_t *context = NULL;
    platform_uart_t *uart = (platform_uart_t *)self;

    result = stm32_uart_validate_object(uart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (uart->device.object.state != PLATFORM_OBJECT_CREATED) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = stm32_uart_get_context(uart, &context);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = stm32_uart_apply_config(context->halUart, &uart->config);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = stm32_uart_map_hal_status(HAL_UART_Init(context->halUart));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_object_set_state(&uart->device.object, PLATFORM_OBJECT_INITIALIZED);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return platform_device_set_power_state(&uart->device, PLATFORM_DEVICE_POWER_IDLE);
}

static platform_error_t stm32_uart_lifecycle_start(void *self)
{
    platform_error_t result = PLATFORM_ERR_OK;
    platform_uart_t *uart = (platform_uart_t *)self;

    result = stm32_uart_validate_object(uart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if ((uart->device.object.state != PLATFORM_OBJECT_INITIALIZED) &&
        (uart->device.object.state != PLATFORM_OBJECT_STOPPED)) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = platform_object_set_state(&uart->device.object, PLATFORM_OBJECT_STARTED);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return platform_device_set_power_state(&uart->device, PLATFORM_DEVICE_POWER_ACTIVE);
}

static platform_error_t stm32_uart_lifecycle_process(void *self)
{
    platform_error_t result = PLATFORM_ERR_OK;
    platform_uart_t *uart = (platform_uart_t *)self;

    result = stm32_uart_validate_object(uart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (uart->device.object.state != PLATFORM_OBJECT_STARTED) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t stm32_uart_lifecycle_stop(void *self)
{
    platform_error_t result = PLATFORM_ERR_OK;
    platform_uart_t *uart = (platform_uart_t *)self;

    result = stm32_uart_validate_object(uart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (uart->device.object.state != PLATFORM_OBJECT_STARTED) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = platform_object_set_state(&uart->device.object, PLATFORM_OBJECT_STOPPED);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return platform_device_set_power_state(&uart->device, PLATFORM_DEVICE_POWER_IDLE);
}

static platform_error_t stm32_uart_lifecycle_deinit(void *self)
{
    platform_error_t result = PLATFORM_ERR_OK;
    stm32_uart_impl_context_t *context = NULL;
    platform_uart_t *uart = (platform_uart_t *)self;

    result = stm32_uart_validate_object(uart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (uart->device.object.state != PLATFORM_OBJECT_STOPPED) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = stm32_uart_get_context(uart, &context);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = stm32_uart_map_hal_status(HAL_UART_DeInit(context->halUart));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_device_set_power_state(&uart->device, PLATFORM_DEVICE_POWER_OFF);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return platform_object_set_state(&uart->device.object, PLATFORM_OBJECT_CREATED);
}

static platform_error_t stm32_uart_write(platform_uart_t *uart,
                                         const uint8_t *data,
                                         platform_size_t dataLength,
                                         uint32_t timeoutMs,
                                         platform_size_t *writtenLength)
{
    platform_error_t result = PLATFORM_ERR_OK;
    stm32_uart_impl_context_t *context = NULL;

    if (writtenLength != NULL) {
        *writtenLength = 0U;
    }

    if ((data == NULL) || (dataLength == 0U) || (writtenLength == NULL)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (dataLength > STM32_UART_HAL_MAX_TRANSFER_SIZE) {
        return PLATFORM_ERR_OVERFLOW;
    }

    result = stm32_uart_get_context(uart, &context);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = stm32_uart_map_hal_status(
        HAL_UART_Transmit(context->halUart, (uint8_t *)data, (uint16_t)dataLength, timeoutMs));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    *writtenLength = dataLength;
    return PLATFORM_ERR_OK;
}

static platform_error_t stm32_uart_read(platform_uart_t *uart,
                                        uint8_t *buffer,
                                        platform_size_t bufferSize,
                                        uint32_t timeoutMs,
                                        platform_size_t *readLength)
{
    platform_error_t result = PLATFORM_ERR_OK;
    stm32_uart_impl_context_t *context = NULL;

    if (readLength != NULL) {
        *readLength = 0U;
    }

    if ((buffer == NULL) || (bufferSize == 0U) || (readLength == NULL)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (bufferSize > STM32_UART_HAL_MAX_TRANSFER_SIZE) {
        return PLATFORM_ERR_OVERFLOW;
    }

    result = stm32_uart_get_context(uart, &context);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = stm32_uart_map_hal_status(
        HAL_UART_Receive(context->halUart, buffer, (uint16_t)bufferSize, timeoutMs));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    *readLength = bufferSize;
    return PLATFORM_ERR_OK;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t impl_platform_uart_usart1_construct(
    platform_uart_t *uart,
    const char *name,
    uint32_t caps,
    const platform_uart_config_t *config,
    platform_uart_callback_t callback,
    void *callbackContext)
{
    platform_uart_init_params_t params;

    if (config == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    params.name = name;
    params.caps = caps;
    params.config = *config;
    params.lifecycle = &g_stm32UartLifecycleOps;
    params.ops = &g_stm32UartOps;
    params.implContext = &g_usart1Context;
    params.callback = callback;
    params.callbackContext = callbackContext;

    return platform_uart_init(uart, &params);
}
//******************************** Functions *********************************//
