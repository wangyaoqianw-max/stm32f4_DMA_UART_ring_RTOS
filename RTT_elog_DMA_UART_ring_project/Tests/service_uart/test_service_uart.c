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

#include <string.h>

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
    platform_error_t notifySetFromIsrResult;
    platform_error_t notifyClearResult;
    platform_error_t notifyWaitResult;
    uint32_t notifySetCount;
    uint32_t notifySetFromIsrCount;
    uint32_t notifyClearCount;
    uint32_t notifyWaitCount;
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
    uint32_t pendingNotifyFlags;
    uint32_t notifyClearFlags;
    uint32_t notifyWaitFlags;
    uint32_t notifyWaitTimeoutMs;
    platform_bool_t notifyWaitAll;
    platform_bool_t notifyWaitClearOnExit;
    platform_bool_t invokeRxDataAfterClear;
    platform_bool_t invokeRxDataBeforeWait;
    platform_uart_t *hookUart;
    const uint8_t *hookData;
    platform_size_t hookDataLength;
} fake_service_platform_t;

static fake_service_platform_t g_fakePlatform;

static void fake_service_platform_invoke_rx_data(platform_uart_t *uart,
                                                 const uint8_t *data,
                                                 platform_size_t dataLength);

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
    g_fakePlatform.notifySetFromIsrResult = PLATFORM_ERR_OK;
    g_fakePlatform.notifyClearResult = PLATFORM_ERR_OK;
    g_fakePlatform.notifyWaitResult = PLATFORM_ERR_TIMEOUT;
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

    if (PLATFORM_ERR_OK == g_fakePlatform.notifySetResult) {
        g_fakePlatform.pendingNotifyFlags |= flags;
    }

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
    g_fakePlatform.notifySetFromIsrCount++;

    if (PLATFORM_ERR_OK == g_fakePlatform.notifySetFromIsrResult) {
        g_fakePlatform.pendingNotifyFlags |= flags;
    }

    return g_fakePlatform.notifySetFromIsrResult;
}

/**
 * @brief 清除并记录 Consumer 当前的私有唤醒位
 * @param[in] flags         : 待清除的通知位
 * @param[out] previousFlags : 清除前的通知位
 * @return 预设的测试替身结果
 */
platform_error_t platform_notify_clear(uint32_t flags, uint32_t *previousFlags)
{
    g_fakePlatform.notifyClearCount++;
    g_fakePlatform.notifyClearFlags = flags;

    if (PLATFORM_ERR_OK != g_fakePlatform.notifyClearResult) {
        return g_fakePlatform.notifyClearResult;
    }

    *previousFlags = g_fakePlatform.pendingNotifyFlags & flags;
    g_fakePlatform.pendingNotifyFlags &= ~flags;

    if (PLATFORM_TRUE == g_fakePlatform.invokeRxDataAfterClear) {
        fake_service_platform_invoke_rx_data(g_fakePlatform.hookUart,
                                             g_fakePlatform.hookData,
                                             g_fakePlatform.hookDataLength);
    }

    return PLATFORM_ERR_OK;
}

/**
 * @brief 等待并记录 Consumer 当前的私有唤醒位
 * @param[in] flags          : 等待的通知位
 * @param[in] waitAll        : 是否等待全部通知位
 * @param[in] clearOnExit    : 返回时是否清除通知位
 * @param[in] timeoutMs      : 等待超时时间
 * @param[out] receivedFlags : 收到的通知位
 * @return 预设的测试替身结果
 */
