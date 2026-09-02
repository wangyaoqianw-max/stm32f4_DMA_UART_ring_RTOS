/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file stm32f4xx_hal.h
 * @brief Platform BSP GPIO Host Test 使用的最小 HAL 类型替身
 * @author Codex
 * @date 2026-09-02
 * @version V1.0
 *
 *****************************************************************************/

#ifndef TEST_PLATFORM_BSP_GPIO_STM32F4XX_HAL_H
#define TEST_PLATFORM_BSP_GPIO_STM32F4XX_HAL_H

#include "platform_types.h"

typedef struct
{
    uint32_t marker;
} GPIO_TypeDef;

typedef struct
{
    uint32_t Pin;
    uint32_t Mode;
    uint32_t Pull;
    uint32_t Speed;
    uint32_t Alternate;
} GPIO_InitTypeDef;

typedef enum
{
    GPIO_PIN_RESET = 0U,
    GPIO_PIN_SET
} GPIO_PinState;

#define GPIO_PIN_0  ((uint16_t)0x0001U)
#define GPIO_PIN_1  ((uint16_t)0x0002U)
#define GPIO_PIN_2  ((uint16_t)0x0004U)
#define GPIO_PIN_3  ((uint16_t)0x0008U)
#define GPIO_PIN_4  ((uint16_t)0x0010U)
#define GPIO_PIN_5  ((uint16_t)0x0020U)
#define GPIO_PIN_6  ((uint16_t)0x0040U)
#define GPIO_PIN_7  ((uint16_t)0x0080U)
#define GPIO_PIN_8  ((uint16_t)0x0100U)
#define GPIO_PIN_9  ((uint16_t)0x0200U)
#define GPIO_PIN_10 ((uint16_t)0x0400U)
#define GPIO_PIN_11 ((uint16_t)0x0800U)
#define GPIO_PIN_12 ((uint16_t)0x1000U)
#define GPIO_PIN_13 ((uint16_t)0x2000U)
#define GPIO_PIN_14 ((uint16_t)0x4000U)
#define GPIO_PIN_15 ((uint16_t)0x8000U)
#define GPIO_PIN_All ((uint16_t)0xFFFFU)

#endif
