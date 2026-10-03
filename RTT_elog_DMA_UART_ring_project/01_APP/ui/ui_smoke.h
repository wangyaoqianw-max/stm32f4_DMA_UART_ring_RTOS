/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file ui_smoke.h
 * @brief 声明 LVGL 临时显示与触摸验收页面
 * @author YaoQian Wang
 * @date 2026-10-03
 * @version V1.0
 *****************************************************************************/

#ifndef UI_SMOKE_H
#define UI_SMOKE_H

#include "platform_error.h"

/** @brief 在 Display Task 中创建临时验收页面。 */
platform_error_t ui_smoke_create(void);

#endif
