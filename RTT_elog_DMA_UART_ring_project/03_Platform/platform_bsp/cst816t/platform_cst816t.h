/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * @file platform_cst816t.h
 * @brief CST816T 单点触摸设备合同，原始坐标与界面无关
 * @author YaoQian Wang
 * @date 2026-10-03
 * @version V1.0
 *****************************************************************************/

#ifndef PLATFORM_CST816T_H
#define PLATFORM_CST816T_H

#include "platform_i2c.h"
#include "platform_gpio.h"

#define PLATFORM_CST816T_INITIALIZER {0}

typedef struct
{
    platform_i2c_t *i2c;
    platform_gpio_t *reset;
    uint8_t chipId;
    uint8_t firmwareVersion;
    platform_bool_t initialized;
} platform_cst816t_t;

typedef struct
{
    platform_bool_t pressed;
    uint16_t x;
    uint16_t y;
} platform_cst816t_sample_t;

/**
 * @brief 复位、识别并配置触屏；在 Task Context 阻塞至少110ms。
 * @param[in,out] device 零初始化对象；不拥有总线及GPIO存储。
 * @param[in,out] i2c 已初始化、由同一任务独占的触屏总线。
 * @param[in,out] reset 已构造的复位GPIO；本接口将其配置为推挽输出。
 * @return 初始化结果；失败时对象保持未初始化，可再次尝试。
 * @note 引用必须长期有效；重复初始化返回ALREADY_INITIALIZED。
 */
platform_error_t platform_cst816t_init(
    platform_cst816t_t *device,
    platform_i2c_t *i2c,
    platform_gpio_t *reset);

/**
 * @brief 在持有设备的任务中同步读取单点原始12bit坐标。
 * @param[in,out] device 已初始化设备。
 * @param[out] sample 成功时整体更新；失败时保持原值；释放坐标清零。
 * @return 总线错误或无效触点数错误；不从ISR调用。
 */
platform_error_t platform_cst816t_read_sample(
    platform_cst816t_t *device,
    platform_cst816t_sample_t *sample);

#endif
