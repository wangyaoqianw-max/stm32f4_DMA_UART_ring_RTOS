/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_bsp_uart.c
 * @brief 验证 Platform BSP 通信串口绑定行为
 * @author YaoQian Wang
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#include <string.h>

#include "platform_bsp_uart.h"

#define TEST_ASSERT(condition)          \
    do {                                \
        if (!(condition)) {             \
            return __LINE__;            \
        }                               \
    } while (0)

typedef struct
{
    uint32_t callCount;
    platform_uart_t *uart;
    const char *name;
    uint32_t caps;
    const platform_uart_config_t *config;
    platform_uart_callback_t callback;
    void *callbackContext;
    platform_error_t result;
} fake_constructor_record_t;

static fake_constructor_record_t g_fakeConstructor;

/**
 * @brief 重置 USART1 构造函数 Fake 记录
 * @param[in] 无
 * @param[out] 无
 * @return 无
 */
static void fake_constructor_reset(void)
{
    /**
     * 每个测试使用独立的构造函数观测记录。
     **/
    memset(&g_fakeConstructor, 0, sizeof(g_fakeConstructor));
    g_fakeConstructor.result = PLATFORM_ERR_OK;
}

/**
 * @brief 在不链接 STM32 HAL 代码时记录 BSP 转发参数
 * @param[in,out] uart : 调用者拥有的 Platform UART 对象存储
 * @param[in] name : UART 名称
 * @param[in] caps : 设备能力标志
 * @param[in] config : UART 配置
 * @param[in] callback : 可选 UART 回调
 * @param[in] callbackContext : 可选回调上下文
 * @return platform_error_t : 配置的 Fake 返回值
 */
platform_error_t impl_platform_uart_usart1_construct(
    platform_uart_t *uart,
    const char *name,
    uint32_t caps,
    const platform_uart_config_t *config,
    platform_uart_callback_t callback,
    void *callbackContext)
{
    /**
     * Fake 向 Host 测试暴露全部冻结的转发参数。
     **/
    g_fakeConstructor.callCount++;
    g_fakeConstructor.uart = uart;
    g_fakeConstructor.name = name;
    g_fakeConstructor.caps = caps;
    g_fakeConstructor.config = config;
    g_fakeConstructor.callback = callback;
    g_fakeConstructor.callbackContext = callbackContext;

    return g_fakeConstructor.result;
}

/**
 * @brief 验证 NULL UART 存储在转发前被拒绝
 * @param[in] 无
 * @param[out] 无
 * @return int : 成功返回零，断言失败返回源码行号
 */
static int test_construct_rejects_null_uart(void)
{
    platform_uart_config_t config = {0};

    /**
     * 无效调用者存储不得进入具体构造函数。
     **/
    fake_constructor_reset();
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_bsp_uart_construct_communication(NULL, &config));
    TEST_ASSERT(0U == g_fakeConstructor.callCount);

    return 0;
}

/**
 * @brief 验证 NULL 配置在转发前被拒绝
 * @param[in] 无
 * @param[out] 无
 * @return int : 成功返回零，断言失败返回源码行号
 */
static int test_construct_rejects_null_config(void)
{
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;

    /**
     * 配置所有权保留给调用者，且该参数必须存在。
     **/
    fake_constructor_reset();
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_bsp_uart_construct_communication(&uart, NULL));
    TEST_ASSERT(0U == g_fakeConstructor.callCount);

    return 0;
}

/**
 * @brief 验证有效构造转发冻结的通信串口绑定参数
 * @param[in] 无
 * @param[out] 无
 * @return int : 成功返回零，断言失败返回源码行号
 */
static int test_construct_forwards_frozen_binding_parameters(void)
{
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    platform_uart_config_t config = {0};

    /**
     * BSP 仅拥有逻辑角色到 USART1 的映射，不拥有调用者存储。
     **/
    fake_constructor_reset();
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_bsp_uart_construct_communication(&uart, &config));
    TEST_ASSERT(1U == g_fakeConstructor.callCount);
    TEST_ASSERT(&uart == g_fakeConstructor.uart);
    TEST_ASSERT(0 == strcmp("communication_uart", g_fakeConstructor.name));
    TEST_ASSERT(PLATFORM_DEVICE_CAP_NONE == g_fakeConstructor.caps);
    TEST_ASSERT(&config == g_fakeConstructor.config);
    TEST_ASSERT(NULL == g_fakeConstructor.callback);
    TEST_ASSERT(NULL == g_fakeConstructor.callbackContext);

    return 0;
}

/**
 * @brief 验证具体构造函数错误原样传播
 * @param[in] 无
 * @param[out] 无
 * @return int : 成功返回零，断言失败返回源码行号
 */
static int test_construct_propagates_constructor_error(void)
{
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    platform_uart_config_t config = {0};

    /**
     * 此薄绑定不得转换 UART 实现层返回的错误。
     **/
    fake_constructor_reset();
    g_fakeConstructor.result = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO ==
                platform_bsp_uart_construct_communication(&uart, &config));
    TEST_ASSERT(1U == g_fakeConstructor.callCount);

    return 0;
}

/**
 * @brief 运行 Platform BSP UART 绑定 Host 测试
 * @param[in] 无
 * @param[out] 无
 * @return int : 成功返回零，断言失败返回源码行号
 */
int main(void)
{
    int result = 0;

    /**
     * 独立执行每项冻结契约行为，以便明确定位失败行。
     **/
    result = test_construct_rejects_null_uart();
    if (0 != result) {
        return result;
    }

    result = test_construct_rejects_null_config();
    if (0 != result) {
        return result;
    }

    result = test_construct_forwards_frozen_binding_parameters();
    if (0 != result) {
        return result;
    }

    return test_construct_propagates_constructor_error();
}
