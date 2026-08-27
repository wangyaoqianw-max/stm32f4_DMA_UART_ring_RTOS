/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_service.h
 * @brief platform层，定义平台服务类型
 * @author YaoQian Wang
 * @date 2026-08-27
 * @version V1.0
 * @note 
 * @warning 
 * @history 
 * 1. 2026-08-27 创建项目
 *
 *****************************************************************************/
#ifndef PLATFORM_SERVICE_H
#define PLATFORM_SERVICE_H
//******************************** Includes *********************************//
#include "platform_types.h"
#include "platform_error.h"
#include "platform_object.h"
#include "platform_lifecycle.h"
#include "platform_def.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//

//******************************** Defines *********************************//

//******************************** Declaring *********************************//
/*平台服务分类*/
typedef enum
{
    PLATFORM_SERVICE_CLASS_SYSTEM,
    PLATFORM_SERVICE_CLASS_SENSOR,
    PLATFORM_SERVICE_CLASS_BATTERY,
    PLATFORM_SERVICE_CLASS_POWER,
    PLATFORM_SERVICE_CLASS_STORAGE,
    PLATFORM_SERVICE_CLASS_BACKLIGHT,
    PLATFORM_SERVICE_CLASS_BLE,
    PLATFORM_SERVICE_CLASS_OTA,

    PLATFORM_SERVICE_CLASS_MAX
}platform_service_class_t;

/*平台服务对象*/
typedef struct 
{
    platform_object_t object;
    platform_service_class_t service_class;
    const platform_lifecycle_ops_t *lifecycle;
}platform_service_t;

/**
 * @brief 初始化服务对象的公共基础字段
 *
 * @param[in] p_svc         : 指向服务对象本体的指针
 * @param[in] name          : 服务的名称
 * @param[in] service_class : 服务功能的细分类型
 * @param[in] p_lifecycle   : 生命周期管理的指针
 *
 * @return platform_error_t : 函数执行状态
 */
platform_error_t platform_service_init(platform_service_t *p_svc,
                                       const char *p_name,
                                       platform_service_class_t service_class,
                                       const platform_lifecycle_ops_t *p_lifecycle);

//******************************** Declaring *********************************//

#endif
