/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_uart.h
 * @brief UART 接收 Service 公共接口
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#ifndef SERVICE_UART_H
#define SERVICE_UART_H

//******************************** Includes *********************************//
#include "ring_buffer.h"
#include "platform_os.h"
#include "platform_uart.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define SERVICE_UART_INITIALIZER             {0}
#define SERVICE_UART_EVENT_RX_AVAILABLE      (1U << 0)
#define SERVICE_UART_EVENT_DATA_LOSS         (1U << 1)
#define SERVICE_UART_EVENT_ERROR             (1U << 2)
#define SERVICE_UART_EVENT_STOPPED           (1U << 3)
//******************************** Defines *********************************//

//******************************** Types ***********************************//
typedef enum
{
    SERVICE_UART_STATE_UNINITIALIZED = 0,
    SERVICE_UART_STATE_INITIALIZED,
    SERVICE_UART_STATE_RUNNING,
    SERVICE_UART_STATE_STOPPING,
    SERVICE_UART_STATE_STOPPED,
    SERVICE_UART_STATE_ERROR,
    SERVICE_UART_STATE_MAX
} service_uart_state_t;

typedef struct
{
    platform_uart_t *uart;
    uint8_t *dmaRxBuffer;
    platform_size_t dmaRxBufferSize;
    uint8_t *ringBufferStorage;
    platform_size_t ringBufferStorageSize;
    platform_thread_t *consumerThread;
} service_uart_config_t;

typedef struct
{
    volatile service_uart_state_t state;
    ring_buffer_t rxRingBuffer;
    volatile platform_error_t lastError;
    volatile platform_bool_t dataLossOccurred;
} service_uart_context_t;

typedef struct
{
    volatile uint32_t rxEventCount;
    volatile uint32_t rxBytesReceived;
    volatile uint32_t rxBytesBuffered;
    volatile uint32_t rxBytesRead;
    volatile uint32_t rxBytesDropped;
    volatile uint32_t ringBufferOverflowCount;
    volatile platform_size_t ringBufferHighWaterMark;
    volatile uint32_t uartErrorCount;
    volatile uint32_t cancelCount;
} service_uart_statistics_t;

typedef struct
{
    service_uart_state_t state;
    platform_error_t lastError;
    platform_bool_t dataLossOccurred;
} service_uart_status_t;

typedef struct
{
    service_uart_config_t config;
    service_uart_context_t context;
    service_uart_statistics_t statistics;
} service_uart_t;
//******************************** Types ***********************************//

//******************************** Declaring *******************************//
/**
 * @brief 初始化 UART Service 并绑定 Platform UART 异步回调
 * @param[in,out] service : 使用 SERVICE_UART_INITIALIZER 初始化的 Service 对象
 * @param[in] config      : 调用者持有的 UART、DMA、RingBuffer 与 Consumer Task 配置
 * @return PLATFORM_ERR_OK 成功；其他值表示参数、状态或回调绑定失败
 * @note 本函数不初始化或启动 UART 硬件，不接管任何外部存储的所有权。
 */
platform_error_t service_uart_init(service_uart_t *service,
                                   const service_uart_config_t *config);
/**
 * @brief 开启一个新的 UART RX Session
 * @param[in,out] service : 已初始化的 UART Service 对象
 * @return PLATFORM_ERR_OK 成功；其他值表示状态或 Platform UART 异步接收启动失败
 * @note 成功启动会丢弃 RingBuffer 中旧 Session 的未读数据，但不清除累计统计。
 */
platform_error_t service_uart_start(service_uart_t *service);
/**
 * @brief 取消当前 UART RX Session 并停止 Service 接收
 * @param[in,out] service : 正在运行的 UART Service 对象
 * @return PLATFORM_ERR_OK 成功；其他值表示状态或 Platform UART 取消失败
 * @note 仅 Service 可以取消其拥有的活动 RX Session。
 */
platform_error_t service_uart_stop(service_uart_t *service);
/**
 * @brief 解绑 Platform UART 回调并清理 UART Service 对象
 * @param[in,out] service : 已初始化且不存在活动 RX Session 的 Service 对象
 * @return PLATFORM_ERR_OK 成功；其他值表示状态或回调解绑失败
 * @note 不停止 UART 硬件、不销毁 Consumer Task，也不释放或清空调用者持有的存储。
 */
platform_error_t service_uart_deinit(service_uart_t *service);
/**
 * @brief 非阻塞读取当前 RingBuffer 中已缓存的 RX 数据
 * @param[in,out] service    : 已初始化的 UART Service 对象
 * @param[out] buffer        : 调用者提供的输出缓冲区
 * @param[in] bufferSize     : 输出缓冲区容量
 * @param[out] readLength    : 实际读取的字节数
 * @return PLATFORM_ERR_OK 成功；其他值表示状态、参数或 RingBuffer 读取结果
 * @note 仅专用 Consumer Task 可以调用本接口。
 */
platform_error_t service_uart_read(service_uart_t *service,
                                   uint8_t *buffer,
                                   platform_size_t bufferSize,
                                   platform_size_t *readLength);
/**
 * @brief 等待 UART Service 的事件唤醒
 * @param[in,out] service   : 已初始化的 UART Service 对象
 * @param[in] timeoutMs     : 等待超时时间，单位为毫秒
 * @param[out] events       : 返回收到的 Service 事件位
 * @return PLATFORM_ERR_OK 成功；其他值表示状态、参数、超时或 Platform Notify 失败
 * @note 通知仅是唤醒提示，数据可读性和运行状态仍以 Service 查询接口为准。
 */
platform_error_t service_uart_wait_event(service_uart_t *service,
                                         uint32_t timeoutMs,
                                         uint32_t *events);
/**
 * @brief 查询当前 RingBuffer 中的可读字节数
 * @param[in] service          : 已初始化的 UART Service 对象
 * @param[out] readableSize    : 当前可读取的字节数
 * @return PLATFORM_ERR_OK 成功；其他值表示状态、参数或 RingBuffer 查询失败
 */
platform_error_t service_uart_get_readable_size(const service_uart_t *service,
                                                platform_size_t *readableSize);
/**
 * @brief 获取 UART Service 当前运行状态快照
 * @param[in] service : 已初始化的 UART Service 对象
 * @param[out] status : 输出状态快照
 * @return PLATFORM_ERR_OK 成功；其他值表示状态或参数错误
 */
platform_error_t service_uart_get_status(const service_uart_t *service,
                                         service_uart_status_t *status);
/**
 * @brief 获取 UART Service 累计统计快照
 * @param[in] service       : 已初始化的 UART Service 对象
 * @param[out] statistics   : 输出统计快照
 * @return PLATFORM_ERR_OK 成功；其他值表示状态或参数错误
 * @note 统计为无锁 best-effort 快照，不保证多个字段具有同一时刻的一致性。
 */
platform_error_t service_uart_get_statistics(
    const service_uart_t *service,
    service_uart_statistics_t *statistics);
/**
 * @brief 清除 UART Service 的累计统计
 * @param[in,out] service : 已初始化且 Consumer 静止的 UART Service 对象
 * @return PLATFORM_ERR_OK 成功；其他值表示状态错误
 * @note 本函数不清除当前 Session 的 dataLossOccurred 或 lastError。
 */
platform_error_t service_uart_clear_statistics(service_uart_t *service);
//******************************** Declaring *******************************//

#endif
