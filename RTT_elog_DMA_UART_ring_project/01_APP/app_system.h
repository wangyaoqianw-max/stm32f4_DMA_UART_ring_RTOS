/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_system.h
 * @brief APP 系统 Composition Root 公共接口
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_SYSTEM_H
#define APP_SYSTEM_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Declaring *******************************//
/**
 * @brief 装配 Communication APP 的静态对象和预调度依赖
 * @return PLATFORM_ERR_OK 成功；其他值表示 BSP、APP、Thread 或 Service 初始化失败
 * @note 必须在 osKernelInitialize() 后、osKernelStart() 前且仅调用一次。
 */
platform_error_t app_system_init(void);
//******************************** Declaring *******************************//

#endif
