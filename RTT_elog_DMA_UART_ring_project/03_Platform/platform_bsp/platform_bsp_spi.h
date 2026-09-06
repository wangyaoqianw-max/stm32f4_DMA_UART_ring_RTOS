/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_bsp_spi.h
 * @brief Platform BSP 显示 SPI Bus 构造契约
 * @author YaoQian Wang
 * @date 2026-09-06
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PLATFORM_BSP_SPI_H
#define PLATFORM_BSP_SPI_H

//******************************** Includes *********************************//
#include "platform_spi.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 构造并绑定显示设备使用的 SPI Bus
 * @param[in,out] bus : 调用者拥有的 Platform SPI Bus 对象存储
 * @return platform_error_t : 构造与绑定结果
 * @note 本函数只执行构造和板级绑定，不执行 SPI Bus 生命周期操作。
 */
platform_error_t platform_bsp_spi_construct_display_bus(
    platform_spi_bus_t *bus);
//******************************** Functions ********************************//

#endif
