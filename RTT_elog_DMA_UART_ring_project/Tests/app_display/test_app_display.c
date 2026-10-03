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
#include "ui_smoke.h"

#include <stdio.h>
#include <string.h>

#define TEST_ASSERT(condition) do { if (!(condition)) { return __LINE__; } } while (0)
#define TEST_MESSAGE_CAPACITY (16U)

typedef struct
{
    platform_queue_t queue;
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

platform_error_t ui_smoke_create(void)
{
    g_fake.uiCreateCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_queue_receive(platform_queue_t *queue, void *item,
                                        uint32_t timeoutMs)
{
    TEST_ASSERT(queue == &g_fake.queue);
    g_fake.receiveTimeouts[g_fake.receiveCount++] = timeoutMs;
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
    *timeMs = 100U;
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

int main(void)
{
    static int (*const tests[])(void) = {
        test_start_initializes_gui_after_lcd,
        test_lcd_failure_keeps_touch_and_cache_running,
        test_gui_failure_disables_lcd,
        test_touch_failure_still_services_gui,
        test_queue_budget_retains_latest_cache_and_runs_gui,
        test_empty_queue_services_touch_and_gui,
        test_flush_failure_is_local
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
    (void)printf("Display: %u/7 PASS\n", (unsigned)(7U - failures));
    return (failures == 0U) ? 0 : 1;
}
