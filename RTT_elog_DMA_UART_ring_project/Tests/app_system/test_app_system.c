/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_app_system.c
 * @brief 验证 Phase 9 Composition Root 的依赖顺序、资源和回滚。
 * @author YaoQian Wang
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#include "app_acquisition.h"
#include "app_communication.h"
#include "app_control.h"
#include "app_display.h"
#include "app_indicator.h"
#include "app_system.h"
#include "button/platform_bsp_button.h"
#include "led/platform_bsp_led.h"
#include "platform_bsp_spi.h"
#include "platform_bsp_st7789.h"
#include "platform_bsp_uart.h"
#include "platform_queue.h"
#include "platform_spi.h"
#include "platform_thread.h"
#include "service_log.h"

#include <stdarg.h>
#include <string.h>

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            return __LINE__; \
        } \
    } while (0)

/** @brief 记录 Composition Root 的创建顺序、资源参数与回滚行为。 */
typedef struct
{
    uint32_t sequence;
    uint32_t lastHardwareSequence;
    uint32_t firstServiceSequence;
    uint32_t lastServiceSequence;
    uint32_t firstQueueSequence;
    uint32_t lastQueueSequence;
    uint32_t firstAppSequence;
    uint32_t lastAppSequence;
    uint32_t firstThreadSequence;
    uint32_t constructCount;
    uint32_t hardwareInitCount;
    uint32_t serviceInitCount;
    uint32_t queueCreateCount;
    uint32_t appInitCount;
    uint32_t threadCreateCount;
    uint32_t threadTerminateCount;
    uint32_t queueDeleteCount;
    uint32_t serviceDeinitCount;
    uint32_t hardwareDeinitCount;
    uint32_t spiLifecycleInitCount;
    uint32_t spiLifecycleStartCount;
    uint32_t spiLifecycleStopCount;
    uint32_t spiLifecycleDeinitCount;
    uint32_t st7789RuntimeInitCount;
    uint32_t rollbackSequence;
    uint32_t lastHardwareDeinitSequence;
    uint32_t spiStopSequence;
    uint32_t spiDeinitSequence;
    uint32_t failThreadCreateCall;
    platform_bool_t failSpiStart;
    platform_size_t queueDepths[5];
    platform_size_t queueItemSizes[5];
    const char *threadNames[5];
    uint32_t threadStacks[5];
    platform_thread_priority_t threadPriorities[5];
    platform_thread_t *createdThreads[5];
    uint32_t terminatedThreadIndices[5];
    platform_queue_t *createdQueues[5];
    uint32_t deletedQueueIndices[5];
    app_communication_config_t communicationConfig;
    platform_queue_t *controlQueue;
    platform_queue_t *displayQueue;
    app_control_message_t submittedControlMessage;
    uint32_t controlSubmitCount;
} fake_system_runtime_t;

static fake_system_runtime_t g_fakeRuntime;

/** @brief 清空系统装配测试的全部观测记录。 */
static void fake_runtime_reset(void)
{
    memset(&g_fakeRuntime, 0, sizeof(g_fakeRuntime));
}

/** @brief 记录一次硬件对象初始化及其顺序。 */
static void fake_record_hardware(void)
{
    g_fakeRuntime.hardwareInitCount++;
    g_fakeRuntime.lastHardwareSequence = ++g_fakeRuntime.sequence;
}

/** @brief 记录一次 Service 初始化及其顺序范围。 */
static void fake_record_service(void)
{
    uint32_t sequence = ++g_fakeRuntime.sequence;

    if (g_fakeRuntime.firstServiceSequence == 0U) {
        g_fakeRuntime.firstServiceSequence = sequence;
    }
    g_fakeRuntime.lastServiceSequence = sequence;
    g_fakeRuntime.serviceInitCount++;
}

