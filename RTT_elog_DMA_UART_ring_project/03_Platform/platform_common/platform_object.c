/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_object.c
 * @brief platform层，对象基类函数实现
 * @author YaoQian Wang
 * @date 2026-06-25
 * @version V1.0
 * @note 
 * @warning 
 * @history 
 * 1. 2026-06-25 创建项目
 *
 *****************************************************************************/
//******************************** Includes *********************************//
#include "platform_def.h"
#include "platform_object.h"
//******************************** Includes *********************************//

//******************************** Functions *********************************//
/**
 * @brief 初始化平台对象的公共基础字段
 *
 * Steps:
 *  1. 检查对象指针是否有效
 *  2. 写入对象的magic，名称，类型和初始状态
 *  3. 清除标志位和数据
 *
 * @param[in] obj    : 指向对象本体的指针
 * @param[in] name   : 对象的名称
 * @param[in] type   : 对象的高层类型
 * @param[in] parent : 指向父对象的指针
 *
 * @return platform_error_t : 函数执行状态
 */
platform_error_t platform_object_init(platform_object_t *obj,
                                      const char *name,
                                      platform_object_type_t type,
                                      void *parent)
{
    /*1.检查指针对象是否有效*/
    if((NULL == obj)||(NULL == name)||(type >= PLATFORM_OBJECT_TYPE_MAX)){
        return PLATFORM_ERR_INVALID_PARAM;
    }
    /*2.写入对象的magic，名称，类型和初始状态*/
    obj->magic     = PLATFORM_OBJECT_MAGIC;
    obj->name      = name;
    obj->type      = type;
    obj->parent    = parent;
    /*3.清除标志位和数据*/
    obj->state     = PLATFORM_OBJECT_CREATED;
    obj->flags     = 0u;
    obj->user_data = NULL;

    return PLATFORM_ERR_OK;
}

/**
 * @brief 更新平台生命周期状态记录
 *
 * @param[in] obj   : 指向对象本体的指针
 * @param[in] state : 新的对象生命周期状态
 *
 * @return platform_error_t : 函数执行状态
 */
platform_error_t platform_object_set_state(platform_object_t *obj,
                                           platform_object_state_t state)
{
    /*1.检查指针对象是否有效*/
    if((NULL == obj)||(PLATFORM_OBJECT_MAGIC != obj->magic)){
        return PLATFORM_ERR_INVALID_PARAM;
    }
    /*2.记录生命周期状态*/
    obj->state = state;

    return PLATFORM_ERR_OK;
}

/**
 * @brief 检查平台对象指针是否匹配期望的对象类型
 *
 * @param[in] obj   : 指向对象本体的指针
 * @param[in] type  : 期望的平台对象高层类型
 *
 * @return platform_bool_t : 如果对象指针有效返回true
 */
platform_bool_t platform_object_is_valid(const platform_object_t *obj,
                                         platform_object_type_t type)
{
    return (NULL != obj)&&
           (PLATFORM_OBJECT_MAGIC == obj->magic)&&
           (obj->type == type);
}
//******************************** Functions *********************************//