platform_error_t platform_notify_wait(uint32_t flags,
                                      platform_bool_t waitAll,
                                      platform_bool_t clearOnExit,
                                      uint32_t timeoutMs,
                                      uint32_t *receivedFlags)
{
    uint32_t availableFlags = 0U;

    g_fakePlatform.notifyWaitCount++;
    g_fakePlatform.notifyWaitFlags = flags;
    g_fakePlatform.notifyWaitAll = waitAll;
    g_fakePlatform.notifyWaitClearOnExit = clearOnExit;
    g_fakePlatform.notifyWaitTimeoutMs = timeoutMs;

    if (PLATFORM_TRUE == g_fakePlatform.invokeRxDataBeforeWait) {
        fake_service_platform_invoke_rx_data(g_fakePlatform.hookUart,
                                             g_fakePlatform.hookData,
                                             g_fakePlatform.hookDataLength);
    }

    if (PLATFORM_ERR_TIMEOUT != g_fakePlatform.notifyWaitResult) {
        *receivedFlags = g_fakePlatform.pendingNotifyFlags & flags;
        return g_fakePlatform.notifyWaitResult;
    }

    availableFlags = g_fakePlatform.pendingNotifyFlags & flags;
    if (0U == availableFlags) {
        return PLATFORM_ERR_TIMEOUT;
    }

    *receivedFlags = availableFlags;
    if (PLATFORM_TRUE == clearOnExit) {
        g_fakePlatform.pendingNotifyFlags &= ~availableFlags;
    }

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
 * @brief 向已绑定的 UART Service 回调显式注入 RX_DATA 事件
 * @param[in] uart       : 产生 RX_DATA 的 UART 对象
 * @param[in] data       : 仅在回调期间有效的输入数据
 * @param[in] dataLength : 输入数据长度
 * @param[out] 无
 * @return 无
 */
static void fake_service_platform_invoke_rx_data(platform_uart_t *uart,
                                                 const uint8_t *data,
                                                 platform_size_t dataLength)
{
    platform_uart_event_t event = {0};

    event.type = PLATFORM_UART_EVENT_RX_DATA;
    event.direction = PLATFORM_UART_DIRECTION_RX;
    event.data = data;
    event.dataLength = dataLength;
    event.error = PLATFORM_ERR_OK;
    g_fakePlatform.callback(uart, &event, g_fakePlatform.callbackContext);
}

/**
 * @brief 向已绑定的 UART Service 回调显式注入 ERROR 事件
 * @param[in] uart  : 产生 ERROR 的 UART 对象
 * @param[in] error : Platform UART 错误码
 * @param[out] 无
 * @return 无
 */
static void fake_service_platform_invoke_error(platform_uart_t *uart,
                                               platform_error_t error)
{
    platform_uart_event_t event = {0};

    event.type = PLATFORM_UART_EVENT_ERROR;
    event.direction = PLATFORM_UART_DIRECTION_RX;
    event.error = error;
    g_fakePlatform.callback(uart, &event, g_fakePlatform.callbackContext);
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
 * @brief 验证 RUNNING 状态下 RX_DATA 被立即缓存、统计并唤醒 Consumer
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_rx_data_buffers_bytes_updates_statistics_and_notifies(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint8_t receivedData[] = {0x11U, 0x22U, 0x33U, 0x44U, 0x55U};
    uint8_t readBuffer[sizeof(receivedData)] = {0};
    platform_size_t readLength = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));

    fake_service_platform_invoke_rx_data(&uart,
                                         receivedData,
                                         sizeof(receivedData));

    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_read(&service.context.rxRingBuffer,
                                                     readBuffer,
                                                     sizeof(readBuffer),
                                                     &readLength));
    TEST_ASSERT(sizeof(receivedData) == readLength);
    TEST_ASSERT(0 == memcmp(receivedData, readBuffer, sizeof(receivedData)));
    TEST_ASSERT(1U == service.statistics.rxEventCount);
    TEST_ASSERT(sizeof(receivedData) == service.statistics.rxBytesReceived);
    TEST_ASSERT(sizeof(receivedData) == service.statistics.rxBytesBuffered);
    TEST_ASSERT(0U == service.statistics.rxBytesDropped);
    TEST_ASSERT(service.statistics.rxBytesReceived ==
                service.statistics.rxBytesBuffered + service.statistics.rxBytesDropped);
    TEST_ASSERT(1U == g_fakePlatform.notifySetFromIsrCount);

    return 0;
}

