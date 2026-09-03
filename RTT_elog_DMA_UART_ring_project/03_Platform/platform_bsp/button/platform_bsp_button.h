/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_bsp_button.h
 * @brief Platform BSP User Key Button 构造契约
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PLATFORM_BSP_BUTTON_H
#define PLATFORM_BSP_BUTTON_H

//******************************** Includes *********************************//
#include "platform_button.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 构造并绑定逻辑用户按键
 * @param[in,out] button : 调用者拥有的 Platform Button 对象存储
 * @return platform_error_t : 构造与绑定结果
 * @note 本函数只执行对象构造和板级 GPIO 绑定，不配置 Button 硬件。
 */
platform_error_t platform_bsp_button_construct_user_key(
    platform_button_t *button);
//******************************** Functions ********************************//

#endif
