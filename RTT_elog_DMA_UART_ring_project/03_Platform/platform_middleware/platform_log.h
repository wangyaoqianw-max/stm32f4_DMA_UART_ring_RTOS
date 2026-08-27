/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_log.h
 * @brief platform层，日志集中管理头文件
 * @author YaoQian Wang
 * @date 2026-05-09
 * @version V1.0
 * @note 只定义接口，不做具体实现
 * @warning 
 * @history
 * 1. 2026-05-09 创建项目
 *
 *****************************************************************************/
#ifndef PLATFORM_LOG_H
#define PLATFORM_LOG_H

//******************************** Includes *********************************//
#include <stdbool.h>
#include "easylogger_port.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
/*对日志错误状态进行枚举定义*/
typedef enum
{
    PLATFORM_LOG_OK              = 0,        //操作成功
    PLATFORM_LOG_ERROR           = 1,        //通用错误
    PLATFORM_LOG_ERROR_PARAMETER = 2,        //参数错误
    PLATFORM_LOG_ERROR_INIT      = 3,        //初始化失败
    PLATFORM_LOG_ERROR_RESOURCE  = 4,        //资源不足
    PLATFORM_LOG_RESETRVED       = 0x7FFFFFFF//预留
}Platform_Log_Error_t;

/*对日志等级进行枚举定义*/
typedef enum
{
    PLATFORM_LOG_LEVEL_ASSERT = 0,
    PLATFORM_LOG_LEVEL_ERROR,
    PLATFORM_LOG_LEVEL_WARN,
    PLATFORM_LOG_LEVEL_INFO,
    PLATFORM_LOG_LEVEL_DEBUG,
    PLATFORM_LOG_LEVEL_VERBOSE,
    PLATFORM_LOG_LEVEL_MAX
}Platform_Log_Level_t;

#define platform_log_e(...)      impl_elog_e(LOG_TAG, __VA_ARGS__)
#define platform_log_w(...)      impl_elog_w(LOG_TAG, __VA_ARGS__)
#define platform_log_i(...)      impl_elog_i(LOG_TAG, __VA_ARGS__)
#define platform_log_d(...)      impl_elog_d(LOG_TAG, __VA_ARGS__)
#define platform_log_v(...)      impl_elog_v(LOG_TAG, __VA_ARGS__)
//******************************** Defines *********************************//

//******************************** Function *********************************//
/**
 * @brief  对日志进行初始化
 * @return Platform_Log_Error_t 返回PLATFORM_LOG_OK表示成功，其他表示失败
 */
Platform_Log_Error_t Platform_Log_Init(void);

/**
 * @brief  设置可以输出的日志的级别
 * @param  level  代表要设置的日志级别
 * @return Platform_Log_Error_t 返回PLATFORM_LOG_OK表示成功，其他表示失败
 */
Platform_Log_Error_t Platform_Log_SetLevel(Platform_Log_Level_t level);

/**
 * @brief  打开/关闭日志输出
 * @param  enable  代表日志的开关指令
 * @return Platform_Log_Error_t 返回PLATFORM_LOG_OK表示成功，其他表示失败
 */
Platform_Log_Error_t Platform_Log_EnableOutput(bool enable);
//******************************** Function *********************************//

#endif
