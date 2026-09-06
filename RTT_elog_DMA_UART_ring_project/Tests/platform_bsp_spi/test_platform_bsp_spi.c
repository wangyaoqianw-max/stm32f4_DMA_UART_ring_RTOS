/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_bsp_spi.c
 * @brief 验证 Platform BSP 显示 SPI Bus 绑定行为
 * @author YaoQian Wang
 * @date 2026-09-06
 * @version V1.0
 *
 *****************************************************************************/

#include <string.h>

#include "platform_bsp_spi.h"

#define TEST_ASSERT(condition)       \
    do {                             \
        if (!(condition)) {          \
            return __LINE__;         \
        }                            \
    } while (0)

typedef struct
{
    platform_spi_bus_t *bus;
    const char *name;
    uint32_t caps;
    platform_error_t result;
    uint32_t callCount;
} fake_spi_constructor_record_t;

static fake_spi_constructor_record_t g_fakeConstructor;

platform_error_t impl_platform_spi1_construct(
    platform_spi_bus_t *bus,
    const char *name,
    uint32_t caps)
{
    g_fakeConstructor.bus = bus;
    g_fakeConstructor.name = name;
    g_fakeConstructor.caps = caps;
    g_fakeConstructor.callCount++;
    return g_fakeConstructor.result;
}

static void fake_constructor_reset(void)
{
    (void)memset(&g_fakeConstructor, 0, sizeof(g_fakeConstructor));
    g_fakeConstructor.result = PLATFORM_ERR_OK;
}

static int test_display_bus_binding_rejects_null_storage(void)
{
    fake_constructor_reset();

    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_bsp_spi_construct_display_bus(NULL));
    TEST_ASSERT(0U == g_fakeConstructor.callCount);

    return 0;
}

static int test_display_bus_binding_forwards_spi1_identity(void)
{
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;

    fake_constructor_reset();

    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_bsp_spi_construct_display_bus(&bus));
    TEST_ASSERT(1U == g_fakeConstructor.callCount);
    TEST_ASSERT(&bus == g_fakeConstructor.bus);
    TEST_ASSERT(0 == strcmp("display_spi_bus", g_fakeConstructor.name));
    TEST_ASSERT(PLATFORM_DEVICE_CAP_NONE == g_fakeConstructor.caps);

    g_fakeConstructor.result = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO ==
                platform_bsp_spi_construct_display_bus(&bus));

    return 0;
}

int main(void)
{
    int result = test_display_bus_binding_rejects_null_storage();

    if (result != 0) {
        return result;
    }

    return test_display_bus_binding_forwards_spi1_identity();
}
