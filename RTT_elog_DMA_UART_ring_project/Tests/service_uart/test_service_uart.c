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
    platform_error_t notifySetResult;
    uint32_t notifySetCount;
    uint32_t notifySetFromIsrCount;
    uint32_t setCallbackCallCount;
    uint32_t readAsyncCallCount;
    uint32_t cancelCallCount;
    uint32_t canceledCallbackCount;
    uint32_t notifySetCountAtCancelReturn;
    platform_uart_direction_t cancelDirection;
    service_uart_state_t cancelStateAtCall;
    platform_bool_t emitCanceledOnCancel;
    platform_thread_t *notifySetThread;
    uint32_t notifySetFlags;
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
    g_fakePlatform.notifySetResult = PLATFORM_ERR_OK;
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
    g_fakePlatform.readAsyncCallCount++;
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
    platform_uart_event_t event = {0};

    /**
     * STM32 Impl 的取消路径会在返回前同步产生 CANCELED；测试替身按需镜像该时序。
     **/
    g_fakePlatform.cancelCallCount++;
    g_fakePlatform.cancelDirection = direction;
    if (NULL != g_fakePlatform.callbackContext) {
        g_fakePlatform.cancelStateAtCall =
            ((service_uart_t *)g_fakePlatform.callbackContext)->context.state;
    }

    if ((PLATFORM_ERR_OK == g_fakePlatform.cancelResult) &&
        (PLATFORM_TRUE == g_fakePlatform.emitCanceledOnCancel) &&
        (NULL != g_fakePlatform.callback)) {
        event.type = PLATFORM_UART_EVENT_CANCELED;
        event.direction = direction;
        event.error = PLATFORM_ERR_OK;
        g_fakePlatform.callback(uart, &event, g_fakePlatform.callbackContext);
        g_fakePlatform.canceledCallbackCount++;
    }

    g_fakePlatform.notifySetCountAtCancelReturn = g_fakePlatform.notifySetCount;

    return g_fakePlatform.cancelResult;
}

/**
 * @brief 记录 Task Context 对 Consumer Thread 的唤醒请求
 * @param[in] thread : 目标线程
 * @param[in] flags  : 通知位
 * @param[out] 无
 * @return 预设的测试替身结果
 */
platform_error_t platform_notify_set(platform_thread_t *thread, uint32_t flags)
{
    g_fakePlatform.notifySetCount++;
    g_fakePlatform.notifySetThread = thread;
    g_fakePlatform.notifySetFlags = flags;

    return g_fakePlatform.notifySetResult;
}

/**
 * @brief 记录 ISR Context 对 Consumer Thread 的唤醒请求
 * @param[in] thread : 目标线程
 * @param[in] flags  : 通知位
 * @param[out] 无
 * @return PLATFORM_ERR_OK
 */
platform_error_t platform_notify_set_from_isr(platform_thread_t *thread, uint32_t flags)
{
    (void)thread;
    (void)flags;
    g_fakePlatform.notifySetFromIsrCount++;

    return PLATFORM_ERR_OK;
}

/**
 * @brief 向已绑定的 UART Service 回调显式注入 CANCELED 事件
 * @param[in] uart : 产生 CANCELED 的 UART 对象
 * @param[out] 无
 * @return 无
 */
static void fake_service_platform_invoke_canceled(platform_uart_t *uart)
{
    platform_uart_event_t event = {0};

    event.type = PLATFORM_UART_EVENT_CANCELED;
    event.direction = PLATFORM_UART_DIRECTION_RX;
    event.error = PLATFORM_ERR_OK;
    g_fakePlatform.callback(uart, &event, g_fakePlatform.callbackContext);
    g_fakePlatform.canceledCallbackCount++;
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
 * @brief 验证 start 建立新 Session 并保留累计统计
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_start_begins_new_session_and_preserves_statistics(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint8_t oldData[] = {0x31U, 0x32U, 0x33U};
    platform_size_t writtenLength = 0U;
    platform_size_t readableSize = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_write(&service.context.rxRingBuffer,
                                                      oldData,
                                                      sizeof(oldData),
                                                      &writtenLength));
    TEST_ASSERT(sizeof(oldData) == writtenLength);
    service.context.dataLossOccurred = PLATFORM_TRUE;
    service.context.lastError = PLATFORM_ERR_IO;
    service.statistics.rxEventCount = 7U;
    service.statistics.rxBytesReceived = 11U;

    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    TEST_ASSERT(SERVICE_UART_STATE_RUNNING == service.context.state);
    TEST_ASSERT(1U == g_fakePlatform.readAsyncCallCount);
    TEST_ASSERT(dmaRxBuffer == g_fakePlatform.rxBuffer);
    TEST_ASSERT(sizeof(dmaRxBuffer) == g_fakePlatform.rxBufferSize);
    TEST_ASSERT(PLATFORM_FALSE == service.context.dataLossOccurred);
    TEST_ASSERT(PLATFORM_ERR_OK == service.context.lastError);
    TEST_ASSERT(7U == service.statistics.rxEventCount);
    TEST_ASSERT(11U == service.statistics.rxBytesReceived);
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_get_readable_size(
                                      &service.context.rxRingBuffer,
                                      &readableSize));
    TEST_ASSERT(0U == readableSize);

    return 0;
}

