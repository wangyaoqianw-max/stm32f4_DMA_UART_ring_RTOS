/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_led.h
 * @brief Platform LED 轻量对象和公共接口
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PLATFORM_LED_H
#define PLATFORM_LED_H

//******************************** Includes *********************************//
#include "platform_gpio.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
/*首次构造前使用此宏初始化 LED 对象存储*/
#define PLATFORM_LED_INITIALIZER {0}
//******************************** Defines *********************************//

//******************************** Declaring *********************************//
/*Platform LED 轻量对象；GPIO 由该对象直接拥有。*/
typedef struct platform_led
{
    platform_gpio_t gpio;
    platform_gpio_level_t activeLevel;
    platform_bool_t initialized;
} platform_led_t;

/**
 * @brief 初始化已完成 GPIO 绑定的 Platform LED 硬件
 * @param[in,out] led : 调用者持有的 LED 对象，其 gpio 必须已完成构造
 * @return platform_error_t : 初始化结果
 * @note activeLevel 必须为 PLATFORM_GPIO_LEVEL_LOW 或 HIGH。
 * @note 成功后 LED 保持 OFF；不拥有或释放底层 GPIO 的绑定 Context。
 */
platform_error_t platform_led_init(platform_led_t *led);

/**
 * @brief 点亮 LED
 * @param[in,out] led : 已完成硬件初始化的 LED 对象
 * @return platform_error_t : 操作结果
 */
platform_error_t platform_led_on(platform_led_t *led);

/**
 * @brief 熄灭 LED
 * @param[in,out] led : 已完成硬件初始化的 LED 对象
 * @return platform_error_t : 操作结果
 */
platform_error_t platform_led_off(platform_led_t *led);

/**
 * @brief 根据 GPIO 物理读回电平翻转 LED 状态
 * @param[in,out] led : 已完成硬件初始化的 LED 对象
 * @return platform_error_t : 操作结果
 */
platform_error_t platform_led_toggle(platform_led_t *led);

/**
 * @brief 反配置 LED 的底层 GPIO 硬件
 * @param[in,out] led : 已完成硬件初始化的 LED 对象
 * @return platform_error_t : 操作结果
 * @note 成功后 LED 可在既有 GPIO 绑定上再次初始化。
 */
platform_error_t platform_led_deinit(platform_led_t *led);
//******************************** Declaring *********************************//

#endif
