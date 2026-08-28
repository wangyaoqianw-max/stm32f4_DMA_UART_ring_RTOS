/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_uart.c
 * @brief 验证 Platform UART 对象和传输行为
 * @author Codex
 * @date 2026-08-28
 * @version V1.0
 *
 *****************************************************************************/

#include "platform_uart.h"

#define TEST_ASSERT(condition)          \
    do {                                \
        if (!(condition)) {             \
            return __LINE__;            \
        }                               \
    } while (0)

typedef struct
{
    platform_uart_t *uart;
    const uint8_t *data;
    uint8_t *buffer;
    platform_size_t dataLength;
    uint32_t timeoutMs;
    platform_size_t completedLength;
    platform_error_t result;
} fake_uart_context_t;

/**
 * @brief 为生命周期测试返回成功
 * @param[in] self : UART 对象
 * @param[out] 无
 * @return PLATFORM_ERR_OK
 */
static platform_error_t fake_lifecycle(void *self)
{
    /**
     * 本测试仅验证对象构造时保存生命周期表。
     **/
    (void)self;
    return PLATFORM_ERR_OK;
}

/**
 * @brief 记录一次阻塞写请求
 * @param[in] uart           : UART 对象
 * @param[in] data           : 发送缓冲区
 * @param[in] dataLength     : 发送长度
 * @param[in] timeoutMs      : 超时时间，单位毫秒
 * @param[out] writtenLength : 实际完成字节数
 * @return 预设的假实现结果
 */
static platform_error_t fake_write(platform_uart_t *uart,
                                   const uint8_t *data,
                                   platform_size_t dataLength,
                                   uint32_t timeoutMs,
                                   platform_size_t *writtenLength)
{
    fake_uart_context_t *context = (fake_uart_context_t *)uart->implContext;

    /**
     * 记录完整边界请求，使参数转发错误能被测试捕获。
     **/
    context->uart = uart;
    context->data = data;
    context->dataLength = dataLength;
    context->timeoutMs = timeoutMs;
    *writtenLength = context->completedLength;

    return context->result;
}

/**
 * @brief 记录一次阻塞读请求
 * @param[in] uart        : UART 对象
 * @param[out] buffer     : 接收缓冲区
 * @param[in] bufferSize  : 接收缓冲区容量
 * @param[in] timeoutMs   : 超时时间，单位毫秒
 * @param[out] readLength : 实际完成字节数
 * @return 预设的假实现结果
 */
static platform_error_t fake_read(platform_uart_t *uart,
                                  uint8_t *buffer,
                                  platform_size_t bufferSize,
                                  uint32_t timeoutMs,
                                  platform_size_t *readLength)
{
    fake_uart_context_t *context = (fake_uart_context_t *)uart->implContext;

    /**
     * 记录完整边界请求，使参数转发错误能被测试捕获。
     **/
    context->uart = uart;
    context->buffer = buffer;
    context->dataLength = bufferSize;
    context->timeoutMs = timeoutMs;
    *readLength = context->completedLength;

    return context->result;
}

static const platform_lifecycle_ops_t g_fakeLifecycleOps = {
    fake_lifecycle,
    fake_lifecycle,
    fake_lifecycle,
    fake_lifecycle,
    fake_lifecycle
};

static const platform_uart_ops_t g_fakeBlockingOps = {
    fake_write,
    fake_read,
    NULL,
    NULL,
    NULL
};

static const platform_uart_ops_t g_noBlockingOps = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/**
 * @brief 创建有效的初始化参数
 * @param[in] context : 假实现上下文
 * @param[out] 无
 * @return 完整填充的初始化参数
 */
