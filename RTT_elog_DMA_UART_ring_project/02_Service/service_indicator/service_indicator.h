/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_indicator.h
 * @brief 定义提示灯语义事件服务接口。
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

#ifndef SERVICE_INDICATOR_H
#define SERVICE_INDICATOR_H

//******************************** Includes *********************************//
#include "platform_def.h"
#include "led/platform_led.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define SERVICE_INDICATOR_INITIALIZER {0}
//******************************** Defines *********************************//

//******************************** Types ***********************************//
typedef enum
{
    SERVICE_INDICATOR_EVENT_STOPPED = 0,
    SERVICE_INDICATOR_EVENT_RUNNING = 1,
    SERVICE_INDICATOR_EVENT_ONCE_SUCCESS = 2
} service_indicator_event_t;

/** @brief 调用者拥有的提示灯服务上下文，不拥有 LED 对象存储。 */
typedef struct
{
    platform_led_t *led;
    platform_bool_t initialized;
} service_indicator_t;
//******************************** Types ***********************************//

//******************************** Declaring *********************************//
/**
 * @brief 绑定已完成硬件初始化的提示灯对象。
 * @param[in,out] service : 调用者拥有的服务上下文。
 * @param[in,out] led : 已初始化的 Platform LED 对象；服务仅保存引用。
 * @return PLATFORM_ERR_OK 表示成功，其他 platform_error_t 表示失败。
 */
platform_error_t service_indicator_init(
    service_indicator_t *service,
    platform_led_t *led);

/**
 * @brief 消费一个提示灯语义事件。
 * @param[in,out] service : 已初始化的服务上下文。
 * @param[in] event : STOPPED、RUNNING 或 ONCE_SUCCESS 语义事件。
 * @return PLATFORM_ERR_OK 表示成功，其他 platform_error_t 表示失败。
 * @note ONCE_SUCCESS 在调用方提供的 Task Context 中阻塞 600 ms，完成后 LED 为 OFF。
 */
platform_error_t service_indicator_handle_event(
    service_indicator_t *service,
    service_indicator_event_t event);

/**
 * @brief 解除服务与 LED 的引用关系。
 * @param[in,out] service : 已初始化的服务上下文。
 * @return PLATFORM_ERR_OK 表示成功，其他 platform_error_t 表示失败。
 * @note 不反初始化或释放调用者拥有的 Platform LED 对象。
 */
platform_error_t service_indicator_deinit(service_indicator_t *service);
//******************************** Declaring *********************************//

#endif
