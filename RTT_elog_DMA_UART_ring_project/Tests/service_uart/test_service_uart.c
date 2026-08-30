/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_service_uart.c
 * @brief 验证 UART Service 对象生命周期
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#include "service_uart.h"

#define TEST_ASSERT(condition)          \
    do {                                \
        if (!(condition)) {             \
            return __LINE__;            \
        }                               \
    } while (0)

typedef struct
{
    platform_uart_callback_t callback;
    void *callbackContext;
    uint8_t *rxBuffer;
    platform_size_t rxBufferSize;
    platform_error_t setCallbackResult;
    platform_error_t readAsyncResult;
    platform_error_t cancelResult;
    uint32_t notifySetCount;
    uint32_t notifySetFromIsrCount;
    uint32_t setCallbackCallCount;
} fake_service_platform_t;

static fake_service_platform_t g_fakePlatform;

/**
 * @brief 重置 Platform 测试替身状态
 * @param[in] 无
 * @param[out] 无
 * @return 无
 */
static void fake_service_platform_reset(void)
{
    /**
     * 默认所有依赖调用成功，单项测试按需覆盖对应返回值。
     **/
    g_fakePlatform = (fake_service_platform_t){0};
    g_fakePlatform.setCallbackResult = PLATFORM_ERR_OK;
    g_fakePlatform.readAsyncResult = PLATFORM_ERR_OK;
    g_fakePlatform.cancelResult = PLATFORM_ERR_OK;
}

/**
 * @brief 记录 Service 对 Platform UART 回调绑定的请求
 * @param[in] uart            : Platform UART 对象
 * @param[in] callback        : 待绑定的回调
 * @param[in] callbackContext : 回调上下文
 * @return 预设的测试替身结果
 */
platform_error_t platform_uart_set_callback(platform_uart_t *uart,
                                            platform_uart_callback_t callback,
                                            void *callbackContext)
{
    /**
     * Service 生命周期测试只验证绑定请求，不依赖 Platform UART 的对象实现。
     **/
    (void)uart;
    g_fakePlatform.setCallbackCallCount++;

    if (PLATFORM_ERR_OK != g_fakePlatform.setCallbackResult) {
        return g_fakePlatform.setCallbackResult;
    }

    g_fakePlatform.callback = callback;
    g_fakePlatform.callbackContext = callbackContext;

    return PLATFORM_ERR_OK;
}

/**
 * @brief 记录 Service 对 Platform UART 异步接收的请求
 * @param[in] uart       : Platform UART 对象
 * @param[out] buffer    : DMA 接收缓冲区
 * @param[in] bufferSize : 接收缓冲区大小
 * @return 预设的测试替身结果
 */
platform_error_t platform_uart_read_async(platform_uart_t *uart,
                                          uint8_t *buffer,
                                          platform_size_t bufferSize)
{
    /**
     * 后续 start 测试复用该替身记录 DMA 缓冲区。
     **/
    (void)uart;
    g_fakePlatform.rxBuffer = buffer;
    g_fakePlatform.rxBufferSize = bufferSize;

    return g_fakePlatform.readAsyncResult;
}

/**
 * @brief 返回预设的 Platform UART 取消结果
 * @param[in] uart      : Platform UART 对象
 * @param[in] direction : 取消方向
 * @return 预设的测试替身结果
 */
platform_error_t platform_uart_cancel(platform_uart_t *uart,
                                      platform_uart_direction_t direction)
{
    /**
     * 后续 stop 测试复用该替身，不在此处模拟回调。
     **/
    (void)uart;
    (void)direction;

    return g_fakePlatform.cancelResult;
}

/**
 * @brief 构造一份有效的 Service 配置
 * @param[in] uart              : Platform UART 对象
 * @param[in] dmaRxBuffer       : DMA 接收缓冲区
 * @param[in] ringBufferStorage : RingBuffer 后备存储
 * @param[in] consumerThread    : Consumer 线程对象
 * @param[out] 无
 * @return 有效的 Service 配置
 */
static service_uart_config_t make_valid_config(platform_uart_t *uart,
                                                uint8_t *dmaRxBuffer,
                                                uint8_t *ringBufferStorage,
                                                platform_thread_t *consumerThread)
{
    service_uart_config_t config = {
        uart,
        dmaRxBuffer,
        8U,
        ringBufferStorage,
        16U,
        consumerThread
    };

    /**
     * 所有指向对象由调用者持有，配置本身可按值复制。
     **/
    return config;
}

