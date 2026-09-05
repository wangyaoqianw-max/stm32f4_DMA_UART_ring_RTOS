/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_bsp_gpio.h
 * @brief Platform BSP 逻辑 GPIO 构造契约
 * @author YaoQian Wang
 * @date 2026-09-02
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PLATFORM_BSP_GPIO_H
#define PLATFORM_BSP_GPIO_H

//******************************** Includes *********************************//
#include "platform_gpio.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 构造并绑定逻辑状态 LED GPIO
 * @param[in,out] gpio : 调用者拥有的 Platform GPIO 对象存储
 * @return platform_error_t : 构造与绑定结果
 * @note 本函数只执行对象构造和板级物理资源绑定，不配置硬件。
 */
platform_error_t platform_bsp_gpio_construct_status_led(
    platform_gpio_t *gpio);

/**
 * @brief 构造并绑定逻辑用户按键 GPIO
 * @param[in,out] gpio : 调用者拥有的 Platform GPIO 对象存储
 * @return platform_error_t : 构造与绑定结果
 * @note 本函数只执行对象构造和板级物理资源绑定，不配置硬件。
 */
platform_error_t platform_bsp_gpio_construct_user_key(
    platform_gpio_t *gpio);

/**
 * @brief 构造并绑定逻辑 Software I2C SCL GPIO
 * @param[in,out] gpio : 调用者拥有的 Platform GPIO 对象存储
 * @return platform_error_t : 构造与绑定结果
 * @note 本函数只执行对象构造和板级物理资源绑定，不配置硬件。
 */
platform_error_t platform_bsp_gpio_construct_soft_i2c_scl(
    platform_gpio_t *gpio);

/**
 * @brief 构造并绑定逻辑 Software I2C SDA GPIO
 * @param[in,out] gpio : 调用者拥有的 Platform GPIO 对象存储
 * @return platform_error_t : 构造与绑定结果
 * @note 本函数只执行对象构造和板级物理资源绑定，不配置硬件。
 */
platform_error_t platform_bsp_gpio_construct_soft_i2c_sda(
    platform_gpio_t *gpio);
//******************************** Functions ********************************//

#endif
