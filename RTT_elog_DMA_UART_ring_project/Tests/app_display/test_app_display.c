/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_app_display.c
 * @brief 验证 Display Task 的 GUI 生命周期、消息预算与触摸服务
 * @author YaoQian Wang
 * @date 2026-10-03
 * @version V1.0
 *****************************************************************************/

#include "app_display.h"
#include "platform_gui.h"
#include "service_log.h"
#include "ui_sensor_monitor.h"

#include <stdio.h>
#include <string.h>

#define TEST_ASSERT(condition) do { if (!(condition)) { return __LINE__; } } while (0)
#define TEST_MESSAGE_CAPACITY (16U)

typedef struct
{
    platform_queue_t queue;
    platform_queue_t controlQueue;
    app_control_event_handler_t handler;
    void *handlerContext;
    app_control_message_t requests[16];
    app_control_ui_status_t status;
    platform_error_t sendResult;
    platform_bool_t pending;
    uint32_t requestCount;
    uint32_t nowMs;
    uint32_t requestFailureCount;
    uint32_t sampleFailureCount;
    uint32_t measurementUpdateCount;
    platform_cst816t_t touch;
    platform_i2c_t touchI2c;
    platform_gpio_t touchScl;
    platform_gpio_t touchSda;
    platform_gpio_t touchReset;
    platform_thread_t thread;
    platform_cst816t_sample_t sample;
    app_display_message_t messages[TEST_MESSAGE_CAPACITY];
    platform_error_t lcdResult;
    platform_error_t guiInitResult;
    platform_error_t guiProcessResult;
    platform_error_t touchInitResult;
    platform_error_t touchReadResult;
    uint32_t messageCount;
    uint32_t messageReadIndex;
    uint32_t receiveCount;
    uint32_t receiveTimeouts[TEST_MESSAGE_CAPACITY];
    uint32_t lcdInitCount;
    uint32_t lcdDeinitCount;
    uint32_t backlightOnCount;
    uint32_t backlightOffCount;
    uint32_t guiInitCount;
    uint32_t guiProcessCount;
    uint32_t uiCreateCount;
    uint32_t touchInitCount;
    uint32_t touchReadCount;
    uint32_t notifyCount;
    uint32_t pendingFlags;
    uint32_t guiFailureLogCount;
} fake_runtime_t;

static fake_runtime_t g_fake;

static void fake_reset(void)
{
    (void)memset(&g_fake, 0, sizeof(g_fake));
    g_fake.queue.native = &g_fake.queue;
    g_fake.controlQueue.native = &g_fake.controlQueue;
    g_fake.nowMs = 100U;
    g_fake.thread.native = &g_fake.thread;
}

static app_display_t create_display(platform_st7789_t *display,
                                    platform_spi_bus_t *spiBus)
{
    app_display_t appDisplay = APP_DISPLAY_INITIALIZER;
    app_display_config_t config = {
        .display = display,
        .spiBus = spiBus,
        .queue = &g_fake.queue,
        .controlQueue = &g_fake.controlQueue,
        .touch = &g_fake.touch,
        .touchI2c = &g_fake.touchI2c,
        .touchScl = &g_fake.touchScl,
        .touchSda = &g_fake.touchSda,
        .touchReset = &g_fake.touchReset,
        .thread = &g_fake.thread
    };

    (void)app_display_init(&appDisplay, &config);
    return appDisplay;
}

static void enqueue(app_display_message_t message)
{
    g_fake.messages[g_fake.messageCount++] = message;
}

static int test_start_initializes_gui_after_lcd(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;

    fake_reset();
    appDisplay = create_display(&display, &bus);
    TEST_ASSERT(g_fake.lcdInitCount == 0U);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.lcdInitCount == 1U);
    TEST_ASSERT(g_fake.guiInitCount == 1U);
    TEST_ASSERT(g_fake.uiCreateCount == 1U);
    TEST_ASSERT(g_fake.backlightOnCount == 1U);
    TEST_ASSERT(appDisplay.context.available == PLATFORM_TRUE);
    return 0;
}

