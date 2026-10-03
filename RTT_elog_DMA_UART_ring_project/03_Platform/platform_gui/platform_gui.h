/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_gui.h
 * @brief 定义 Display Task 独占的 LVGL 硬件端口接口
 * @author YaoQian Wang
 * @date 2026-10-03
 * @version V1.0
 *****************************************************************************/

#ifndef PLATFORM_GUI_H
#define PLATFORM_GUI_H

#include "platform_cst816t.h"
#include "platform_st7789.h"

/**
 * @brief 初始化 LVGL、ST7789 刷新、缓存触摸输入和单调毫秒时基。
 * @param display 已初始化的 LCD；调用方保持其生命周期。
 * @param touchSample Display Task 持有的长期有效样本缓存，端口只读。
 * @return 成功为 PLATFORM_ERR_OK；参数或 LVGL 资源失败返回对应错误。
 */
platform_error_t platform_gui_init(platform_st7789_t *display,
                                   const platform_cst816t_sample_t *touchSample);

/**
 * @brief 在 Display Task 内驱动 LVGL 定时器并汇报本轮 flush 错误。
 * @return 本轮第一个 LCD 写入错误；无错误返回 PLATFORM_ERR_OK。
 */
platform_error_t platform_gui_process(void);

#endif