/** @brief 记录一次 APP 初始化及其顺序范围。 */
static void fake_record_app(void)
{
    uint32_t sequence = ++g_fakeRuntime.sequence;

    if (g_fakeRuntime.firstAppSequence == 0U) {
        g_fakeRuntime.firstAppSequence = sequence;
    }
    g_fakeRuntime.lastAppSequence = sequence;
    g_fakeRuntime.appInitCount++;
}

/** @brief 验证 SPI start 失败会反初始化已初始化 Bus 并保持可重试。 */
static int test_spi_start_failure_rolls_back_and_allows_retry(void)
{
    fake_runtime_reset();
    g_fakeRuntime.failSpiStart = PLATFORM_TRUE;

    TEST_ASSERT(app_system_init() == PLATFORM_ERR_NO_RESOURCE);
    TEST_ASSERT(g_fakeRuntime.spiLifecycleInitCount == 1U);
    TEST_ASSERT(g_fakeRuntime.spiLifecycleStartCount == 1U);
    TEST_ASSERT(g_fakeRuntime.spiLifecycleStopCount == 0U);
    TEST_ASSERT(g_fakeRuntime.spiLifecycleDeinitCount == 1U);
    TEST_ASSERT(g_fakeRuntime.queueCreateCount == 0U);
    TEST_ASSERT(g_fakeRuntime.threadCreateCount == 0U);

    return 0;
}

/** @brief 验证线程创建失败会完整回滚并允许重试。 */
static int test_thread_failure_rolls_back_and_allows_retry(void)
{
    fake_runtime_reset();
    g_fakeRuntime.failThreadCreateCall = 5U;

    TEST_ASSERT(app_system_init() == PLATFORM_ERR_NO_RESOURCE);
    TEST_ASSERT(g_fakeRuntime.threadTerminateCount == 4U);
    TEST_ASSERT(g_fakeRuntime.queueDeleteCount == 5U);
    TEST_ASSERT(g_fakeRuntime.serviceDeinitCount == 4U);
    TEST_ASSERT(g_fakeRuntime.hardwareDeinitCount == 5U);
    TEST_ASSERT(g_fakeRuntime.spiLifecycleStopCount == 1U);
    TEST_ASSERT(g_fakeRuntime.spiLifecycleDeinitCount == 1U);
    TEST_ASSERT(g_fakeRuntime.terminatedThreadIndices[0] == 3U);
    TEST_ASSERT(g_fakeRuntime.terminatedThreadIndices[1] == 2U);
    TEST_ASSERT(g_fakeRuntime.terminatedThreadIndices[2] == 1U);
    TEST_ASSERT(g_fakeRuntime.terminatedThreadIndices[3] == 0U);
    TEST_ASSERT(g_fakeRuntime.deletedQueueIndices[0] == 4U);
    TEST_ASSERT(g_fakeRuntime.deletedQueueIndices[1] == 3U);
    TEST_ASSERT(g_fakeRuntime.deletedQueueIndices[2] == 2U);
    TEST_ASSERT(g_fakeRuntime.deletedQueueIndices[3] == 1U);
    TEST_ASSERT(g_fakeRuntime.deletedQueueIndices[4] == 0U);
    TEST_ASSERT(g_fakeRuntime.lastHardwareDeinitSequence <
                g_fakeRuntime.spiStopSequence);
    TEST_ASSERT(g_fakeRuntime.spiStopSequence < g_fakeRuntime.spiDeinitSequence);

    fake_runtime_reset();
    TEST_ASSERT(app_system_init() == PLATFORM_ERR_OK);

    return 0;
}