static int test_lcd_failure_keeps_touch_and_cache_running(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;
    app_display_message_t message = {0};

    fake_reset();
    g_fake.lcdResult = PLATFORM_ERR_IO;
    appDisplay = create_display(&display, &bus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.touchInitCount == 1U);
    TEST_ASSERT(g_fake.guiInitCount == 0U);
    TEST_ASSERT(appDisplay.context.available == PLATFORM_FALSE);
    message.type = APP_DISPLAY_MESSAGE_MEASUREMENT;
    message.payload.measurement.environment.temperatureC = 20.0F;
    enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.measurementValid == PLATFORM_TRUE);
    TEST_ASSERT(g_fake.guiProcessCount == 0U);
    return 0;
}

static int test_gui_failure_disables_lcd(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;

    fake_reset();
    g_fake.guiInitResult = PLATFORM_ERR_NO_MEMORY;
    appDisplay = create_display(&display, &bus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.uiCreateCount == 0U);
    TEST_ASSERT(g_fake.lcdDeinitCount == 1U);
    TEST_ASSERT(appDisplay.context.available == PLATFORM_FALSE);
    return 0;
}

static int test_touch_failure_still_services_gui(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;

    fake_reset();
    g_fake.touchInitResult = PLATFORM_ERR_NOT_FOUND;
    appDisplay = create_display(&display, &bus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.touchAvailable == PLATFORM_FALSE);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.guiProcessCount == 1U);
    return 0;
}

static int test_queue_budget_retains_latest_cache_and_runs_gui(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;
    app_display_message_t message = {0};
    uint32_t index;

    fake_reset();
    appDisplay = create_display(&display, &bus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    g_fake.receiveCount = 0U;
    message.type = APP_DISPLAY_MESSAGE_MEASUREMENT;
    for (index = 0U; index < 8U; index++) {
        message.payload.measurement.environment.temperatureC = (float)index;
        enqueue(message);
    }
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.messageReadIndex == 4U);
    TEST_ASSERT(g_fake.receiveTimeouts[0] == PROJECT_DISPLAY_WAIT_TIMEOUT_MS);
    TEST_ASSERT(g_fake.receiveTimeouts[1] == PLATFORM_OS_NO_WAIT);
    TEST_ASSERT(appDisplay.context.latestMeasurement.environment.temperatureC == 3.0F);
    TEST_ASSERT(g_fake.guiProcessCount == 1U);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.messageReadIndex == 8U);
    TEST_ASSERT(appDisplay.context.latestMeasurement.environment.temperatureC == 7.0F);
    TEST_ASSERT(g_fake.guiProcessCount == 2U);
    return 0;
}

static int test_empty_queue_services_touch_and_gui(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;

    fake_reset();
    appDisplay = create_display(&display, &bus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    g_fake.sample.pressed = PLATFORM_TRUE;
    g_fake.sample.x = 123U;
    g_fake.sample.y = 234U;
    app_display_touch_irq_from_isr(&appDisplay);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.touchSample.x == 123U);
    TEST_ASSERT(g_fake.touchReadCount == 2U);
    TEST_ASSERT(g_fake.guiProcessCount == 1U);
    g_fake.touchReadResult = PLATFORM_ERR_IO;
    app_display_touch_irq_from_isr(&appDisplay);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.touchSample.pressed == PLATFORM_FALSE);
    TEST_ASSERT(g_fake.guiProcessCount == 2U);
    return 0;
}

static int test_flush_failure_is_local(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;

    fake_reset();
    appDisplay = create_display(&display, &bus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    g_fake.guiProcessResult = PLATFORM_ERR_IO;
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.statistics.renderFailureCount == 1U);
    TEST_ASSERT(appDisplay.context.available == PLATFORM_TRUE);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.statistics.renderFailureCount == 2U);
    TEST_ASSERT(g_fake.guiFailureLogCount == 1U);
    g_fake.guiProcessResult = PLATFORM_ERR_OK;
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.guiProcessCount == 3U);
    g_fake.guiProcessResult = PLATFORM_ERR_IO;
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.guiFailureLogCount == 2U);
    return 0;
}