/**
 * @brief 验证 start 的状态限制以及 STOPPED 和 ERROR 重启路径
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_start_validates_state_and_allows_safe_restart_states(void)
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

    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == service_uart_start(&service));

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    service.context.state = SERVICE_UART_STATE_RUNNING;
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE == service_uart_start(&service));
    service.context.state = SERVICE_UART_STATE_STOPPING;
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE == service_uart_start(&service));

    service.context.state = SERVICE_UART_STATE_STOPPED;
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    TEST_ASSERT(SERVICE_UART_STATE_RUNNING == service.context.state);

    service.context.state = SERVICE_UART_STATE_ERROR;
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    TEST_ASSERT(SERVICE_UART_STATE_RUNNING == service.context.state);
    TEST_ASSERT(2U == g_fakePlatform.readAsyncCallCount);

    return 0;
}

/**
 * @brief 验证异步接收启动失败时恢复调用前安全状态
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_start_restores_safe_state_when_read_async_fails(void)
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
    g_fakePlatform.readAsyncResult = PLATFORM_ERR_IO;

    TEST_ASSERT(PLATFORM_ERR_IO == service_uart_start(&service));
    TEST_ASSERT(SERVICE_UART_STATE_INITIALIZED == service.context.state);
    TEST_ASSERT(1U == g_fakePlatform.readAsyncCallCount);
    TEST_ASSERT(dmaRxBuffer == g_fakePlatform.rxBuffer);
    TEST_ASSERT(sizeof(dmaRxBuffer) == g_fakePlatform.rxBufferSize);

    return 0;
}

/**
 * @brief 验证 stop 在同步 CANCELED 后于 Task Context 唤醒 Consumer
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_stop_cancels_session_and_notifies_after_callback(void)
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
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    g_fakePlatform.emitCanceledOnCancel = PLATFORM_TRUE;

    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_stop(&service));
    TEST_ASSERT(1U == g_fakePlatform.cancelCallCount);
    TEST_ASSERT(PLATFORM_UART_DIRECTION_RX == g_fakePlatform.cancelDirection);
    TEST_ASSERT(SERVICE_UART_STATE_STOPPING == g_fakePlatform.cancelStateAtCall);
    TEST_ASSERT(1U == g_fakePlatform.canceledCallbackCount);
    TEST_ASSERT(0U == g_fakePlatform.notifySetCountAtCancelReturn);
    TEST_ASSERT(SERVICE_UART_STATE_STOPPED == service.context.state);
    TEST_ASSERT(1U == service.statistics.cancelCount);
    TEST_ASSERT(1U == g_fakePlatform.notifySetCount);
    TEST_ASSERT(0U == g_fakePlatform.notifySetFromIsrCount);
    TEST_ASSERT(&consumerThread == g_fakePlatform.notifySetThread);

    return 0;
}

/**
 * @brief 验证 stop 取消失败时回滚为 RUNNING，其他状态禁止 stop
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_stop_restores_running_state_when_cancel_fails(void)
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

    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == service_uart_stop(&service));

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE == service_uart_stop(&service));
    service.context.state = SERVICE_UART_STATE_STOPPED;
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE == service_uart_stop(&service));
    service.context.state = SERVICE_UART_STATE_ERROR;
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE == service_uart_stop(&service));

    service.context.state = SERVICE_UART_STATE_RUNNING;
    g_fakePlatform.cancelResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == service_uart_stop(&service));
    TEST_ASSERT(SERVICE_UART_STATE_RUNNING == service.context.state);
    TEST_ASSERT(1U == g_fakePlatform.cancelCallCount);
    TEST_ASSERT(0U == g_fakePlatform.notifySetCount);

    return 0;
}

/**
 * @brief 验证 stop 在停止后通知失败时返回错误但仍保持 STOPPED
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_stop_reports_notification_failure_after_stopping(void)
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
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    g_fakePlatform.emitCanceledOnCancel = PLATFORM_TRUE;
    g_fakePlatform.notifySetResult = PLATFORM_ERR_IO;

    TEST_ASSERT(PLATFORM_ERR_IO == service_uart_stop(&service));
    TEST_ASSERT(1U == g_fakePlatform.cancelCallCount);
    TEST_ASSERT(1U == g_fakePlatform.notifySetCount);
    TEST_ASSERT(SERVICE_UART_STATE_STOPPED == service.context.state);

    return 0;
}

/**
 * @brief 验证非预期 CANCELED 停止 Session 且不直接通知 Task
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_unexpected_canceled_stops_without_notification(void)
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
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));

    fake_service_platform_invoke_canceled(&uart);

    TEST_ASSERT(SERVICE_UART_STATE_STOPPED == service.context.state);
    TEST_ASSERT(1U == service.statistics.cancelCount);
    TEST_ASSERT(0U == g_fakePlatform.notifySetCount);
    TEST_ASSERT(0U == g_fakePlatform.notifySetFromIsrCount);

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

    result = test_start_begins_new_session_and_preserves_statistics();
    if (0 != result) {
        return result;
    }

    result = test_start_validates_state_and_allows_safe_restart_states();
    if (0 != result) {
        return result;
    }

    result = test_start_restores_safe_state_when_read_async_fails();
    if (0 != result) {
        return result;
    }

    result = test_stop_cancels_session_and_notifies_after_callback();
    if (0 != result) {
        return result;
    }

    result = test_stop_restores_running_state_when_cancel_fails();
    if (0 != result) {
        return result;
    }

    result = test_stop_reports_notification_failure_after_stopping();
    if (0 != result) {
        return result;
    }

    result = test_unexpected_canceled_stops_without_notification();
    if (0 != result) {
        return result;
    }

    return 0;
}