static platform_uart_init_params_t make_valid_params(fake_uart_context_t *context)
{
    platform_uart_init_params_t params = {
        "uart-test",
        PLATFORM_DEVICE_CAP_NONE,
        {
            115200U,
            PLATFORM_UART_DATA_BITS_8,
            PLATFORM_UART_STOP_BITS_1,
            PLATFORM_UART_PARITY_NONE,
            PLATFORM_UART_FLOW_CONTROL_NONE,
            100U
        },
        &g_fakeLifecycleOps,
        &g_fakeBlockingOps,
        context,
        NULL,
        NULL
    };

    /**
     * 按值返回，用于验证 platform_uart_init 必须复制配置。
     **/
    return params;
}

/**
 * @brief 验证 UART 对象构造成功
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_init_constructs_uart_object(void)
{
    platform_uart_t uart = {0};
    fake_uart_context_t context = {0};
    platform_uart_init_params_t params = make_valid_params(&context);

    TEST_ASSERT(PLATFORM_ERR_OK == platform_uart_init(&uart, &params));
    TEST_ASSERT(PLATFORM_TRUE ==
                platform_object_is_valid(&uart.device.object,
                                         PLATFORM_OBJECT_DEVICE));
    TEST_ASSERT(PLATFORM_DEVICE_CLASS_UART == uart.device.dev_class);
    TEST_ASSERT(PLATFORM_DEVICE_POWER_OFF == uart.device.power_state);
    TEST_ASSERT(PLATFORM_OBJECT_CREATED == uart.device.object.state);
    TEST_ASSERT(115200U == uart.config.baudRate);
    TEST_ASSERT(&g_fakeBlockingOps == uart.ops);
    TEST_ASSERT(&context == uart.implContext);

    return 0;
}

/**
 * @brief 验证非法构造参数被拒绝
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_init_rejects_invalid_parameters(void)
{
    platform_uart_t uart = {0};
    fake_uart_context_t context = {0};
    platform_uart_init_params_t params = make_valid_params(&context);

    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_uart_init(NULL, &params));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_uart_init(&uart, NULL));

    params.name = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_uart_init(&uart, &params));
    params = make_valid_params(&context);
    params.lifecycle = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_uart_init(&uart, &params));
    params = make_valid_params(&context);
    params.ops = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_uart_init(&uart, &params));
    params = make_valid_params(&context);
    params.config.baudRate = 0U;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_uart_init(&uart, &params));
    params = make_valid_params(&context);
    params.config.dataBits = (platform_uart_data_bits_t)6;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_uart_init(&uart, &params));
    params = make_valid_params(&context);
    params.config.stopBits = PLATFORM_UART_STOP_BITS_MAX;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_uart_init(&uart, &params));
    params = make_valid_params(&context);
    params.config.parity = PLATFORM_UART_PARITY_MAX;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_uart_init(&uart, &params));
    params = make_valid_params(&context);
    params.config.flowControl = PLATFORM_UART_FLOW_CONTROL_MAX;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_uart_init(&uart, &params));
    params = make_valid_params(&context);
    params.config.defaultTimeoutMs = PLATFORM_UART_TIMEOUT_USE_DEFAULT;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_uart_init(&uart, &params));

    return 0;
}

/**
 * @brief 验证写接口的状态、参数、超时和返回行为
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_write_validates_and_delegates(void)
{
    platform_uart_t uart = {0};
    fake_uart_context_t context = {0};
    platform_uart_init_params_t params = make_valid_params(&context);
    uint8_t data[2] = {0x12U, 0x34U};
    platform_size_t writtenLength = 9U;

    TEST_ASSERT(PLATFORM_ERR_OK == platform_uart_init(&uart, &params));
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE ==
                platform_uart_write(&uart, data, 2U, 10U, &writtenLength));
    TEST_ASSERT(0U == writtenLength);

    uart.device.object.state = PLATFORM_OBJECT_STARTED;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_uart_write(&uart, NULL, 2U, 10U, &writtenLength));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_uart_write(&uart, data, 0U, 10U, &writtenLength));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_uart_write(&uart, data, 2U, 10U, NULL));

    context.completedLength = 2U;
    context.result = PLATFORM_ERR_OK;
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_uart_write(&uart,
                                    data,
                                    2U,
                                    PLATFORM_UART_TIMEOUT_USE_DEFAULT,
                                    &writtenLength));
    TEST_ASSERT(&uart == context.uart);
    TEST_ASSERT(data == context.data);
    TEST_ASSERT(2U == context.dataLength);
    TEST_ASSERT(100U == context.timeoutMs);
    TEST_ASSERT(2U == writtenLength);

    context.result = PLATFORM_ERR_BUSY;
    TEST_ASSERT(PLATFORM_ERR_BUSY ==
                platform_uart_write(&uart, data, 2U, 25U, &writtenLength));
    TEST_ASSERT(25U == context.timeoutMs);

    return 0;
}

/**
 * @brief 验证读接口的状态、参数、超时和返回行为
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_read_validates_and_delegates(void)
{
    platform_uart_t uart = {0};
    fake_uart_context_t context = {0};
    platform_uart_init_params_t params = make_valid_params(&context);
    uint8_t buffer[4] = {0};
    platform_size_t readLength = 9U;

    TEST_ASSERT(PLATFORM_ERR_OK == platform_uart_init(&uart, &params));
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE ==
                platform_uart_read(&uart, buffer, 4U, 10U, &readLength));
    TEST_ASSERT(0U == readLength);

    uart.device.object.state = PLATFORM_OBJECT_STARTED;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_uart_read(&uart, NULL, 4U, 10U, &readLength));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_uart_read(&uart, buffer, 0U, 10U, &readLength));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_uart_read(&uart, buffer, 4U, 10U, NULL));

    context.completedLength = 3U;
    context.result = PLATFORM_ERR_OK;
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_uart_read(&uart,
                                   buffer,
                                   4U,
                                   PLATFORM_UART_TIMEOUT_USE_DEFAULT,
                                   &readLength));
    TEST_ASSERT(&uart == context.uart);
    TEST_ASSERT(buffer == context.buffer);
    TEST_ASSERT(4U == context.dataLength);
    TEST_ASSERT(100U == context.timeoutMs);
    TEST_ASSERT(3U == readLength);

    context.result = PLATFORM_ERR_TIMEOUT;
    TEST_ASSERT(PLATFORM_ERR_TIMEOUT ==
                platform_uart_read(&uart, buffer, 4U, 20U, &readLength));
    TEST_ASSERT(20U == context.timeoutMs);

    return 0;
}

/**
 * @brief 验证未提供的阻塞 Ops 返回不支持
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_missing_blocking_ops_are_not_supported(void)
{
    platform_uart_t uart = {0};
    fake_uart_context_t context = {0};
    platform_uart_init_params_t params = make_valid_params(&context);
    uint8_t buffer[2] = {0};
    platform_size_t completedLength = 0U;

    params.ops = &g_noBlockingOps;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_uart_init(&uart, &params));
    uart.device.object.state = PLATFORM_OBJECT_STARTED;

    TEST_ASSERT(PLATFORM_ERR_NOT_SUPPORTED ==
                platform_uart_write(&uart,
                                    buffer,
                                    sizeof(buffer),
                                    10U,
                                    &completedLength));
    TEST_ASSERT(PLATFORM_ERR_NOT_SUPPORTED ==
                platform_uart_read(&uart,
                                   buffer,
                                   sizeof(buffer),
                                   10U,
                                   &completedLength));

    return 0;
}

/**
 * @brief 运行 Platform UART 对象和阻塞 API 测试
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
int main(void)
{
    int result = 0;

    /**
     * 首个契约失败时停止，并返回对应源码行号。
     **/
    result = test_init_constructs_uart_object();
    if (0 != result) {
        return result;
    }

    result = test_init_rejects_invalid_parameters();
    if (0 != result) {
        return result;
    }

    result = test_write_validates_and_delegates();
    if (0 != result) {
        return result;
    }

    result = test_read_validates_and_delegates();
    if (0 != result) {
        return result;
    }

    result = test_missing_blocking_ops_are_not_supported();
    if (0 != result) {
        return result;
    }

    return 0;
}
