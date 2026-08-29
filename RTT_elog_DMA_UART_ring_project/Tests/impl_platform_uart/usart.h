/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file usart.h
 * @brief Impl Platform UART 主机测试使用的最小 HAL UART 替身
 * @author YaoQian Wang
 * @date 2026-08-29
 * @version V1.0
 *
 *****************************************************************************/

#ifndef TEST_IMPL_PLATFORM_UART_USART_H
#define TEST_IMPL_PLATFORM_UART_USART_H

typedef enum
{
    HAL_OK = 0U,
    HAL_ERROR,
    HAL_BUSY,
    HAL_TIMEOUT
} HAL_StatusTypeDef;

typedef struct
{
    uint32_t BaudRate;
    uint32_t WordLength;
    uint32_t StopBits;
    uint32_t Parity;
    uint32_t Mode;
    uint32_t HwFlowCtl;
    uint32_t OverSampling;
} UART_InitTypeDef;

typedef struct
{
    void *Instance;
    UART_InitTypeDef Init;
} UART_HandleTypeDef;

#define UART_WORDLENGTH_8B  (0x00000000U)
#define UART_WORDLENGTH_9B  (0x00001000U)
#define UART_STOPBITS_1     (0x00000000U)
#define UART_STOPBITS_2     (0x00002000U)
#define UART_PARITY_NONE    (0x00000000U)
#define UART_PARITY_EVEN    (0x00000400U)
#define UART_PARITY_ODD     (0x00000600U)
#define UART_MODE_TX_RX     (0x0000000CU)
#define UART_HWCONTROL_NONE (0x00000000U)
#define UART_OVERSAMPLING_16 (0x00000000U)

extern UART_HandleTypeDef huart1;

static inline HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *halUart)
{
    (void)halUart;
    return HAL_OK;
}

static inline HAL_StatusTypeDef HAL_UART_DeInit(UART_HandleTypeDef *halUart)
{
    (void)halUart;
    return HAL_OK;
}

static inline HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *halUart,
                                                   uint8_t *data,
                                                   uint16_t dataLength,
                                                   uint32_t timeoutMs)
{
    (void)halUart;
    (void)data;
    (void)dataLength;
    (void)timeoutMs;
    return HAL_OK;
}

static inline HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *halUart,
                                                  uint8_t *buffer,
                                                  uint16_t bufferSize,
                                                  uint32_t timeoutMs)
{
    (void)halUart;
    (void)buffer;
    (void)bufferSize;
    (void)timeoutMs;
    return HAL_OK;
}

#endif
