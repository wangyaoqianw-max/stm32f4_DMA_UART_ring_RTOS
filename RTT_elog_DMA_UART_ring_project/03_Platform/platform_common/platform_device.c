/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_device.c
 * @brief platform层，平台设备的具体实现
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
#include "platform_device.h"
//******************************** Includes *********************************//

//******************************** Functions *********************************//
/**
 * @brief 初始化设备对象的公共基础字段
 *
 * @param[in] p_dev       : 指向设备对象本体的指针
 * @param[in] name        : 设备对象的名称
 * @param[in] dev_class   : 对象的细分类型
 * @param[in] caps        : 设备能力标志
 * @param[in] p_lifecycle : 生命周期管理的指针
 *
 * @return platform_error_t : 函数执行状态
 */
platform_error_t platform_device_init(platform_device_t *p_dev,
                                      const char *p_name,
                                      platform_device_class_t dev_class,
                                      uint32_t caps,
                                      const platform_lifecycle_ops_t *p_lifecycle)
{
    platform_error_t ret = PLATFORM_ERR_OK;
    /*1.参数校验*/
    if(NULL == p_dev){
        return PLATFORM_ERR_INVALID_PARAM;
    }
    /*2.初始化基类*/
    ret = platform_object_init(&p_dev->object,p_name,PLATFORM_OBJECT_DEVICE,NULL);
    if(PLATFORM_ERR_OK != ret) return ret;
    /*3.初始化其他值*/
    p_dev->dev_class = dev_class;
    p_dev->caps = caps;
    p_dev->power_state = PLATFORM_DEVICE_POWER_OFF;
    p_dev->lifecycle = p_lifecycle;

    return PLATFORM_ERR_OK;
}

/**
 * @brief 修改设备的电源及运行状态
 *
 * @param[in] p_dev       : 指向设备对象本体的指针
 * @param[in] power_atate : 电源及运行状态
 *
 * @return platform_error_t : 函数执行状态
 */
platform_error_t platform_device_set_power_state(
                                platform_device_t *p_dev,
                                platform_device_power_state_t power_atate)
{
    if(NULL == p_dev) return PLATFORM_ERR_INVALID_PARAM;
    p_dev->power_state = power_atate;
    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//