/** @brief 验证最终装配顺序、Queue 合同和五任务参数。 */
static int test_final_composition_order_and_resources(void)
{
    static const char *expectedNames[] = {
        "communication",
        "control",
        "acquisition",
        "indicator",
        "display"
    };
    static const uint32_t expectedStacks[] = {2048U, 1024U, 1536U, 768U, 1536U};
    static const platform_thread_priority_t expectedPriorities[] = {
        PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL,
        PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL,
        PLATFORM_THREAD_PRIORITY_NORMAL,
        PLATFORM_THREAD_PRIORITY_BELOW_NORMAL,
        PLATFORM_THREAD_PRIORITY_NORMAL
    };
    static const platform_size_t expectedDepths[] = {8U, 4U, 8U, 4U, 4U};
    static const platform_size_t expectedItemSizes[] = {
        sizeof(app_control_message_t),
        sizeof(app_acquisition_command_t),
        sizeof(app_control_response_t),
        sizeof(app_indicator_command_t),
        sizeof(app_display_message_t)
    };
    uint32_t index;

    TEST_ASSERT(g_fakeRuntime.constructCount == 7U);
    TEST_ASSERT(g_fakeRuntime.hardwareInitCount == 7U);
    TEST_ASSERT(g_fakeRuntime.spiLifecycleInitCount == 1U);
    TEST_ASSERT(g_fakeRuntime.spiLifecycleStartCount == 1U);
    TEST_ASSERT(g_fakeRuntime.st7789RuntimeInitCount == 0U);
    TEST_ASSERT(g_fakeRuntime.serviceInitCount == 4U);
    TEST_ASSERT(g_fakeRuntime.queueCreateCount == 5U);
    TEST_ASSERT(g_fakeRuntime.appInitCount == 5U);
    TEST_ASSERT(g_fakeRuntime.threadCreateCount == 5U);
    TEST_ASSERT(g_fakeRuntime.lastHardwareSequence < g_fakeRuntime.firstServiceSequence);
    TEST_ASSERT(g_fakeRuntime.lastServiceSequence < g_fakeRuntime.firstQueueSequence);
    TEST_ASSERT(g_fakeRuntime.lastQueueSequence < g_fakeRuntime.firstAppSequence);
    TEST_ASSERT(g_fakeRuntime.lastAppSequence < g_fakeRuntime.firstThreadSequence);

    for (index = 0U; index < 5U; index++) {
        TEST_ASSERT(g_fakeRuntime.queueDepths[index] == expectedDepths[index]);
        TEST_ASSERT(g_fakeRuntime.queueItemSizes[index] == expectedItemSizes[index]);
        TEST_ASSERT(strcmp(g_fakeRuntime.threadNames[index], expectedNames[index]) == 0);
        TEST_ASSERT(g_fakeRuntime.threadStacks[index] == expectedStacks[index]);
        TEST_ASSERT(g_fakeRuntime.threadPriorities[index] == expectedPriorities[index]);
    }

    TEST_ASSERT(g_fakeRuntime.communicationConfig.controlHandler != NULL);
    TEST_ASSERT(g_fakeRuntime.communicationConfig.outboundQueue != NULL);
    TEST_ASSERT(g_fakeRuntime.communicationConfig.controlHandler(
                    g_fakeRuntime.communicationConfig.controlContext,
                    APP_CTRL_START) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.controlSubmitCount == 1U);
    TEST_ASSERT(g_fakeRuntime.submittedControlMessage.type ==
                APP_CONTROL_MESSAGE_CONTROL_REQUEST);
    TEST_ASSERT(g_fakeRuntime.submittedControlMessage.payload.request.event == APP_CTRL_START);
    TEST_ASSERT(g_fakeRuntime.submittedControlMessage.payload.request.source ==
                APP_CTRL_SOURCE_UART);

    return 0;
}

