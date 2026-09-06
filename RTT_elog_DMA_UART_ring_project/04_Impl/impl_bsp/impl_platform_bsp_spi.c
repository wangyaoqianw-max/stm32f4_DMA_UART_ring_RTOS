/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file impl_platform_bsp_spi.c
 * @brief 当前板级 Platform BSP 显示 SPI Bus 绑定
 * @author YaoQian Wang
 * @date 2026-09-06
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_bsp_spi.h"

#include "impl_platform_spi.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
platform_error_t platform_bsp_spi_construct_display_bus(
    platform_spi_bus_t *bus)
{
    if (bus == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return impl_platform_spi1_construct(bus,
                                        "display_spi_bus",
                                        PLATFORM_DEVICE_CAP_NONE);
}
//******************************** Functions ********************************//
