/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_app_system.c
 * @brief 验证 Phase 9 Composition Root 的依赖顺序、资源和回滚。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#include "app_acquisition.h"
#include "app_communication.h"
#include "app_control.h"
#include "app_indicator.h"
#include "app_system.h"
#include "button/platform_bsp_button.h"
#include "led/platform_bsp_led.h"
#include "platform_bsp_uart.h"
#include "platform_queue.h"
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
    uint32_t failThreadCreateCall;
    platform_size_t queueDepths[4];
    platform_size_t queueItemSizes[4];
    const char *threadNames[4];
    uint32_t threadStacks[4];
    platform_thread_priority_t threadPriorities[4];
    app_communication_config_t communicationConfig;
    platform_queue_t *controlQueue;
    app_control_message_t submittedControlMessage;
    uint32_t controlSubmitCount;
} fake_system_runtime_t;

static fake_system_runtime_t g_fakeRuntime;

static void fake_runtime_reset(void)
{
    memset(&g_fakeRuntime, 0, sizeof(g_fakeRuntime));
}

static void fake_record_hardware(void)
{
    g_fakeRuntime.hardwareInitCount++;
    g_fakeRuntime.lastHardwareSequence = ++g_fakeRuntime.sequence;
}

static void fake_record_service(void)
{
    uint32_t sequence = ++g_fakeRuntime.sequence;

    if (g_fakeRuntime.firstServiceSequence == 0U) {
        g_fakeRuntime.firstServiceSequence = sequence;
    }
    g_fakeRuntime.lastServiceSequence = sequence;
    g_fakeRuntime.serviceInitCount++;
}

static void fake_record_app(void)
{
    uint32_t sequence = ++g_fakeRuntime.sequence;

    if (g_fakeRuntime.firstAppSequence == 0U) {
        g_fakeRuntime.firstAppSequence = sequence;
    }
    g_fakeRuntime.lastAppSequence = sequence;
    g_fakeRuntime.appInitCount++;
}

static int test_thread_failure_rolls_back_and_allows_retry(void)
{
    fake_runtime_reset();
    g_fakeRuntime.failThreadCreateCall = 3U;

    TEST_ASSERT(app_system_init() == PLATFORM_ERR_NO_RESOURCE);
    TEST_ASSERT(g_fakeRuntime.threadTerminateCount == 2U);
    TEST_ASSERT(g_fakeRuntime.queueDeleteCount == 4U);
    TEST_ASSERT(g_fakeRuntime.serviceDeinitCount == 4U);
    TEST_ASSERT(g_fakeRuntime.hardwareDeinitCount == 5U);

    fake_runtime_reset();
    TEST_ASSERT(app_system_init() == PLATFORM_ERR_OK);

    return 0;
}

static int test_final_composition_order_and_resources(void)
{
    static const char *expectedNames[] = {
        "communication",
        "control",
        "acquisition",
        "indicator"
    };
    static const uint32_t expectedStacks[] = {2048U, 1024U, 1536U, 768U};
    static const platform_thread_priority_t expectedPriorities[] = {
        PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL,
        PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL,
        PLATFORM_THREAD_PRIORITY_NORMAL,
        PLATFORM_THREAD_PRIORITY_BELOW_NORMAL
    };
    static const platform_size_t expectedDepths[] = {8U, 4U, 8U, 4U};
    static const platform_size_t expectedItemSizes[] = {
        sizeof(app_control_message_t),
        sizeof(app_acquisition_command_t),
        sizeof(app_communication_outbound_message_t),
        sizeof(app_indicator_command_t)
    };
    uint32_t index;

    TEST_ASSERT(g_fakeRuntime.constructCount == 5U);
    TEST_ASSERT(g_fakeRuntime.hardwareInitCount == 5U);
    TEST_ASSERT(g_fakeRuntime.serviceInitCount == 4U);
    TEST_ASSERT(g_fakeRuntime.queueCreateCount == 4U);
    TEST_ASSERT(g_fakeRuntime.appInitCount == 4U);
    TEST_ASSERT(g_fakeRuntime.threadCreateCount == 4U);
    TEST_ASSERT(g_fakeRuntime.lastHardwareSequence < g_fakeRuntime.firstServiceSequence);
    TEST_ASSERT(g_fakeRuntime.lastServiceSequence < g_fakeRuntime.firstQueueSequence);
    TEST_ASSERT(g_fakeRuntime.lastQueueSequence < g_fakeRuntime.firstAppSequence);
    TEST_ASSERT(g_fakeRuntime.lastAppSequence < g_fakeRuntime.firstThreadSequence);

    for (index = 0U; index < 4U; index++) {
        TEST_ASSERT(g_fakeRuntime.queueDepths[index] == expectedDepths[index]);
        TEST_ASSERT(g_fakeRuntime.queueItemSizes[index] == expectedItemSizes[index]);
        TEST_ASSERT(strcmp(g_fakeRuntime.threadNames[index], expectedNames[index]) == 0);
        TEST_ASSERT(g_fakeRuntime.threadStacks[index] == expectedStacks[index]);
        TEST_ASSERT(g_fakeRuntime.threadPriorities[index] == expectedPriorities[index]);
    }

    TEST_ASSERT(g_fakeRuntime.communicationConfig.controlHandler != NULL);
    TEST_ASSERT(g_fakeRuntime.communicationConfig.outboundQueue != NULL);
    TEST_ASSERT(g_fakeRuntime.communicationConfig.controlQueue == g_fakeRuntime.controlQueue);
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
    queue->native = queue;
    if (index == 0U) {
        g_fakeRuntime.controlQueue = queue;
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
    fake_record_app();
    return PLATFORM_ERR_OK;
}

platform_error_t app_acquisition_init(
    app_acquisition_t *acquisition,
    const app_acquisition_config_t *config)
{
    (void)acquisition;
    TEST_ASSERT(config->service != NULL);
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
    thread->native = NULL;
    g_fakeRuntime.threadTerminateCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_queue_delete(platform_queue_t *queue)
{
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
    int result = test_thread_failure_rolls_back_and_allows_retry();

    if (result != 0) {
        return result;
    }
    return test_final_composition_order_and_resources();
}
