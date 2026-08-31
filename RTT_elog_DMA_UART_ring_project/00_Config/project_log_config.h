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
#include <stdbool.h>
#include "service_log.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define PROJECT_LOG_DEFAULT_LEVEL          SERVICE_LOG_LEVEL_INFO
#define PROJECT_LOG_DEFAULT_OUTPUT_ENABLE  true
//******************************** Defines *********************************//

#endif
