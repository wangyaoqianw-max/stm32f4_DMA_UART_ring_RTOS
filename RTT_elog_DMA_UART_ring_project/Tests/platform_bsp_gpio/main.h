/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file main.h
 * @brief Platform BSP GPIO Host Test 使用的 CubeMX 资源宏替身
 * @author YaoQian Wang
 * @date 2026-09-02
 * @version V1.0
 *
 *****************************************************************************/

#ifndef TEST_PLATFORM_BSP_GPIO_MAIN_H
#define TEST_PLATFORM_BSP_GPIO_MAIN_H

#include "stm32f4xx_hal.h"

extern GPIO_TypeDef g_fakePortA;
extern GPIO_TypeDef g_fakePortB;
extern GPIO_TypeDef g_fakePortC;

#define GPIOA (&g_fakePortA)
#define GPIOB (&g_fakePortB)
#define GPIOC (&g_fakePortC)

#define LED_OUT_Pin       GPIO_PIN_13
#define LED_OUT_GPIO_Port GPIOC
#define KEY_IN_Pin        GPIO_PIN_0
#define KEY_IN_GPIO_Port  GPIOA
#define I2C_SCL_Pin       GPIO_PIN_6
#define I2C_SCL_GPIO_Port GPIOB
#define I2C_SDA_Pin       GPIO_PIN_7
#define I2C_SDA_GPIO_Port GPIOB

#endif