/**
 * @brief 验证初始化拒绝缺失或无效的配置字段
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_init_rejects_invalid_parameters(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == service_uart_init(NULL, &config));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == service_uart_init(&service, NULL));

    config.uart = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == service_uart_init(&service, &config));
    config = make_valid_config(&uart, dmaRxBuffer, ringBufferStorage, &consumerThread);
    config.dmaRxBuffer = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == service_uart_init(&service, &config));
    config = make_valid_config(&uart, dmaRxBuffer, ringBufferStorage, &consumerThread);
    config.dmaRxBufferSize = 0U;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == service_uart_init(&service, &config));
    config = make_valid_config(&uart, dmaRxBuffer, ringBufferStorage, &consumerThread);
    config.ringBufferStorage = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == service_uart_init(&service, &config));
    config = make_valid_config(&uart, dmaRxBuffer, ringBufferStorage, &consumerThread);
    config.ringBufferStorageSize = 1U;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == service_uart_init(&service, &config));
    config = make_valid_config(&uart, dmaRxBuffer, ringBufferStorage, &consumerThread);
    config.consumerThread = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == service_uart_init(&service, &config));

    return 0;
}

/**
 * @brief 验证初始化复制配置、构造 RingBuffer 并绑定回调
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_init_constructs_service_and_binds_callback(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(SERVICE_UART_STATE_INITIALIZED == service.context.state);
    TEST_ASSERT(&uart == service.config.uart);
    TEST_ASSERT(dmaRxBuffer == service.config.dmaRxBuffer);
    TEST_ASSERT(sizeof(dmaRxBuffer) == service.config.dmaRxBufferSize);
    TEST_ASSERT(ringBufferStorage == service.config.ringBufferStorage);
    TEST_ASSERT(sizeof(ringBufferStorage) == service.config.ringBufferStorageSize);
    TEST_ASSERT(&consumerThread == service.config.consumerThread);
    TEST_ASSERT(ringBufferStorage == service.context.rxRingBuffer.storage);
    TEST_ASSERT(sizeof(ringBufferStorage) == service.context.rxRingBuffer.storageSize);
    TEST_ASSERT(0U == service.context.rxRingBuffer.readIndex);
    TEST_ASSERT(0U == service.context.rxRingBuffer.writeIndex);
    TEST_ASSERT(1U == g_fakePlatform.setCallbackCallCount);
    TEST_ASSERT(NULL != g_fakePlatform.callback);
    TEST_ASSERT(&service == g_fakePlatform.callbackContext);
    TEST_ASSERT(PLATFORM_ERR_ALREADY_INITIALIZED == service_uart_init(&service, &config));

    return 0;
}

/**
 * @brief 验证回调绑定失败时初始化回滚
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_init_rolls_back_when_callback_binding_fails(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    g_fakePlatform.setCallbackResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == service_uart_init(&service, &config));
    TEST_ASSERT(SERVICE_UART_STATE_UNINITIALIZED == service.context.state);
    TEST_ASSERT(NULL == service.config.uart);
    TEST_ASSERT(NULL == service.context.rxRingBuffer.storage);
    TEST_ASSERT(NULL == g_fakePlatform.callback);
    TEST_ASSERT(NULL == g_fakePlatform.callbackContext);

    return 0;
}

/**
 * @brief 验证 deinit 仅在解绑成功后清理 Service
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_deinit_unbinds_and_preserves_external_storage(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0xA5U};
    uint8_t ringBufferStorage[16] = {0x5AU};
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_deinit(&service));
    TEST_ASSERT(2U == g_fakePlatform.setCallbackCallCount);
    TEST_ASSERT(NULL == g_fakePlatform.callback);
    TEST_ASSERT(NULL == g_fakePlatform.callbackContext);
    TEST_ASSERT(SERVICE_UART_STATE_UNINITIALIZED == service.context.state);
    TEST_ASSERT(NULL == service.config.uart);
    TEST_ASSERT(NULL == service.context.rxRingBuffer.storage);
    TEST_ASSERT(0xA5U == dmaRxBuffer[0]);
    TEST_ASSERT(0x5AU == ringBufferStorage[0]);

    return 0;
}

/**
 * @brief 验证 STOPPED 和 ERROR 可 deinit，运行中状态被拒绝
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_deinit_validates_state_and_preserves_on_unbind_failure(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    service.context.state = SERVICE_UART_STATE_RUNNING;
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE == service_uart_deinit(&service));
    TEST_ASSERT(1U == g_fakePlatform.setCallbackCallCount);
    TEST_ASSERT(&uart == service.config.uart);

    service.context.state = SERVICE_UART_STATE_STOPPING;
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE == service_uart_deinit(&service));
    TEST_ASSERT(1U == g_fakePlatform.setCallbackCallCount);

    service.context.state = SERVICE_UART_STATE_STOPPED;
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_deinit(&service));
    TEST_ASSERT(SERVICE_UART_STATE_UNINITIALIZED == service.context.state);

    fake_service_platform_reset();
    service = (service_uart_t)SERVICE_UART_INITIALIZER;
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    service.context.state = SERVICE_UART_STATE_ERROR;
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_deinit(&service));

    fake_service_platform_reset();
    service = (service_uart_t)SERVICE_UART_INITIALIZER;
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    g_fakePlatform.setCallbackResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == service_uart_deinit(&service));
    TEST_ASSERT(SERVICE_UART_STATE_INITIALIZED == service.context.state);
    TEST_ASSERT(&uart == service.config.uart);
    TEST_ASSERT(ringBufferStorage == service.context.rxRingBuffer.storage);

    return 0;
}

/**
 * @brief 运行 UART Service 对象生命周期测试
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
int main(void)
{
    int result = 0;

    result = test_init_rejects_invalid_parameters();
    if (0 != result) {
        return result;
    }

    result = test_init_constructs_service_and_binds_callback();
    if (0 != result) {
        return result;
    }

    result = test_init_rolls_back_when_callback_binding_fails();
    if (0 != result) {
        return result;
    }

    result = test_deinit_unbinds_and_preserves_external_storage();
    if (0 != result) {
        return result;
    }

    result = test_deinit_validates_state_and_preserves_on_unbind_failure();
    if (0 != result) {
        return result;
    }

    return 0;
}
