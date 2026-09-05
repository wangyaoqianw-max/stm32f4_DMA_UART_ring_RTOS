/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_system.c
 * @brief APP 系统 Composition Root 实现。
 * @author YaoQian Wang
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_system.h"

#include "app_acquisition.h"
#include "app_communication.h"
#include "app_control.h"
#include "app_indicator.h"
#include "button/platform_bsp_button.h"
#include "dht20/platform_dht20.h"
#include "led/platform_bsp_led.h"
#include "mpu6050/platform_mpu6050.h"
#include "platform_bsp_gpio.h"
#include "platform_bsp_uart.h"
#include "platform_i2c.h"
#include "platform_queue.h"
#include "platform_thread.h"
#include "project_config.h"
#include "service_acquisition.h"
#include "service_button.h"
#include "service_indicator.h"
#include "service_log.h"
#include "service_uart.h"

#include <string.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define LOG_TAG                                "app_system"
//******************************** Defines **********************************//

//******************************** Variables *********************************//
static platform_uart_t g_communicationUart = PLATFORM_UART_INITIALIZER;
static platform_led_t g_statusLed = PLATFORM_LED_INITIALIZER;
static platform_button_t g_userButton = PLATFORM_BUTTON_INITIALIZER;
static platform_gpio_t g_softI2cScl = PLATFORM_GPIO_INITIALIZER;
static platform_gpio_t g_softI2cSda = PLATFORM_GPIO_INITIALIZER;
static platform_i2c_t g_sharedI2c = PLATFORM_I2C_INITIALIZER;
static platform_dht20_t g_dht20 = PLATFORM_DHT20_INITIALIZER;
static platform_mpu6050_t g_mpu6050 = PLATFORM_MPU6050_INITIALIZER;

static service_uart_t g_uartService = SERVICE_UART_INITIALIZER;
static service_button_t g_buttonService = SERVICE_BUTTON_INITIALIZER;
static service_indicator_t g_indicatorService = SERVICE_INDICATOR_INITIALIZER;
static service_acquisition_t g_acquisitionService = SERVICE_ACQUISITION_INITIALIZER;

static platform_queue_t g_controlQueue = PLATFORM_OS_OBJECT_INITIALIZER;
static platform_queue_t g_acquisitionQueue = PLATFORM_OS_OBJECT_INITIALIZER;
static platform_queue_t g_communicationOutboundQueue = PLATFORM_OS_OBJECT_INITIALIZER;
static platform_queue_t g_indicatorQueue = PLATFORM_OS_OBJECT_INITIALIZER;

static app_communication_t g_appCommunication = APP_COMMUNICATION_INITIALIZER;
static app_control_t g_appControl = APP_CONTROL_INITIALIZER;
static app_acquisition_t g_appAcquisition = APP_ACQUISITION_INITIALIZER;
static app_indicator_t g_appIndicator = APP_INDICATOR_INITIALIZER;

static platform_thread_t g_communicationThread = PLATFORM_OS_OBJECT_INITIALIZER;
static platform_thread_t g_controlThread = PLATFORM_OS_OBJECT_INITIALIZER;
static platform_thread_t g_acquisitionThread = PLATFORM_OS_OBJECT_INITIALIZER;
static platform_thread_t g_indicatorThread = PLATFORM_OS_OBJECT_INITIALIZER;

static uint8_t g_dmaRxStorage[PROJECT_COMM_DMA_RX_BUFFER_SIZE] = {0};
static uint8_t g_ringStorage[PROJECT_COMM_RING_BUFFER_STORAGE_SIZE] = {0};
static platform_bool_t g_isInitialized = PLATFORM_FALSE;

static const platform_uart_config_t g_communicationUartConfig = {
    .baudRate = PROJECT_COMM_UART_BAUD_RATE,
    .dataBits = PROJECT_COMM_UART_DATA_BITS,
    .stopBits = PROJECT_COMM_UART_STOP_BITS,
    .parity = PROJECT_COMM_UART_PARITY,
    .flowControl = PROJECT_COMM_UART_FLOW_CONTROL,
    .defaultTimeoutMs = PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS
};
//******************************** Variables *********************************//

