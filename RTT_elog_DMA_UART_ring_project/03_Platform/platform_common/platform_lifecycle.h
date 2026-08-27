/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_lifecycle.h
 * @brief platform层，对象生命周期入口
 * @author YaoQian Wang
 * @date 2026-06-25
 * @version V1.0
 * @note 
 * @warning 
 * @history 
 * 1. 2026-06-25 创建项目
 *
 *****************************************************************************/
#ifndef PLATFORM_LIFECYCLE_H
#define PLATFORM_LIFECYCLE_H
//******************************** Includes *********************************//
#include "platform_types.h"
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Declaring *********************************//
/*生命周期函数指针结构体*/
// void *self 表示当前对象本身，类似于this指针
typedef struct 
{
    platform_error_t (*init)    (void *self); //对象初始化入口
    platform_error_t (*start)   (void *self); //对象启动入口
    platform_error_t (*process) (void *self); //周期处理入口
    platform_error_t (*stop)    (void *self); //对象停止入口
    platform_error_t (*deinit)  (void *self); //对象资源释放入口
}platform_lifecycle_ops_t;

//******************************** Declaring *********************************//
#endif
