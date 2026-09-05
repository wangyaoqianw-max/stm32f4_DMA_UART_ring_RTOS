/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file impl_platform_spi.h
 * @brief STM32 SPI1 的 Platform SPI Bus 构造入口
 * @author YaoQian Wang
 * @date 2026-09-05
 * @version V1.0
 *
 *****************************************************************************/

#ifndef IMPL_PLATFORM_SPI_H
#define IMPL_PLATFORM_SPI_H

//******************************** Includes *********************************//
#include "platform_spi.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 构造并绑定 STM32 SPI1 的 Platform SPI Bus 对象
 * @param[in,out] bus : 使用 PLATFORM_SPI_BUS_INITIALIZER 清零的 Bus 对象
 * @param[in] name : Platform SPI Bus 名称
 * @param[in] caps : 设备能力标志
 * @return platform_error_t : 构造结果；本函数不初始化或启动 SPI 硬件
 * @note CubeMX MX_SPI1_Init() 仍是 SPI1 硬件配置所有者。
 */
platform_error_t impl_platform_spi1_construct(
    platform_spi_bus_t *bus,
    const char *name,
    uint32_t caps);
//******************************** Functions ********************************//

#endif
