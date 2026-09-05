/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_bsp_led.h
 * @brief Platform BSP Status LED 构造契约
 * @author YaoQian Wang
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PLATFORM_BSP_LED_H
#define PLATFORM_BSP_LED_H

//******************************** Includes *********************************//
#include "platform_led.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 构造并绑定逻辑状态 LED
 * @param[in,out] led : 调用者拥有的 Platform LED 对象存储
 * @return platform_error_t : 构造与绑定结果
 * @note 本函数只执行对象构造和板级 GPIO 绑定，不配置 LED 硬件。
 */
platform_error_t platform_bsp_led_construct_status_led(platform_led_t *led);
//******************************** Functions ********************************//

#endif
