/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_gpio_types.c
 * @brief 验证 Platform GPIO 公共类型契约和 Header Isolation
 * @author Codex
 * @date 2026-09-01
 * @version V1.0
 *
 *****************************************************************************/

#include "platform_gpio_types.h"

_Static_assert(PLATFORM_GPIO_LEVEL_MAX > PLATFORM_GPIO_LEVEL_HIGH,
               "GPIO level sentinel 必须位于有效枚举之后");
_Static_assert(PLATFORM_GPIO_DIRECTION_MAX > PLATFORM_GPIO_DIRECTION_OUTPUT,
               "GPIO direction sentinel 必须位于有效枚举之后");
_Static_assert(PLATFORM_GPIO_PULL_MAX > PLATFORM_GPIO_PULL_DOWN,
               "GPIO pull sentinel 必须位于有效枚举之后");
_Static_assert(PLATFORM_GPIO_OUTPUT_MAX > PLATFORM_GPIO_OUTPUT_OPEN_DRAIN,
               "GPIO output sentinel 必须位于有效枚举之后");

/**
 * @brief 验证 GPIO 公共配置类型可以被 Host 调用者独立使用
 * @param[in] 无
 * @param[out] 无
 * @return 公共类型契约有效时返回 0
 */
int main(void)
{
    platform_gpio_config_t config = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_LOW
    };

    return ((PLATFORM_GPIO_DIRECTION_OUTPUT == config.direction) &&
            (PLATFORM_GPIO_LEVEL_LOW == config.initialLevel)) ? 0 : 1;
}