platform_error_t platform_bsp_uart_construct_communication(
    platform_uart_t *uart,
    const platform_uart_config_t *config)
{
    (void)uart;
    TEST_ASSERT(config->baudRate == 115200U);
    g_fakeRuntime.constructCount++;
    ++g_fakeRuntime.sequence;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_bsp_led_construct_status_led(platform_led_t *led)
{
    (void)led;
    g_fakeRuntime.constructCount++;
    ++g_fakeRuntime.sequence;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_bsp_button_construct_user_key(platform_button_t *button)
{
    (void)button;
    g_fakeRuntime.constructCount++;
    ++g_fakeRuntime.sequence;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_bsp_gpio_construct_soft_i2c_scl(platform_gpio_t *gpio)
{
    (void)gpio;
    g_fakeRuntime.constructCount++;
    ++g_fakeRuntime.sequence;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_bsp_gpio_construct_soft_i2c_sda(platform_gpio_t *gpio)
{
    (void)gpio;
    g_fakeRuntime.constructCount++;
    ++g_fakeRuntime.sequence;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_bsp_spi_construct_display_bus(platform_spi_bus_t *bus)
{
    (void)bus;
    g_fakeRuntime.constructCount++;
    ++g_fakeRuntime.sequence;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_bsp_st7789_construct_display(platform_st7789_t *display)
{
    (void)display;
    g_fakeRuntime.constructCount++;
    ++g_fakeRuntime.sequence;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_spi_bus_lifecycle_init(platform_spi_bus_t *bus)
{
    (void)bus;
    g_fakeRuntime.spiLifecycleInitCount++;
    fake_record_hardware();
    return PLATFORM_ERR_OK;
}

platform_error_t platform_spi_bus_lifecycle_start(platform_spi_bus_t *bus)
{
    (void)bus;
    g_fakeRuntime.spiLifecycleStartCount++;
    if (g_fakeRuntime.failSpiStart == PLATFORM_TRUE) {
        return PLATFORM_ERR_NO_RESOURCE;
    }
    fake_record_hardware();
    return PLATFORM_ERR_OK;
}

platform_error_t platform_spi_bus_lifecycle_stop(platform_spi_bus_t *bus)
{
    (void)bus;
    g_fakeRuntime.spiLifecycleStopCount++;
    g_fakeRuntime.spiStopSequence = ++g_fakeRuntime.rollbackSequence;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_spi_bus_lifecycle_deinit(platform_spi_bus_t *bus)
{
    (void)bus;
    g_fakeRuntime.spiLifecycleDeinitCount++;
    g_fakeRuntime.spiDeinitSequence = ++g_fakeRuntime.rollbackSequence;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_i2c_init(
    platform_i2c_t *i2c,
    const char *name,
    platform_gpio_t *scl,
    platform_gpio_t *sda)
{
    (void)name;
    (void)scl;
    (void)sda;
    i2c->initialized = PLATFORM_TRUE;
    fake_record_hardware();
    return PLATFORM_ERR_OK;
}

platform_error_t platform_dht20_init(platform_dht20_t *dht20, platform_i2c_t *i2c)
{
    (void)i2c;
    dht20->initialized = PLATFORM_TRUE;
    fake_record_hardware();
    return PLATFORM_ERR_OK;
}

platform_error_t platform_mpu6050_init(
    platform_mpu6050_t *mpu6050,
    platform_i2c_t *i2c,
    uint8_t address)
{
    (void)i2c;
    TEST_ASSERT(address == 0x68U);
    mpu6050->initialized = PLATFORM_TRUE;
    fake_record_hardware();
    return PLATFORM_ERR_OK;
}

platform_error_t platform_led_init(platform_led_t *led)
{
    led->initialized = PLATFORM_TRUE;
    fake_record_hardware();
    return PLATFORM_ERR_OK;
}

platform_error_t platform_button_init(platform_button_t *button)
{
    button->initialized = PLATFORM_TRUE;
    fake_record_hardware();
    return PLATFORM_ERR_OK;
}

platform_error_t service_uart_init(service_uart_t *service, const service_uart_config_t *config)
{
    (void)service;
    TEST_ASSERT(config->ownerThread != NULL);
    fake_record_service();
    return PLATFORM_ERR_OK;
}

platform_error_t service_button_init(service_button_t *service)
{
    service->initialized = PLATFORM_TRUE;
    fake_record_service();
    return PLATFORM_ERR_OK;
}

platform_error_t service_indicator_init(service_indicator_t *service, platform_led_t *led)
{
    (void)led;
    service->initialized = PLATFORM_TRUE;
    fake_record_service();
    return PLATFORM_ERR_OK;
}

platform_error_t service_acquisition_init(
    service_acquisition_t *service,
    const service_acquisition_config_t *config)
{
    TEST_ASSERT(config->dht20 != NULL);
    TEST_ASSERT(config->mpu6050 != NULL);
    service->initialized = PLATFORM_TRUE;
    fake_record_service();
    return PLATFORM_ERR_OK;
}

platform_error_t platform_queue_create(
    platform_queue_t *queue,
    platform_size_t itemCount,
    platform_size_t itemSize)
{
    uint32_t index = g_fakeRuntime.queueCreateCount++;
    uint32_t sequence = ++g_fakeRuntime.sequence;

    if (g_fakeRuntime.firstQueueSequence == 0U) {
        g_fakeRuntime.firstQueueSequence = sequence;
    }
    g_fakeRuntime.lastQueueSequence = sequence;
    g_fakeRuntime.queueDepths[index] = itemCount;
    g_fakeRuntime.queueItemSizes[index] = itemSize;
    g_fakeRuntime.createdQueues[index] = queue;
    queue->native = queue;
    if (index == 0U) {
        g_fakeRuntime.controlQueue = queue;
    } else if (index == 4U) {
        g_fakeRuntime.displayQueue = queue;
    }
    return PLATFORM_ERR_OK;
}

platform_error_t app_communication_init(
    app_communication_t *communication,
    const app_communication_config_t *config)
{
    (void)communication;
    g_fakeRuntime.communicationConfig = *config;
    fake_record_app();
    return PLATFORM_ERR_OK;
}

platform_error_t app_control_init(app_control_t *control, const app_control_config_t *config)
{
    (void)control;
    TEST_ASSERT(config->controlQueue == g_fakeRuntime.controlQueue);
    TEST_ASSERT(config->displayQueue == g_fakeRuntime.displayQueue);
    fake_record_app();
    return PLATFORM_ERR_OK;
}

platform_error_t app_acquisition_init(
    app_acquisition_t *acquisition,
    const app_acquisition_config_t *config)
{
    (void)acquisition;
    TEST_ASSERT(config->service != NULL);
    TEST_ASSERT(config->displayQueue == g_fakeRuntime.displayQueue);
    fake_record_app();
    return PLATFORM_ERR_OK;
}

platform_error_t app_display_init(
    app_display_t *display,
    const app_display_config_t *config)
{
    (void)display;
    TEST_ASSERT(config->display != NULL);
    TEST_ASSERT(config->spiBus != NULL);
    TEST_ASSERT(config->queue == g_fakeRuntime.displayQueue);
    fake_record_app();
    return PLATFORM_ERR_OK;
}

platform_error_t app_indicator_init(
    app_indicator_t *indicator,
    const app_indicator_config_t *config)
{
    (void)indicator;
    TEST_ASSERT(config->service != NULL);
    fake_record_app();
    return PLATFORM_ERR_OK;
}

platform_error_t platform_thread_create(
    platform_thread_t *thread,
    const platform_thread_config_t *config)
{
    uint32_t call = g_fakeRuntime.threadCreateCount + 1U;
    uint32_t index = g_fakeRuntime.threadCreateCount++;

    if (g_fakeRuntime.firstThreadSequence == 0U) {
        g_fakeRuntime.firstThreadSequence = ++g_fakeRuntime.sequence;
    } else {
        ++g_fakeRuntime.sequence;
    }
    if (call == g_fakeRuntime.failThreadCreateCall) {
        return PLATFORM_ERR_NO_RESOURCE;
    }
    g_fakeRuntime.threadNames[index] = config->name;
    g_fakeRuntime.threadStacks[index] = config->stackSizeBytes;
    g_fakeRuntime.threadPriorities[index] = config->priority;
    g_fakeRuntime.createdThreads[index] = thread;
    thread->native = thread;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_queue_send(
    platform_queue_t *queue,
    const void *item,
    uint32_t timeoutMs)
{
    TEST_ASSERT(queue == g_fakeRuntime.controlQueue);
    TEST_ASSERT(timeoutMs == PLATFORM_OS_NO_WAIT);
    g_fakeRuntime.submittedControlMessage = *(const app_control_message_t *)item;
    g_fakeRuntime.controlSubmitCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_thread_terminate(platform_thread_t *thread)
{
    uint32_t index;

    for (index = 0U; index < g_fakeRuntime.threadCreateCount; index++) {
        if (g_fakeRuntime.createdThreads[index] == thread) {
            g_fakeRuntime.terminatedThreadIndices[g_fakeRuntime.threadTerminateCount] = index;
            break;
        }
    }
    thread->native = NULL;
    g_fakeRuntime.threadTerminateCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_queue_delete(platform_queue_t *queue)
{
    uint32_t index;

    for (index = 0U; index < g_fakeRuntime.queueCreateCount; index++) {
        if (g_fakeRuntime.createdQueues[index] == queue) {
            g_fakeRuntime.deletedQueueIndices[g_fakeRuntime.queueDeleteCount] = index;
            break;
        }
    }
    queue->native = NULL;
    g_fakeRuntime.queueDeleteCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t service_acquisition_deinit(service_acquisition_t *service)
{
    service->initialized = PLATFORM_FALSE;
    g_fakeRuntime.serviceDeinitCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t service_indicator_deinit(service_indicator_t *service)
{
    service->initialized = PLATFORM_FALSE;
    g_fakeRuntime.serviceDeinitCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t service_button_deinit(service_button_t *service)
{
    service->initialized = PLATFORM_FALSE;
    g_fakeRuntime.serviceDeinitCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t service_uart_deinit(service_uart_t *service)
{
    (void)service;
    g_fakeRuntime.serviceDeinitCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_button_deinit(platform_button_t *button)
{
    button->initialized = PLATFORM_FALSE;
    g_fakeRuntime.hardwareDeinitCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_led_deinit(platform_led_t *led)
{
    led->initialized = PLATFORM_FALSE;
    g_fakeRuntime.hardwareDeinitCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_mpu6050_deinit(platform_mpu6050_t *mpu6050)
{
    mpu6050->initialized = PLATFORM_FALSE;
    g_fakeRuntime.hardwareDeinitCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_dht20_deinit(platform_dht20_t *dht20)
{
    dht20->initialized = PLATFORM_FALSE;
    g_fakeRuntime.hardwareDeinitCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_i2c_deinit(platform_i2c_t *i2c)
{
    i2c->initialized = PLATFORM_FALSE;
    g_fakeRuntime.hardwareDeinitCount++;
    g_fakeRuntime.lastHardwareDeinitSequence = ++g_fakeRuntime.rollbackSequence;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_gpio_deinit(platform_gpio_t *gpio)
{
    (void)gpio;
    g_fakeRuntime.hardwareDeinitCount++;
    return PLATFORM_ERR_OK;
}

void app_communication_task_entry(void *argument)
{
    (void)argument;
}

void app_control_task_entry(void *argument)
{
    (void)argument;
}

void app_acquisition_task_entry(void *argument)
{
    (void)argument;
}

void app_indicator_task_entry(void *argument)
{
    (void)argument;
}

void app_display_task_entry(void *argument)
{
    (void)argument;
}

/** @brief 吸收测试期间的日志输出。 */
static void fake_log_output(
    uint8_t level,
    const char *tag,
    const char *file,
    const char *func,
    long line,
    const char *format,
    ...)
{
    (void)level;
    (void)tag;
    (void)file;
    (void)func;
    (void)line;
    (void)format;
}

platform_log_output_fn_t platform_log_get_output_fn(void)
{
    return fake_log_output;
}

int main(void)
{
    int result = test_spi_start_failure_rolls_back_and_allows_retry();

    if (result != 0) {
        return result;
    }
    result = test_thread_failure_rolls_back_and_allows_retry();

    if (result != 0) {
        return result;
    }
    return test_final_composition_order_and_resources();
}