platform_error_t platform_st7789_init(platform_st7789_t *display,
                                      platform_spi_bus_t *spiBus)
{
    (void)spiBus;
    g_fake.lcdInitCount++;
    if (g_fake.lcdResult == PLATFORM_ERR_OK) {
        display->initialized = PLATFORM_TRUE;
    }
    return g_fake.lcdResult;
}

platform_error_t platform_st7789_deinit(platform_st7789_t *display)
{
    display->initialized = PLATFORM_FALSE;
    g_fake.lcdDeinitCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_st7789_backlight_on(platform_st7789_t *display)
{
    (void)display;
    g_fake.backlightOnCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_st7789_backlight_off(platform_st7789_t *display)
{
    (void)display;
    g_fake.backlightOffCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_gui_init(platform_st7789_t *display,
                                   const platform_cst816t_sample_t *sample)
{
    TEST_ASSERT(display->initialized == PLATFORM_TRUE);
    TEST_ASSERT(sample != NULL);
    g_fake.guiInitCount++;
    return g_fake.guiInitResult;
}

platform_error_t platform_gui_process(void)
{
    g_fake.guiProcessCount++;
    return g_fake.guiProcessResult;
}

platform_error_t ui_sensor_monitor_create(app_control_event_handler_t handler, void *context)
{
    g_fake.uiCreateCount++;
    g_fake.handler = handler;
    g_fake.handlerContext = context;
    return PLATFORM_ERR_OK;
}

void ui_sensor_monitor_update_control(const app_control_ui_status_t *status, platform_bool_t pending)
{
    g_fake.status = *status;
    g_fake.pending = pending;
}

void ui_sensor_monitor_update_measurement(const app_acquisition_data_t *measurement)
{
    (void)measurement;
    g_fake.measurementUpdateCount++;
}

void ui_sensor_monitor_show_request_failure(void)
{
    g_fake.requestFailureCount++;
}

void ui_sensor_monitor_show_sample_failure(void)
{
    g_fake.sampleFailureCount++;
}

platform_error_t platform_queue_send(platform_queue_t *queue, const void *item, uint32_t timeoutMs)
{
    TEST_ASSERT(queue == &g_fake.controlQueue);
    TEST_ASSERT(timeoutMs == PLATFORM_OS_NO_WAIT);
    if (g_fake.sendResult != PLATFORM_ERR_OK) {
        return g_fake.sendResult;
    }
    g_fake.requests[g_fake.requestCount++] = *(const app_control_message_t *)item;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_queue_receive(platform_queue_t *queue, void *item,
                                        uint32_t timeoutMs)
{
    TEST_ASSERT(queue == &g_fake.queue);
    g_fake.receiveTimeouts[g_fake.receiveCount++ % TEST_MESSAGE_CAPACITY] = timeoutMs;
    if (g_fake.messageReadIndex >= g_fake.messageCount) {
        return (timeoutMs == PLATFORM_OS_NO_WAIT) ? PLATFORM_ERR_EMPTY : PLATFORM_ERR_TIMEOUT;
    }
    *(app_display_message_t *)item = g_fake.messages[g_fake.messageReadIndex++];
    return PLATFORM_ERR_OK;
}

platform_error_t platform_time_delay_ms(uint32_t delayMs)
{
    (void)delayMs;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_time_get_ms(uint32_t *timeMs)
{
    *timeMs = g_fake.nowMs;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_i2c_init(platform_i2c_t *i2c, const char *name,
                                   platform_gpio_t *scl, platform_gpio_t *sda)
{
    (void)i2c;
    (void)name;
    (void)scl;
    (void)sda;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_cst816t_init(platform_cst816t_t *touch,
                                       platform_i2c_t *i2c, platform_gpio_t *reset)
{
    (void)touch;
    (void)i2c;
    (void)reset;
    g_fake.touchInitCount++;
    return g_fake.touchInitResult;
}

platform_error_t platform_cst816t_read_sample(platform_cst816t_t *touch,
                                              platform_cst816t_sample_t *sample)
{
    (void)touch;
    g_fake.touchReadCount++;
    if (g_fake.touchReadResult == PLATFORM_ERR_OK) {
        *sample = g_fake.sample;
    }
    return g_fake.touchReadResult;
}

platform_error_t platform_notify_set_from_isr(platform_thread_t *thread,
                                              uint32_t flags)
{
    TEST_ASSERT(thread == &g_fake.thread);
    g_fake.notifyCount++;
    g_fake.pendingFlags |= flags;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_notify_wait(uint32_t flags, platform_bool_t waitAll,
                                      platform_bool_t clearOnExit, uint32_t timeoutMs,
                                      uint32_t *receivedFlags)
{
    (void)flags;
    (void)waitAll;
    (void)clearOnExit;
    (void)timeoutMs;
    if (g_fake.pendingFlags == 0U) {
        return PLATFORM_ERR_EMPTY;
    }
    *receivedFlags = g_fake.pendingFlags;
    g_fake.pendingFlags = 0U;
    return PLATFORM_ERR_OK;
}

static void fake_log_output(uint8_t level, const char *tag, const char *file,
                            const char *func, long line, const char *format, ...)
{
    (void)level;
    (void)tag;
    (void)file;
    (void)func;
    (void)line;
    if (strcmp(format, "gui flush failed: %d") == 0) {
        g_fake.guiFailureLogCount++;
    }
}

platform_log_output_fn_t platform_log_get_output_fn(void)
{
    return fake_log_output;
}

static int test_ui_request_waits_for_confirmation_and_recovers_lost_response(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;
    app_display_message_t message = {0};
    uint32_t requests;

    fake_reset();
    appDisplay = create_display(&display, &bus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.handler != NULL);
    message.type = APP_DISPLAY_MESSAGE_CONTROL_STATUS;
    message.payload.controlStatus.state = APP_CONTROL_STATE_STOPPED;
    enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    g_fake.requestCount = 0U;
    TEST_ASSERT(g_fake.handler(g_fake.handlerContext, APP_CTRL_START) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.requests[0].payload.request.source == APP_CTRL_SOURCE_UI);
    TEST_ASSERT(g_fake.requests[0].payload.request.event == APP_CTRL_START);
    TEST_ASSERT(appDisplay.context.requestPending == PLATFORM_TRUE);
    TEST_ASSERT(appDisplay.context.systemState == APP_CONTROL_STATE_STOPPED);
    TEST_ASSERT(g_fake.pending == PLATFORM_TRUE);
    message.payload.controlStatus.responseValid = PLATFORM_TRUE;
    message.payload.controlStatus.response = APP_CONTROL_RESPONSE_OK_ONCE;
    message.payload.controlStatus.source = APP_CTRL_SOURCE_UART;
    enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.requestPending == PLATFORM_TRUE);
    /* 点击前已在队列中的查询响应不能充当这次 START 的执行确认。 */
    message.payload.controlStatus.responseValid = PLATFORM_TRUE;
    message.payload.controlStatus.response = APP_CONTROL_RESPONSE_STATUS_STOPPED;
    message.payload.controlStatus.source = APP_CTRL_SOURCE_UI;
    enqueue(message);
    g_fake.nowMs = 500U;
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.requestPending == PLATFORM_TRUE);
    g_fake.nowMs = 1099U;
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.requestCount == 1U);
    g_fake.nowMs = 1100U;
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.requests[1].payload.request.event == APP_CTRL_GET_STATUS);
    requests = g_fake.requestCount;
    g_fake.nowMs = 1200U;
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.requestCount == requests);
    message.payload.controlStatus.state = APP_CONTROL_STATE_RUNNING;
    message.payload.controlStatus.responseValid = PLATFORM_TRUE;
    message.payload.controlStatus.response = APP_CONTROL_RESPONSE_STATUS_RUNNING;
    message.payload.controlStatus.source = APP_CTRL_SOURCE_UI;
    enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.requestPending == PLATFORM_FALSE);
    TEST_ASSERT(g_fake.status.state == APP_CONTROL_STATE_RUNNING);
    TEST_ASSERT(g_fake.pending == PLATFORM_FALSE);
    return 0;
}

static int test_queue_full_and_external_once_do_not_stick_or_corrupt_cache(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;
    app_display_message_t message = {0};

    fake_reset();
    appDisplay = create_display(&display, &bus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.handler != NULL);
    message.type = APP_DISPLAY_MESSAGE_CONTROL_STATUS;
    message.payload.controlStatus.state = APP_CONTROL_STATE_STOPPED;
    enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    g_fake.sendResult = PLATFORM_ERR_FULL;
    TEST_ASSERT(g_fake.handler(g_fake.handlerContext, APP_CTRL_SAMPLE_ONCE) == PLATFORM_ERR_FULL);
    TEST_ASSERT(appDisplay.context.requestPending == PLATFORM_FALSE);
    TEST_ASSERT(g_fake.requestFailureCount == 1U);
    g_fake.sendResult = PLATFORM_ERR_OK;
    message.payload.controlStatus.onceActive = PLATFORM_TRUE;
    enqueue(message);
    message.type = APP_DISPLAY_MESSAGE_SYSTEM_STATE;
    message.payload.systemState = APP_CONTROL_STATE_RUNNING;
    enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.status.onceActive == PLATFORM_TRUE);
    TEST_ASSERT(g_fake.status.state == APP_CONTROL_STATE_STOPPED);
    TEST_ASSERT(g_fake.handler(g_fake.handlerContext, APP_CTRL_START) == PLATFORM_ERR_BUSY);
    message.type = APP_DISPLAY_MESSAGE_CONTROL_STATUS;
    memset(&message.payload.controlStatus, 0, sizeof(message.payload.controlStatus));
    message.payload.controlStatus.responseValid = PLATFORM_TRUE;
    message.payload.controlStatus.response = APP_CONTROL_RESPONSE_ACQUISITION_FAILED;
    enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fake.status.onceActive == PLATFORM_FALSE);
    message.type = APP_DISPLAY_MESSAGE_MEASUREMENT;
    message.payload.measurement.environment.temperatureC = 25.0F;
    enqueue(message);
    message.type = APP_DISPLAY_MESSAGE_ACQUISITION_FAILURE;
    message.payload.acquisitionResult = PLATFORM_ERR_IO;
    enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.latestMeasurement.environment.temperatureC == 25.0F);
    TEST_ASSERT(g_fake.sampleFailureCount == 1U);
    return 0;
}

int main(void)
{
    static int (*const tests[])(void) = {
        test_start_initializes_gui_after_lcd,
        test_lcd_failure_keeps_touch_and_cache_running,
        test_gui_failure_disables_lcd,
        test_touch_failure_still_services_gui,
        test_queue_budget_retains_latest_cache_and_runs_gui,
        test_empty_queue_services_touch_and_gui,
        test_flush_failure_is_local,
        test_ui_request_waits_for_confirmation_and_recovers_lost_response,
        test_queue_full_and_external_once_do_not_stick_or_corrupt_cache
    };
    uint32_t index;
    uint32_t failures = 0U;

    for (index = 0U; index < sizeof(tests) / sizeof(tests[0]); index++) {
        int result = tests[index]();
        if (result != 0) {
            failures++;
            (void)printf("FAIL app_display[%u]:%d\n", (unsigned)index, result);
        }
    }
    (void)printf("Display: %u/%u PASS\n", (unsigned)(sizeof(tests) / sizeof(tests[0]) - failures),
        (unsigned)(sizeof(tests) / sizeof(tests[0])));
    return (failures == 0U) ? 0 : 1;
}