/**
 * @brief 验证 RX_DATA Partial Write 保留前缀并记录数据丢失
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_rx_data_partial_write_tracks_drop_and_notifies(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[5] = {0};
    uint8_t receivedData[] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
    uint8_t readBuffer[4] = {0};
    platform_size_t readLength = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    config.ringBufferStorageSize = sizeof(ringBufferStorage);
    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));

    fake_service_platform_invoke_rx_data(&uart,
                                         receivedData,
                                         sizeof(receivedData));

    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_read(&service.context.rxRingBuffer,
                                                     readBuffer,
                                                     sizeof(readBuffer),
                                                     &readLength));
    TEST_ASSERT(sizeof(readBuffer) == readLength);
    TEST_ASSERT(0 == memcmp(receivedData, readBuffer, sizeof(readBuffer)));
    TEST_ASSERT(1U == service.statistics.rxEventCount);
    TEST_ASSERT(sizeof(receivedData) == service.statistics.rxBytesReceived);
    TEST_ASSERT(sizeof(readBuffer) == service.statistics.rxBytesBuffered);
    TEST_ASSERT((sizeof(receivedData) - sizeof(readBuffer)) ==
                service.statistics.rxBytesDropped);
    TEST_ASSERT(1U == service.statistics.ringBufferOverflowCount);
    TEST_ASSERT(sizeof(readBuffer) == service.statistics.ringBufferHighWaterMark);
    TEST_ASSERT(service.statistics.ringBufferHighWaterMark <=
                (config.ringBufferStorageSize - 1U));
    TEST_ASSERT(PLATFORM_TRUE == service.context.dataLossOccurred);
    TEST_ASSERT(service.statistics.rxBytesReceived ==
                service.statistics.rxBytesBuffered + service.statistics.rxBytesDropped);
    TEST_ASSERT(1U == g_fakePlatform.notifySetFromIsrCount);

    return 0;
}

/**
 * @brief 验证已满 RingBuffer 的 RX_DATA 全量丢弃仍唤醒 Consumer
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_rx_data_full_drop_tracks_loss_and_notifies(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[5] = {0};
    uint8_t firstData[] = {0x10U, 0x11U, 0x12U, 0x13U};
    uint8_t droppedData[] = {0x20U, 0x21U};
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    config.ringBufferStorageSize = sizeof(ringBufferStorage);
    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    fake_service_platform_invoke_rx_data(&uart, firstData, sizeof(firstData));

    fake_service_platform_invoke_rx_data(&uart, droppedData, sizeof(droppedData));

    TEST_ASSERT(2U == service.statistics.rxEventCount);
    TEST_ASSERT((sizeof(firstData) + sizeof(droppedData)) ==
                service.statistics.rxBytesReceived);
    TEST_ASSERT(sizeof(firstData) == service.statistics.rxBytesBuffered);
    TEST_ASSERT(sizeof(droppedData) == service.statistics.rxBytesDropped);
    TEST_ASSERT(1U == service.statistics.ringBufferOverflowCount);
    TEST_ASSERT(sizeof(firstData) == service.statistics.ringBufferHighWaterMark);
    TEST_ASSERT(service.statistics.ringBufferHighWaterMark <=
                (config.ringBufferStorageSize - 1U));
    TEST_ASSERT(PLATFORM_TRUE == service.context.dataLossOccurred);
    TEST_ASSERT(service.statistics.rxBytesReceived ==
                service.statistics.rxBytesBuffered + service.statistics.rxBytesDropped);
    TEST_ASSERT(2U == g_fakePlatform.notifySetFromIsrCount);

    return 0;
}

/**
 * @brief 验证停止后旧数据可读取且新 Session 会丢弃未读 RingBuffer 数据
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_restart_discards_unread_rx_data_and_preserves_statistics(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint8_t receivedData[] = {0x71U, 0x72U, 0x73U, 0x74U, 0x75U};
    uint8_t stoppedReadBuffer[2] = {0};
    platform_size_t readLength = 0U;
    platform_size_t readableSize = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    fake_service_platform_invoke_rx_data(&uart,
                                         receivedData,
                                         sizeof(receivedData));

    g_fakePlatform.emitCanceledOnCancel = PLATFORM_TRUE;
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_stop(&service));
    TEST_ASSERT(SERVICE_UART_STATE_STOPPED == service.context.state);
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_read(&service.context.rxRingBuffer,
                                                     stoppedReadBuffer,
                                                     sizeof(stoppedReadBuffer),
                                                     &readLength));
    TEST_ASSERT(sizeof(stoppedReadBuffer) == readLength);
    TEST_ASSERT(0 == memcmp(receivedData, stoppedReadBuffer, sizeof(stoppedReadBuffer)));

    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    TEST_ASSERT(SERVICE_UART_STATE_RUNNING == service.context.state);
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_get_readable_size(&service.context.rxRingBuffer,
                                                                  &readableSize));
    TEST_ASSERT(0U == readableSize);
    TEST_ASSERT(1U == service.statistics.rxEventCount);
    TEST_ASSERT(sizeof(receivedData) == service.statistics.rxBytesReceived);
    TEST_ASSERT(sizeof(receivedData) == service.statistics.rxBytesBuffered);
    TEST_ASSERT(0U == service.statistics.rxBytesDropped);

    return 0;
}

/**
 * @brief 验证非 RUNNING 状态的 RX_DATA 不修改 RingBuffer、统计或 ISR 唤醒
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_rx_data_is_ignored_outside_running(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint8_t receivedData[] = {0x81U, 0x82U};
    platform_size_t readableSize = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));

    service.context.state = SERVICE_UART_STATE_STOPPING;
    fake_service_platform_invoke_rx_data(&uart, receivedData, sizeof(receivedData));
    service.context.state = SERVICE_UART_STATE_STOPPED;
    fake_service_platform_invoke_rx_data(&uart, receivedData, sizeof(receivedData));
    service.context.state = SERVICE_UART_STATE_ERROR;
    fake_service_platform_invoke_rx_data(&uart, receivedData, sizeof(receivedData));

    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_get_readable_size(&service.context.rxRingBuffer,
                                                                  &readableSize));
    TEST_ASSERT(0U == readableSize);
    TEST_ASSERT(0U == service.statistics.rxEventCount);
    TEST_ASSERT(0U == service.statistics.rxBytesReceived);
    TEST_ASSERT(0U == service.statistics.rxBytesBuffered);
    TEST_ASSERT(0U == service.statistics.rxBytesDropped);
    TEST_ASSERT(0U == g_fakePlatform.notifySetFromIsrCount);

    return 0;
}

/**
 * @brief 验证 RingBuffer High Water Mark 单调保持并跨新 Session 累计
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_rx_data_keeps_high_water_mark_across_reads_and_restart(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint8_t receivedData[] = {0x11U, 0x22U, 0x33U, 0x44U, 0x55U};
    uint8_t readBuffer[3] = {0};
    platform_size_t readLength = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    fake_service_platform_invoke_rx_data(&uart, receivedData, sizeof(receivedData));
    TEST_ASSERT(sizeof(receivedData) == service.statistics.ringBufferHighWaterMark);

    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_read(&service.context.rxRingBuffer,
                                                     readBuffer,
                                                     sizeof(readBuffer),
                                                     &readLength));
    TEST_ASSERT(sizeof(readBuffer) == readLength);
    TEST_ASSERT(sizeof(receivedData) == service.statistics.ringBufferHighWaterMark);

    g_fakePlatform.emitCanceledOnCancel = PLATFORM_TRUE;
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_stop(&service));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    TEST_ASSERT(sizeof(receivedData) == service.statistics.ringBufferHighWaterMark);

    return 0;
}

/**
 * @brief 验证 Service 非阻塞读取的状态、长度与统计语义
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_read_returns_buffered_data_and_tracks_actual_length(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint8_t receivedData[] = {0x91U, 0x92U, 0x93U};
    uint8_t readBuffer[2] = {0};
    platform_size_t readLength = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE ==
                service_uart_read(&service, readBuffer, sizeof(readBuffer), &readLength));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    fake_service_platform_invoke_rx_data(&uart, receivedData, sizeof(receivedData));

    TEST_ASSERT(PLATFORM_ERR_OK ==
                service_uart_read(&service, readBuffer, sizeof(readBuffer), &readLength));
    TEST_ASSERT(sizeof(readBuffer) == readLength);
    TEST_ASSERT(0 == memcmp(receivedData, readBuffer, sizeof(readBuffer)));
    TEST_ASSERT(sizeof(readBuffer) == service.statistics.rxBytesRead);

    service.context.state = SERVICE_UART_STATE_STOPPED;
    TEST_ASSERT(PLATFORM_ERR_OK ==
                service_uart_read(&service, readBuffer, sizeof(readBuffer), &readLength));
    TEST_ASSERT(1U == readLength);
    TEST_ASSERT(receivedData[2] == readBuffer[0]);
    TEST_ASSERT(sizeof(receivedData) == service.statistics.rxBytesRead);
    service.context.state = SERVICE_UART_STATE_ERROR;
    TEST_ASSERT(PLATFORM_ERR_EMPTY ==
                service_uart_read(&service, readBuffer, sizeof(readBuffer), &readLength));
    TEST_ASSERT(0U == readLength);
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_read(&service, NULL, 0U, &readLength));
    TEST_ASSERT(0U == readLength);

    return 0;
}

/**
 * @brief 验证 Service 查询接口返回状态、RingBuffer 真值与逐字段统计快照
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_queries_copy_service_truth_and_reject_uninitialized(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint8_t receivedData[] = {0xA1U, 0xA2U};
    platform_size_t readableSize = 0U;
    service_uart_status_t status = {0};
    service_uart_statistics_t statistics = {0};
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                service_uart_get_readable_size(&service, &readableSize));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == service_uart_get_status(&service, &status));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                service_uart_get_statistics(&service, &statistics));

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    fake_service_platform_invoke_rx_data(&uart, receivedData, sizeof(receivedData));
    service.context.lastError = PLATFORM_ERR_IO;
    service.context.dataLossOccurred = PLATFORM_TRUE;
    service.statistics.rxBytesRead = 3U;
    service.statistics.uartErrorCount = 4U;
    service.statistics.cancelCount = 5U;

    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_get_readable_size(&service, &readableSize));
    TEST_ASSERT(sizeof(receivedData) == readableSize);
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_get_status(&service, &status));
    TEST_ASSERT(SERVICE_UART_STATE_RUNNING == status.state);
    TEST_ASSERT(PLATFORM_ERR_IO == status.lastError);
    TEST_ASSERT(PLATFORM_TRUE == status.dataLossOccurred);
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_get_statistics(&service, &statistics));
    TEST_ASSERT(1U == statistics.rxEventCount);
    TEST_ASSERT(sizeof(receivedData) == statistics.rxBytesReceived);
    TEST_ASSERT(sizeof(receivedData) == statistics.rxBytesBuffered);
    TEST_ASSERT(3U == statistics.rxBytesRead);
    TEST_ASSERT(0U == statistics.rxBytesDropped);
    TEST_ASSERT(0U == statistics.ringBufferOverflowCount);
    TEST_ASSERT(sizeof(receivedData) == statistics.ringBufferHighWaterMark);
    TEST_ASSERT(4U == statistics.uartErrorCount);
    TEST_ASSERT(5U == statistics.cancelCount);

    return 0;
}

/**
 * @brief 验证清除统计的状态限制及其不影响运行时状态和 RingBuffer
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_clear_statistics_preserves_runtime_and_buffer(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint8_t receivedData[] = {0xB1U, 0xB2U};
    platform_size_t readableSize = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    service.statistics.rxEventCount = 1U;
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_clear_statistics(&service));
    TEST_ASSERT(0U == service.statistics.rxEventCount);
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    fake_service_platform_invoke_rx_data(&uart, receivedData, sizeof(receivedData));
    service.context.lastError = PLATFORM_ERR_IO;
    service.context.dataLossOccurred = PLATFORM_TRUE;
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE == service_uart_clear_statistics(&service));
    service.context.state = SERVICE_UART_STATE_STOPPING;
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE == service_uart_clear_statistics(&service));
    service.context.state = SERVICE_UART_STATE_STOPPED;

    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_get_readable_size(&service.context.rxRingBuffer,
                                                                  &readableSize));
    TEST_ASSERT(sizeof(receivedData) == readableSize);
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_clear_statistics(&service));
    TEST_ASSERT(0U == service.statistics.rxEventCount);
    TEST_ASSERT(0U == service.statistics.rxBytesReceived);
    TEST_ASSERT(0U == service.statistics.rxBytesBuffered);
    TEST_ASSERT(0U == service.statistics.rxBytesRead);
    TEST_ASSERT(0U == service.statistics.rxBytesDropped);
    TEST_ASSERT(0U == service.statistics.ringBufferOverflowCount);
    TEST_ASSERT(0U == service.statistics.ringBufferHighWaterMark);
    TEST_ASSERT(0U == service.statistics.uartErrorCount);
    TEST_ASSERT(0U == service.statistics.cancelCount);
    TEST_ASSERT(PLATFORM_ERR_IO == service.context.lastError);
    TEST_ASSERT(PLATFORM_TRUE == service.context.dataLossOccurred);
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_get_readable_size(&service.context.rxRingBuffer,
                                                                  &readableSize));
    TEST_ASSERT(sizeof(receivedData) == readableSize);

    service.context.state = SERVICE_UART_STATE_ERROR;
    service.statistics.rxEventCount = 1U;
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_clear_statistics(&service));
    TEST_ASSERT(0U == service.statistics.rxEventCount);

    return 0;
}

/**
 * @brief 验证 wait_event 直接根据 Service 真值重建事件而不阻塞
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_wait_event_returns_immediate_service_truth(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint8_t receivedData[] = {0xC1U, 0xC2U};
    uint32_t events = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    fake_service_platform_invoke_rx_data(&uart, receivedData, sizeof(receivedData));
    service.context.dataLossOccurred = PLATFORM_TRUE;

    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_wait_event(&service, 37U, &events));
    TEST_ASSERT((SERVICE_UART_EVENT_RX_AVAILABLE | SERVICE_UART_EVENT_DATA_LOSS) == events);
    TEST_ASSERT(1U == g_fakePlatform.notifyClearCount);
    TEST_ASSERT(0U == g_fakePlatform.notifyWaitCount);

    service.context.state = SERVICE_UART_STATE_ERROR;
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_wait_event(&service, 37U, &events));
    TEST_ASSERT((SERVICE_UART_EVENT_RX_AVAILABLE | SERVICE_UART_EVENT_DATA_LOSS |
                 SERVICE_UART_EVENT_ERROR) == events);
    TEST_ASSERT(0U == g_fakePlatform.notifyWaitCount);

    service.context.state = SERVICE_UART_STATE_STOPPED;
    service.context.dataLossOccurred = PLATFORM_FALSE;
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_wait_event(&service, 37U, &events));
    TEST_ASSERT((SERVICE_UART_EVENT_RX_AVAILABLE | SERVICE_UART_EVENT_STOPPED) == events);
    TEST_ASSERT(0U == g_fakePlatform.notifyWaitCount);

    return 0;
}

/**
 * @brief 验证 wait_event 的状态和参数限制
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_wait_event_validates_state_and_events_pointer(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint32_t events = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == service_uart_wait_event(NULL, 1U, &events));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == service_uart_wait_event(&service, 1U, &events));

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == service_uart_wait_event(&service, 1U, NULL));
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE == service_uart_wait_event(&service, 1U, &events));

    service.context.state = SERVICE_UART_STATE_STOPPING;
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE == service_uart_wait_event(&service, 1U, &events));

    return 0;
}

/**
 * @brief 验证 clear 后首次真值检查能够发现新到达的 RX 数据
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_wait_event_detects_rx_arriving_after_clear(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint8_t receivedData[] = {0xD1U};
    uint32_t events = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    g_fakePlatform.invokeRxDataAfterClear = PLATFORM_TRUE;
    g_fakePlatform.hookUart = &uart;
    g_fakePlatform.hookData = receivedData;
    g_fakePlatform.hookDataLength = sizeof(receivedData);

    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_wait_event(&service, 51U, &events));
    TEST_ASSERT(SERVICE_UART_EVENT_RX_AVAILABLE == events);
    TEST_ASSERT(1U == g_fakePlatform.notifyClearCount);
    TEST_ASSERT(0U == g_fakePlatform.notifyWaitCount);

    return 0;
}

/**
 * @brief 验证首次检查与 wait 之间到达的 RX 数据通过私有唤醒位返回
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_wait_event_detects_rx_arriving_before_wait(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint8_t receivedData[] = {0xD2U};
    uint32_t events = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    g_fakePlatform.invokeRxDataBeforeWait = PLATFORM_TRUE;
    g_fakePlatform.hookUart = &uart;
    g_fakePlatform.hookData = receivedData;
    g_fakePlatform.hookDataLength = sizeof(receivedData);

    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_wait_event(&service, 53U, &events));
    TEST_ASSERT(SERVICE_UART_EVENT_RX_AVAILABLE == events);
    TEST_ASSERT(1U == g_fakePlatform.notifyClearCount);
    TEST_ASSERT(1U == g_fakePlatform.notifyWaitCount);
    TEST_ASSERT(1U == g_fakePlatform.notifyWaitFlags);
    TEST_ASSERT(PLATFORM_FALSE == g_fakePlatform.notifyWaitAll);
    TEST_ASSERT(PLATFORM_TRUE == g_fakePlatform.notifyWaitClearOnExit);
    TEST_ASSERT(53U == g_fakePlatform.notifyWaitTimeoutMs);

    return 0;
}

/**
 * @brief 验证 stale wake 不会伪造 RX 事件
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_wait_event_rejects_stale_wake_without_service_truth(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint32_t events = 0xFFFFFFFFU;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    g_fakePlatform.pendingNotifyFlags = 1U;
    g_fakePlatform.notifyWaitResult = PLATFORM_ERR_OK;

    TEST_ASSERT(PLATFORM_ERR_EMPTY == service_uart_wait_event(&service, 59U, &events));
    TEST_ASSERT(0U == events);
    TEST_ASSERT(1U == g_fakePlatform.notifyClearCount);
    TEST_ASSERT(1U == g_fakePlatform.notifyWaitCount);

    return 0;
}

/**
 * @brief 验证 wait_event 原样返回 Notify 清除和等待失败
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_wait_event_propagates_notify_errors(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint32_t events = 0xFFFFFFFFU;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    g_fakePlatform.notifyClearResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == service_uart_wait_event(&service, 61U, &events));
    TEST_ASSERT(0U == events);
    TEST_ASSERT(0U == g_fakePlatform.notifyWaitCount);

    g_fakePlatform.notifyClearResult = PLATFORM_ERR_OK;
    g_fakePlatform.notifyWaitResult = PLATFORM_ERR_IO;
    events = 0xFFFFFFFFU;
    TEST_ASSERT(PLATFORM_ERR_IO == service_uart_wait_event(&service, 61U, &events));
    TEST_ASSERT(0U == events);

    return 0;
}

/**
 * @brief 验证 RUNNING 状态的 ERROR callback 终止 Session 并保留已缓存数据
 * @param[in] 无
 * @param[out] 无
 * @return 成功返回 0，失败返回断言行号
 */
