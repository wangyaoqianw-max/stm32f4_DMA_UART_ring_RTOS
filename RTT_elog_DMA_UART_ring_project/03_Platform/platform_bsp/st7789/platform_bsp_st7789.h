/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_bsp_st7789.h
 * @brief 当前板级 ST7789T3 静态构造接口
 * @author YaoQian Wang
 * @date 2026-09-06
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PLATFORM_BSP_ST7789_H
#define PLATFORM_BSP_ST7789_H

//******************************** Includes *********************************//
#include "platform_st7789.h"
//******************************** Includes *********************************//

//******************************** Declaring *********************************//
/**
 * @brief 构造并绑定当前板级 240x280 ST7789T3 显示设备
 * @param[in,out] display : 使用 PLATFORM_ST7789_INITIALIZER 清零的对象存储
 * @return platform_error_t : 静态构造结果
 * @note 只绑定 GPIO 和面板配置，不配置硬件、不延时、不发送命令。
 */
platform_error_t platform_bsp_st7789_construct_display(
    platform_st7789_t *display);
//******************************** Declaring *********************************//

#endif