//******************************** Private Functions *************************//
/** @brief 将 Communication 提交的 UART 事件封装为 Control Queue 消息。 */
static platform_error_t app_system_submit_uart_control(
    void *context,
    app_ctrl_event_t event)
{
    platform_queue_t *controlQueue = (platform_queue_t *)context;
    app_control_message_t message = {
        .type = APP_CONTROL_MESSAGE_CONTROL_REQUEST,
        .payload.request = {
            .event = event,
            .source = APP_CTRL_SOURCE_UART
        }
    };

    if (controlQueue == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    return platform_queue_send(controlQueue, &message, PLATFORM_OS_NO_WAIT);
}

/** @brief 将 Composition Root 的静态对象恢复为零初始化状态。 */
static void app_system_reset_storage(void)
{
    (void)memset(&g_communicationUart, 0, sizeof(g_communicationUart));
    (void)memset(&g_statusLed, 0, sizeof(g_statusLed));
    (void)memset(&g_userButton, 0, sizeof(g_userButton));
    (void)memset(&g_softI2cScl, 0, sizeof(g_softI2cScl));
    (void)memset(&g_softI2cSda, 0, sizeof(g_softI2cSda));
    (void)memset(&g_sharedI2c, 0, sizeof(g_sharedI2c));
    (void)memset(&g_dht20, 0, sizeof(g_dht20));
    (void)memset(&g_mpu6050, 0, sizeof(g_mpu6050));
    (void)memset(&g_uartService, 0, sizeof(g_uartService));
    (void)memset(&g_buttonService, 0, sizeof(g_buttonService));
    (void)memset(&g_indicatorService, 0, sizeof(g_indicatorService));
    (void)memset(&g_acquisitionService, 0, sizeof(g_acquisitionService));
    (void)memset(&g_controlQueue, 0, sizeof(g_controlQueue));
    (void)memset(&g_acquisitionQueue, 0, sizeof(g_acquisitionQueue));
    (void)memset(&g_communicationOutboundQueue, 0, sizeof(g_communicationOutboundQueue));
    (void)memset(&g_indicatorQueue, 0, sizeof(g_indicatorQueue));
    (void)memset(&g_appCommunication, 0, sizeof(g_appCommunication));
    (void)memset(&g_appControl, 0, sizeof(g_appControl));
    (void)memset(&g_appAcquisition, 0, sizeof(g_appAcquisition));
    (void)memset(&g_appIndicator, 0, sizeof(g_appIndicator));
    (void)memset(&g_communicationThread, 0, sizeof(g_communicationThread));
    (void)memset(&g_controlThread, 0, sizeof(g_controlThread));
    (void)memset(&g_acquisitionThread, 0, sizeof(g_acquisitionThread));
    (void)memset(&g_indicatorThread, 0, sizeof(g_indicatorThread));
}

/** @brief 按创建顺序逆序释放已建立资源并恢复可重试状态。 */
static void app_system_rollback(
    platform_bool_t uartServiceInitialized,
    platform_bool_t buttonServiceInitialized,
    platform_bool_t indicatorServiceInitialized,
    platform_bool_t acquisitionServiceInitialized)
{
    if (g_indicatorThread.native != NULL) {
        (void)platform_thread_terminate(&g_indicatorThread);
    }
    if (g_acquisitionThread.native != NULL) {
        (void)platform_thread_terminate(&g_acquisitionThread);
    }
    if (g_controlThread.native != NULL) {
        (void)platform_thread_terminate(&g_controlThread);
    }
    if (g_communicationThread.native != NULL) {
        (void)platform_thread_terminate(&g_communicationThread);
    }

    if (g_indicatorQueue.native != NULL) {
        (void)platform_queue_delete(&g_indicatorQueue);
    }
    if (g_communicationOutboundQueue.native != NULL) {
        (void)platform_queue_delete(&g_communicationOutboundQueue);
    }
    if (g_acquisitionQueue.native != NULL) {
        (void)platform_queue_delete(&g_acquisitionQueue);
    }
    if (g_controlQueue.native != NULL) {
        (void)platform_queue_delete(&g_controlQueue);
    }

    if (acquisitionServiceInitialized == PLATFORM_TRUE) {
        (void)service_acquisition_deinit(&g_acquisitionService);
    }
    if (indicatorServiceInitialized == PLATFORM_TRUE) {
        (void)service_indicator_deinit(&g_indicatorService);
    }
    if (buttonServiceInitialized == PLATFORM_TRUE) {
        (void)service_button_deinit(&g_buttonService);
    }
    if (uartServiceInitialized == PLATFORM_TRUE) {
        (void)service_uart_deinit(&g_uartService);
    }

    if (g_userButton.initialized == PLATFORM_TRUE) {
        (void)platform_button_deinit(&g_userButton);
    }
    if (g_statusLed.initialized == PLATFORM_TRUE) {
        (void)platform_led_deinit(&g_statusLed);
    }
    if (g_mpu6050.initialized == PLATFORM_TRUE) {
        (void)platform_mpu6050_deinit(&g_mpu6050);
    }
    if (g_dht20.initialized == PLATFORM_TRUE) {
        (void)platform_dht20_deinit(&g_dht20);
    }
    if (g_sharedI2c.initialized == PLATFORM_TRUE) {
        (void)platform_i2c_deinit(&g_sharedI2c);
    }

    app_system_reset_storage();
    g_isInitialized = PLATFORM_FALSE;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t app_system_init(void)
{
    service_uart_config_t uartServiceConfig = {
        .uart = &g_communicationUart,
        .dmaRxBuffer = g_dmaRxStorage,
        .dmaRxBufferSize = PROJECT_COMM_DMA_RX_BUFFER_SIZE,
        .ringBufferStorage = g_ringStorage,
        .ringBufferStorageSize = PROJECT_COMM_RING_BUFFER_STORAGE_SIZE,
        .ownerThread = &g_communicationThread
    };
    service_acquisition_config_t acquisitionServiceConfig = {
        .dht20 = &g_dht20,
        .mpu6050 = &g_mpu6050
    };
    app_communication_config_t communicationConfig = {
        .uart = &g_communicationUart,
        .service = &g_uartService,
        .controlHandler = app_system_submit_uart_control,
        .controlContext = &g_controlQueue,
        .outboundQueue = &g_communicationOutboundQueue,
        .controlQueue = &g_controlQueue
    };
    app_control_config_t controlConfig = {
        .button = &g_userButton,
        .buttonService = &g_buttonService,
        .controlQueue = &g_controlQueue,
        .acquisitionQueue = &g_acquisitionQueue,
        .communicationQueue = &g_communicationOutboundQueue,
        .indicatorQueue = &g_indicatorQueue
    };
    app_acquisition_config_t acquisitionConfig = {
        .service = &g_acquisitionService,
        .commandQueue = &g_acquisitionQueue,
        .communicationQueue = &g_communicationOutboundQueue,
        .controlQueue = &g_controlQueue
    };
    app_indicator_config_t indicatorConfig = {
        .service = &g_indicatorService,
        .queue = &g_indicatorQueue
    };
    platform_thread_config_t communicationThreadConfig = {
        .name = "communication",
        .entry = app_communication_task_entry,
        .argument = &g_appCommunication,
        .stackSizeBytes = PROJECT_COMM_TASK_STACK_SIZE_BYTES,
        .priority = PROJECT_COMM_TASK_PRIORITY
    };
    platform_thread_config_t controlThreadConfig = {
        .name = "control",
        .entry = app_control_task_entry,
        .argument = &g_appControl,
        .stackSizeBytes = PROJECT_CONTROL_TASK_STACK_SIZE_BYTES,
        .priority = PROJECT_CONTROL_TASK_PRIORITY
    };
    platform_thread_config_t acquisitionThreadConfig = {
        .name = "acquisition",
        .entry = app_acquisition_task_entry,
        .argument = &g_appAcquisition,
        .stackSizeBytes = PROJECT_ACQUISITION_TASK_STACK_SIZE_BYTES,
        .priority = PROJECT_ACQUISITION_TASK_PRIORITY
    };
    platform_thread_config_t indicatorThreadConfig = {
        .name = "indicator",
        .entry = app_indicator_task_entry,
        .argument = &g_appIndicator,
        .stackSizeBytes = PROJECT_INDICATOR_TASK_STACK_SIZE_BYTES,
        .priority = PROJECT_INDICATOR_TASK_PRIORITY
    };
    platform_bool_t uartServiceInitialized = PLATFORM_FALSE;
    platform_bool_t buttonServiceInitialized = PLATFORM_FALSE;
    platform_bool_t indicatorServiceInitialized = PLATFORM_FALSE;
    platform_bool_t acquisitionServiceInitialized = PLATFORM_FALSE;
    platform_error_t result;

    if (g_isInitialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    result = platform_bsp_uart_construct_communication(
        &g_communicationUart, &g_communicationUartConfig);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_bsp_led_construct_status_led(&g_statusLed);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_bsp_button_construct_user_key(&g_userButton);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_bsp_gpio_construct_soft_i2c_scl(&g_softI2cScl);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_bsp_gpio_construct_soft_i2c_sda(&g_softI2cSda);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }

    result = platform_i2c_init(
        &g_sharedI2c, "shared_soft_i2c", &g_softI2cScl, &g_softI2cSda);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_dht20_init(&g_dht20, &g_sharedI2c);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_mpu6050_init(
        &g_mpu6050, &g_sharedI2c, PROJECT_MPU6050_I2C_ADDRESS);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_led_init(&g_statusLed);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_button_init(&g_userButton);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }

    result = service_uart_init(&g_uartService, &uartServiceConfig);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    uartServiceInitialized = PLATFORM_TRUE;
    result = service_button_init(&g_buttonService);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    buttonServiceInitialized = PLATFORM_TRUE;
    result = service_indicator_init(&g_indicatorService, &g_statusLed);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    indicatorServiceInitialized = PLATFORM_TRUE;
    result = service_acquisition_init(
        &g_acquisitionService, &acquisitionServiceConfig);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    acquisitionServiceInitialized = PLATFORM_TRUE;

    result = platform_queue_create(
        &g_controlQueue,
        PROJECT_CONTROL_QUEUE_DEPTH,
        sizeof(app_control_message_t));
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_queue_create(
        &g_acquisitionQueue,
        PROJECT_ACQUISITION_QUEUE_DEPTH,
        sizeof(app_acquisition_command_t));
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_queue_create(
        &g_communicationOutboundQueue,
        PROJECT_COMM_OUTBOUND_QUEUE_DEPTH,
        sizeof(app_communication_outbound_message_t));
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_queue_create(
        &g_indicatorQueue,
        PROJECT_INDICATOR_QUEUE_DEPTH,
        sizeof(app_indicator_command_t));
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }

    result = app_control_init(&g_appControl, &controlConfig);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = app_acquisition_init(&g_appAcquisition, &acquisitionConfig);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = app_communication_init(&g_appCommunication, &communicationConfig);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = app_indicator_init(&g_appIndicator, &indicatorConfig);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }

    result = platform_thread_create(&g_communicationThread, &communicationThreadConfig);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_thread_create(&g_controlThread, &controlThreadConfig);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_thread_create(&g_acquisitionThread, &acquisitionThreadConfig);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }
    result = platform_thread_create(&g_indicatorThread, &indicatorThreadConfig);
    if (result != PLATFORM_ERR_OK) {
        goto cleanup;
    }

    g_isInitialized = PLATFORM_TRUE;
    SERVICE_LOG_I("system composition initialized");
    return PLATFORM_ERR_OK;

cleanup:
    app_system_rollback(
        uartServiceInitialized,
        buttonServiceInitialized,
        indicatorServiceInitialized,
        acquisitionServiceInitialized);
    return result;
}
//******************************** Functions *********************************//