static int test_error_callback_sets_error_and_preserves_buffered_data(void)
{
    service_uart_t service = SERVICE_UART_INITIALIZER;
    platform_uart_t uart = {0};
    platform_thread_t consumerThread = PLATFORM_OS_OBJECT_INITIALIZER;
    uint8_t dmaRxBuffer[8] = {0};
    uint8_t ringBufferStorage[16] = {0};
    uint8_t receivedData[] = {0xE1U, 0xE2U};
    uint8_t readBuffer[2] = {0};
    platform_size_t readLength = 0U;
    platform_size_t readableSize = 0U;
    service_uart_config_t config = make_valid_config(&uart,
                                                      dmaRxBuffer,
                                                      ringBufferStorage,
                                                      &consumerThread);

    fake_service_platform_reset();
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_init(&service, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_start(&service));
    fake_service_platform_invoke_rx_data(&uart, receivedData, sizeof(receivedData));
    fake_service_platform_invoke_error(&uart, PLATFORM_ERR_IO);

    TEST_ASSERT(SERVICE_UART_STATE_ERROR == service.context.state);
    TEST_ASSERT(PLATFORM_ERR_IO == service.context.lastError);
    TEST_ASSERT(1U == service.statistics.uartErrorCount);
    TEST_ASSERT(2U == g_fakePlatform.notifySetFromIsrCount);
    TEST_ASSERT(0U == g_fakePlatform.cancelCallCount);
    TEST_ASSERT(1U == g_fakePlatform.readAsyncCallCount);
    TEST_ASSERT(PLATFORM_ERR_OK == service_uart_get_readable_size(&service, &readableSize));
    TEST_ASSERT(sizeof(receivedData) == readableSize);
    TEST_ASSERT(PLATFORM_ERR_OK ==
                service_uart_read(&service, readBuffer, sizeof(readBuffer), &readLength));
    TEST_ASSERT(sizeof(readBuffer) == readLength);
    TEST_ASSERT(0 == memcmp(receivedData, readBuffer, sizeof(readBuffer)));

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

    result = test_rx_data_buffers_bytes_updates_statistics_and_notifies();
    if (0 != result) {
        return result;
    }

    result = test_rx_data_partial_write_tracks_drop_and_notifies();
    if (0 != result) {
        return result;
    }

    result = test_rx_data_full_drop_tracks_loss_and_notifies();
    if (0 != result) {
        return result;
    }

    result = test_rx_data_keeps_high_water_mark_across_reads_and_restart();
    if (0 != result) {
        return result;
    }

    result = test_restart_discards_unread_rx_data_and_preserves_statistics();
    if (0 != result) {
        return result;
    }

    result = test_rx_data_is_ignored_outside_running();
    if (0 != result) {
        return result;
    }

    result = test_read_returns_buffered_data_and_tracks_actual_length();
    if (0 != result) {
        return result;
    }

    result = test_queries_copy_service_truth_and_reject_uninitialized();
    if (0 != result) {
        return result;
    }

    result = test_clear_statistics_preserves_runtime_and_buffer();
    if (0 != result) {
        return result;
    }

    result = test_wait_event_returns_immediate_service_truth();
    if (0 != result) {
        return result;
    }

    result = test_wait_event_validates_state_and_events_pointer();
    if (0 != result) {
        return result;
    }

    result = test_wait_event_detects_rx_arriving_after_clear();
    if (0 != result) {
        return result;
    }

    result = test_wait_event_detects_rx_arriving_before_wait();
    if (0 != result) {
        return result;
    }

    result = test_wait_event_rejects_stale_wake_without_service_truth();
    if (0 != result) {
        return result;
    }

    result = test_wait_event_propagates_notify_errors();
    if (0 != result) {
        return result;
    }

    result = test_error_callback_sets_error_and_preserves_buffered_data();
    if (0 != result) {
        return result;
    }

    return 0;
}
