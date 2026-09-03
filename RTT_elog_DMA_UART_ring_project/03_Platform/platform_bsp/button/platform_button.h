/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_button.h
 * @brief Platform Button 轻量对象和公共接口
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PLATFORM_BUTTON_H
#define PLATFORM_BUTTON_H

//******************************** Includes *********************************//
#include "platform_gpio.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
/*首次构造前使用此宏初始化 Button 对象存储*/
#define PLATFORM_BUTTON_INITIALIZER {0}
//******************************** Defines *********************************//

//******************************** Declaring *********************************//
typedef enum
{
    PLATFORM_BUTTON_STATE_RELEASED = 0,
    PLATFORM_BUTTON_STATE_PRESSED,
    PLATFORM_BUTTON_STATE_MAX
} platform_button_state_t;

/*Platform Button 轻量对象；GPIO 由该对象直接拥有。*/
typedef struct platform_button
{
    platform_gpio_t gpio;
    platform_gpio_level_t activeLevel;
    platform_gpio_pull_t pull;
    platform_bool_t initialized;
} platform_button_t;

/**
 * @brief 初始化已完成 GPIO 绑定的 Platform Button 硬件
 * @param[in,out] button : 调用者持有的 Button 对象，其 gpio 必须已完成构造
 * @return platform_error_t : 初始化结果
 * @note activeLevel 必须为 PLATFORM_GPIO_LEVEL_LOW 或 HIGH。
 * @note pull 必须为 PLATFORM_GPIO_PULL_NONE、UP 或 DOWN。
 */
platform_error_t platform_button_init(platform_button_t *button);

/**
 * @brief 读取按键逻辑状态
 * @param[in] button : 已完成硬件初始化的 Button 对象
 * @param[out] state : 按键逻辑状态
 * @return platform_error_t : 操作结果
 */
platform_error_t platform_button_read(platform_button_t *button,
                                      platform_button_state_t *state);

/**
 * @brief 反配置 Button 的底层 GPIO 硬件
 * @param[in,out] button : 已完成硬件初始化的 Button 对象
 * @return platform_error_t : 操作结果
 * @note 成功后 Button 可在既有 GPIO 绑定上再次初始化。
 */
platform_error_t platform_button_deinit(platform_button_t *button);
//******************************** Declaring *********************************//

#endif
