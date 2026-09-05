/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file project_log_config.h
 * @brief 定义产品级日志静态策略。
 * @author YaoQian Wang
 * @date 2026-08-31
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PROJECT_LOG_CONFIG_H
#define PROJECT_LOG_CONFIG_H

//******************************** Includes *********************************//
#include "service_log.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
/*
 * 产品级默认日志策略。
 * 默认级别决定初始化后允许输出的最低严重程度；输出开关可在运行时通过
 * Platform Log API 覆盖，本文件只提供上电时的初始值。
 */
#define PROJECT_LOG_DEFAULT_LEVEL          SERVICE_LOG_LEVEL_INFO
#define PROJECT_LOG_DEFAULT_OUTPUT_ENABLE  PLATFORM_TRUE
//******************************** Defines *********************************//

#endif
