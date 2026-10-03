/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_app_display.c
 * @brief 验证 Display Task 启动、合并刷新和故障隔离
 * @author YaoQian Wang
 * @date 2026-09-06
 * @version V1.0
 *
 *****************************************************************************/

#include "app_display.h"
#include "platform_graphics.h"
#include "service_log.h"

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#define TEST_ASSERT(condition)       \
    do {                             \
        if (!(condition)) {          \
            return __LINE__;         \
        }                            \
    } while (0)

#define TEST_MESSAGE_CAPACITY (16U)
#define TEST_DRAW_CAPACITY    (64U)
#define TEST_TEXT_CAPACITY    (32U)

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
    platform_error_t touchInitResult;
    platform_error_t touchReadResult;
    platform_error_t notifyWaitResult;
    uint32_t touchInitCount;
    uint32_t touchReadCount;
    uint32_t busInitCount;
    uint32_t notifyCount;
    uint32_t pendingFlags;
    app_display_message_t messages[TEST_MESSAGE_CAPACITY];
    char drawnText[TEST_DRAW_CAPACITY][TEST_TEXT_CAPACITY];
    uint16_t drawnX[TEST_DRAW_CAPACITY];
    uint16_t drawnY[TEST_DRAW_CAPACITY];
    uint16_t fillRectX[TEST_DRAW_CAPACITY];
    uint16_t fillRectWidth[TEST_DRAW_CAPACITY];
    platform_error_t initResult;
    platform_error_t drawResult;
    uint32_t messageReadIndex;
    uint32_t messageCount;
    uint32_t drawCount;
    uint32_t fillCount;
    uint32_t fillRectCount;
    uint32_t initCount;
    uint32_t deinitCount;
    uint32_t backlightOnCount;
    uint32_t backlightOffCount;
    uint32_t delayCount;
    uint32_t lastDelayMs;
    uint32_t receiveCount;
    uint32_t receiveTimeouts[TEST_MESSAGE_CAPACITY];
} fake_display_runtime_t;

static fake_display_runtime_t g_fakeRuntime;

static void fake_runtime_reset(void)
{
    (void)memset(&g_fakeRuntime, 0, sizeof(g_fakeRuntime));
    g_fakeRuntime.queue.native = &g_fakeRuntime.queue;
    g_fakeRuntime.thread.native = &g_fakeRuntime.thread;
    g_fakeRuntime.notifyWaitResult = PLATFORM_ERR_EMPTY;
    g_fakeRuntime.initResult = PLATFORM_ERR_OK;
    g_fakeRuntime.drawResult = PLATFORM_ERR_OK;
}

static app_display_t create_display(
    platform_st7789_t *display,
    platform_spi_bus_t *spiBus)
{
    app_display_t appDisplay = APP_DISPLAY_INITIALIZER;
    app_display_config_t config = {
        .display = display,
        .spiBus = spiBus,
        .queue = &g_fakeRuntime.queue,
        .touch = &g_fakeRuntime.touch,
        .touchI2c = &g_fakeRuntime.touchI2c,
        .touchScl = &g_fakeRuntime.touchScl,
        .touchSda = &g_fakeRuntime.touchSda,
        .touchReset = &g_fakeRuntime.touchReset,
        .thread = &g_fakeRuntime.thread
    };

    display->width = 240U;
    display->height = 280U;
    (void)app_display_init(&appDisplay, &config);
    return appDisplay;
}

static void fake_enqueue(app_display_message_t message)
{
    g_fakeRuntime.messages[g_fakeRuntime.messageCount++] = message;
}

static platform_bool_t fake_drew_text(const char *text)
{
    uint32_t index;

    for (index = 0U; index < g_fakeRuntime.drawCount; index++) {
        if (strcmp(g_fakeRuntime.drawnText[index], text) == 0) {
            return PLATFORM_TRUE;
        }
    }
    return PLATFORM_FALSE;
}

static platform_bool_t fake_drew_text_at(
    const char *text,
    uint16_t x,
    uint16_t y)
{
    uint32_t index;

    for (index = 0U; index < g_fakeRuntime.drawCount; index++) {
        if ((g_fakeRuntime.drawnX[index] == x) &&
            (g_fakeRuntime.drawnY[index] == y) &&
            (strcmp(g_fakeRuntime.drawnText[index], text) == 0)) {
            return PLATFORM_TRUE;
        }
    }
    return PLATFORM_FALSE;
}

static int test_init_only_binds_dependencies(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t spiBus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;

    fake_runtime_reset();
    appDisplay = create_display(&display, &spiBus);

    TEST_ASSERT(appDisplay.context.initialized == PLATFORM_TRUE);
    TEST_ASSERT(appDisplay.context.available == PLATFORM_FALSE);
    TEST_ASSERT(g_fakeRuntime.initCount == 0U);
    TEST_ASSERT(g_fakeRuntime.drawCount == 0U);
    TEST_ASSERT(g_fakeRuntime.delayCount == 0U);

    return 0;
}

static int test_start_draws_boot_then_main_with_initial_placeholders(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t spiBus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;

    fake_runtime_reset();
    appDisplay = create_display(&display, &spiBus);

    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.initCount == 1U);
    TEST_ASSERT(g_fakeRuntime.backlightOnCount == 1U);
    TEST_ASSERT(g_fakeRuntime.delayCount == 1U);
    TEST_ASSERT(g_fakeRuntime.lastDelayMs == PROJECT_DISPLAY_BOOT_DURATION_MS);
    TEST_ASSERT(fake_drew_text("SENSOR MONITOR") == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text("STM32F4 + RTOS") == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text("STARTING...") == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text("--") == PLATFORM_TRUE);
    TEST_ASSERT(appDisplay.context.available == PLATFORM_TRUE);

    return 0;
}

static int test_main_ui_uses_rounded_corner_safe_horizontal_layout(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t spiBus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;
    uint32_t index;

    fake_runtime_reset();
    appDisplay = create_display(&display, &spiBus);

    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(fake_drew_text_at("SENSOR MONITOR", 64U, 0U) == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text_at("STATE :", 56U, 32U) == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text_at("ENVIRONMENT", 76U, 64U) == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text_at("TEMP  :", 56U, 80U) == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text_at("HUM   :", 56U, 96U) == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text_at("ACCEL (g)", 84U, 128U) == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text_at("X     :", 56U, 144U) == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text_at("GYRO (dps)", 80U, 208U) == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text_at("Z     :", 56U, 256U) == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text_at("--", 120U, 32U) == PLATFORM_TRUE);
    TEST_ASSERT(g_fakeRuntime.fillRectCount == 9U);
    for (index = 0U; index < g_fakeRuntime.fillRectCount; index++) {
        TEST_ASSERT(g_fakeRuntime.fillRectX[index] == 120U);
        TEST_ASSERT(g_fakeRuntime.fillRectWidth[index] == 80U);
    }

    return 0;
}

static int test_queue_coalescing_renders_only_latest_cache(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t spiBus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;
    app_display_message_t message = {0};

    fake_runtime_reset();
    appDisplay = create_display(&display, &spiBus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    g_fakeRuntime.drawCount = 0U;
    g_fakeRuntime.fillRectCount = 0U;
    g_fakeRuntime.receiveCount = 0U;

    message.type = APP_DISPLAY_MESSAGE_SYSTEM_STATE;
    message.payload.systemState = APP_CONTROL_STATE_STOPPED;
    fake_enqueue(message);
    message.payload.systemState = APP_CONTROL_STATE_RUNNING;
    fake_enqueue(message);
    message.type = APP_DISPLAY_MESSAGE_MEASUREMENT;
    message.payload.measurement.environment.temperatureC = 23.4F;
    message.payload.measurement.environment.humidityPercent = 45.6F;
    message.payload.measurement.motion.accelXG = 0.012F;
    message.payload.measurement.motion.accelYG = -0.034F;
    message.payload.measurement.motion.accelZG = 0.998F;
    message.payload.measurement.motion.gyroXDps = 12.3F;
    message.payload.measurement.motion.gyroYDps = -0.8F;
    message.payload.measurement.motion.gyroZDps = 1.2F;
    fake_enqueue(message);

    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.receiveTimeouts[0] == 5U);
    TEST_ASSERT(g_fakeRuntime.receiveTimeouts[1] == PLATFORM_OS_NO_WAIT);
    TEST_ASSERT(appDisplay.context.systemState == APP_CONTROL_STATE_RUNNING);
    TEST_ASSERT(appDisplay.context.latestMeasurement.environment.temperatureC > 23.39F);
    TEST_ASSERT(appDisplay.context.latestMeasurement.environment.temperatureC < 23.41F);
    TEST_ASSERT(g_fakeRuntime.fillRectCount == 9U);
    TEST_ASSERT(fake_drew_text("RUNNING") == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text("+23.4 C") == PLATFORM_TRUE);
    TEST_ASSERT(fake_drew_text("+0.012") == PLATFORM_TRUE);

    return 0;
}

static int test_start_drains_pending_before_first_dynamic_render(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t spiBus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;
    app_display_message_t message = {0};

    fake_runtime_reset();
    appDisplay = create_display(&display, &spiBus);
    message.type = APP_DISPLAY_MESSAGE_SYSTEM_STATE;
    message.payload.systemState = APP_CONTROL_STATE_RUNNING;
    fake_enqueue(message);
    message.type = APP_DISPLAY_MESSAGE_MEASUREMENT;
    message.payload.measurement.environment.temperatureC = 21.5F;
    fake_enqueue(message);

    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.systemState == APP_CONTROL_STATE_RUNNING);
    TEST_ASSERT(appDisplay.context.measurementValid == PLATFORM_TRUE);
    TEST_ASSERT(g_fakeRuntime.fillRectCount == 9U);
    TEST_ASSERT(fake_drew_text("--") == PLATFORM_FALSE);
    TEST_ASSERT(fake_drew_text("+21.5 C") == PLATFORM_TRUE);

    return 0;
}

static int test_stopped_state_retains_latest_measurement(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t spiBus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;
    app_display_message_t message = {0};

    fake_runtime_reset();
    appDisplay = create_display(&display, &spiBus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    message.type = APP_DISPLAY_MESSAGE_MEASUREMENT;
    message.payload.measurement.environment.temperatureC = 19.5F;
    fake_enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);

    g_fakeRuntime.drawCount = 0U;
    g_fakeRuntime.fillRectCount = 0U;
    message.type = APP_DISPLAY_MESSAGE_SYSTEM_STATE;
    message.payload.systemState = APP_CONTROL_STATE_STOPPED;
    fake_enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.latestMeasurement.environment.temperatureC == 19.5F);
    TEST_ASSERT(g_fakeRuntime.fillRectCount == 1U);
    TEST_ASSERT(fake_drew_text("STOPPED") == PLATFORM_TRUE);

    return 0;
}

static int test_failures_remain_inside_display_subsystem(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t spiBus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;
    app_display_message_t message = {0};

    fake_runtime_reset();
    g_fakeRuntime.initResult = PLATFORM_ERR_IO;
    appDisplay = create_display(&display, &spiBus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.available == PLATFORM_FALSE);

    message.type = APP_DISPLAY_MESSAGE_MEASUREMENT;
    message.payload.measurement.environment.temperatureC = 20.0F;
    fake_enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.measurementValid == PLATFORM_TRUE);

    fake_runtime_reset();
    appDisplay = create_display(&display, &spiBus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    g_fakeRuntime.drawResult = PLATFORM_ERR_IO;
    message.type = APP_DISPLAY_MESSAGE_SYSTEM_STATE;
    message.payload.systemState = APP_CONTROL_STATE_RUNNING;
    fake_enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.available == PLATFORM_TRUE);
    TEST_ASSERT(appDisplay.context.stateDirty == PLATFORM_TRUE);
    TEST_ASSERT(appDisplay.statistics.renderFailureCount == 1U);

    g_fakeRuntime.drawResult = PLATFORM_ERR_OK;
    fake_enqueue(message);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.stateDirty == PLATFORM_FALSE);

    return 0;
}

platform_error_t platform_st7789_init(
    platform_st7789_t *display,
    platform_spi_bus_t *spiBus)
{
    (void)spiBus;
    g_fakeRuntime.initCount++;
    if (g_fakeRuntime.initResult == PLATFORM_ERR_OK) {
        display->initialized = PLATFORM_TRUE;
    }
    return g_fakeRuntime.initResult;
}

platform_error_t platform_st7789_deinit(platform_st7789_t *display)
{
    display->initialized = PLATFORM_FALSE;
    g_fakeRuntime.deinitCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_st7789_backlight_on(platform_st7789_t *display)
{
    (void)display;
    g_fakeRuntime.backlightOnCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_st7789_backlight_off(platform_st7789_t *display)
{
    (void)display;
    g_fakeRuntime.backlightOffCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_st7789_fill(
    platform_st7789_t *display,
    uint16_t color)
{
    (void)display;
    (void)color;
    g_fakeRuntime.fillCount++;
    return g_fakeRuntime.drawResult;
}

platform_error_t platform_st7789_fill_rect(
    platform_st7789_t *display,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color)
{
    uint32_t index = g_fakeRuntime.fillRectCount++;

    (void)display;
    (void)y;
    (void)height;
    (void)color;
    TEST_ASSERT(index < TEST_DRAW_CAPACITY);
    g_fakeRuntime.fillRectX[index] = x;
    g_fakeRuntime.fillRectWidth[index] = width;
    return g_fakeRuntime.drawResult;
}

platform_error_t platform_graphics_draw_string(
    platform_st7789_t *display,
    uint16_t x,
    uint16_t y,
    const char_t *text,
    const platform_font_t *font,
    uint16_t foreground,
    uint16_t background)
{
    uint32_t index = g_fakeRuntime.drawCount++;

    (void)display;
    (void)font;
    (void)foreground;
    (void)background;
    TEST_ASSERT(index < TEST_DRAW_CAPACITY);
    g_fakeRuntime.drawnX[index] = x;
    g_fakeRuntime.drawnY[index] = y;
    (void)strncpy(g_fakeRuntime.drawnText[index], text, TEST_TEXT_CAPACITY - 1U);
    return g_fakeRuntime.drawResult;
}

platform_error_t platform_queue_receive(
    platform_queue_t *queue,
    void *item,
    uint32_t timeoutMs)
{
    TEST_ASSERT(queue == &g_fakeRuntime.queue);
    g_fakeRuntime.receiveTimeouts[g_fakeRuntime.receiveCount++] = timeoutMs;
    if (g_fakeRuntime.messageReadIndex >= g_fakeRuntime.messageCount) {
        return (timeoutMs == PLATFORM_OS_NO_WAIT) ?
            PLATFORM_ERR_EMPTY : PLATFORM_ERR_TIMEOUT;
    }
    *(app_display_message_t *)item =
        g_fakeRuntime.messages[g_fakeRuntime.messageReadIndex++];
    return PLATFORM_ERR_OK;
}

platform_error_t platform_time_delay_ms(uint32_t delayMs)
{
    g_fakeRuntime.delayCount++;
    g_fakeRuntime.lastDelayMs = delayMs;
    return PLATFORM_ERR_OK;
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

static int test_touch_start_reads_once_and_remains_independent(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;

    fake_runtime_reset();
    appDisplay = create_display(&display, &bus);
    g_fakeRuntime.initResult = PLATFORM_ERR_IO;
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.available == PLATFORM_FALSE);
    TEST_ASSERT(appDisplay.context.touchAvailable == PLATFORM_TRUE);
    TEST_ASSERT(g_fakeRuntime.busInitCount == 1U && g_fakeRuntime.touchInitCount == 1U);
    TEST_ASSERT(g_fakeRuntime.touchReadCount == 1U);
    return 0;
}

static int test_touch_init_failure_keeps_lcd_running(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;

    fake_runtime_reset();
    appDisplay = create_display(&display, &bus);
    g_fakeRuntime.touchInitResult = PLATFORM_ERR_NOT_FOUND;
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.available == PLATFORM_TRUE);
    TEST_ASSERT(appDisplay.context.touchAvailable == PLATFORM_FALSE);
    TEST_ASSERT(g_fakeRuntime.touchReadCount == 0U);
    TEST_ASSERT(appDisplay.context.touchLastError == PLATFORM_ERR_NOT_FOUND);
    return 0;
}

static int test_irq_before_thread_ready_is_ignored(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;

    fake_runtime_reset();
    appDisplay = create_display(&display, &bus);
    g_fakeRuntime.thread.native = NULL;
    app_display_touch_irq_from_isr(&appDisplay);
    TEST_ASSERT(g_fakeRuntime.notifyCount == 0U && g_fakeRuntime.touchReadCount == 0U);
    g_fakeRuntime.thread.native = &g_fakeRuntime.thread;
    app_display_touch_irq_from_isr(&appDisplay);
    app_display_touch_irq_from_isr(&appDisplay);
    TEST_ASSERT(g_fakeRuntime.notifyCount == 2U && g_fakeRuntime.pendingFlags == 1U);
    TEST_ASSERT(g_fakeRuntime.touchReadCount == 0U);
    return 0;
}

static int test_empty_queue_services_touch_and_failure_releases_cache(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;

    fake_runtime_reset();
    appDisplay = create_display(&display, &bus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    g_fakeRuntime.sample.pressed = PLATFORM_TRUE;
    g_fakeRuntime.sample.x = 123U;
    g_fakeRuntime.sample.y = 234U;
    app_display_touch_irq_from_isr(&appDisplay);
    app_display_touch_irq_from_isr(&appDisplay);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.touchSample.pressed == PLATFORM_TRUE);
    TEST_ASSERT(appDisplay.context.touchSample.x == 123U);
    TEST_ASSERT(g_fakeRuntime.touchReadCount == 2U);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.touchReadCount == 2U);
    g_fakeRuntime.touchReadResult = PLATFORM_ERR_IO;
    app_display_touch_irq_from_isr(&appDisplay);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(appDisplay.context.touchSample.pressed == PLATFORM_FALSE);
    TEST_ASSERT(appDisplay.context.touchLastError == PLATFORM_ERR_IO);
    TEST_ASSERT(appDisplay.context.available == PLATFORM_TRUE);
    return 0;
}

static int test_message_flood_is_bounded_and_touch_runs_each_round(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    app_display_t appDisplay;
    app_display_message_t message = {0};
    uint32_t index;

    fake_runtime_reset();
    appDisplay = create_display(&display, &bus);
    TEST_ASSERT(app_display_start(&appDisplay) == PLATFORM_ERR_OK);
    message.type = APP_DISPLAY_MESSAGE_SYSTEM_STATE;
    message.payload.systemState = APP_CONTROL_STATE_RUNNING;
    for (index = 0U; index < 8U; index++) {
        fake_enqueue(message);
    }
    app_display_touch_irq_from_isr(&appDisplay);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.messageReadIndex == 4U);
    TEST_ASSERT(g_fakeRuntime.touchReadCount == 2U);
    TEST_ASSERT(app_display_run_once(&appDisplay) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_fakeRuntime.messageReadIndex == 8U);
    return 0;
}

platform_error_t platform_i2c_init(platform_i2c_t *i2c, const char *name,
    platform_gpio_t *scl, platform_gpio_t *sda)
{
    TEST_ASSERT(i2c == &g_fakeRuntime.touchI2c);
    TEST_ASSERT(scl == &g_fakeRuntime.touchScl && sda == &g_fakeRuntime.touchSda);
    TEST_ASSERT(strcmp(name, "touch_soft_i2c") == 0);
    g_fakeRuntime.busInitCount++;
    i2c->initialized = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_cst816t_init(platform_cst816t_t *touch,
    platform_i2c_t *i2c, platform_gpio_t *reset)
{
    TEST_ASSERT(touch == &g_fakeRuntime.touch && i2c == &g_fakeRuntime.touchI2c);
    TEST_ASSERT(reset == &g_fakeRuntime.touchReset);
    g_fakeRuntime.touchInitCount++;
    touch->chipId = 0xB5U;
    touch->firmwareVersion = 0x12U;
    return g_fakeRuntime.touchInitResult;
}

platform_error_t platform_cst816t_read_sample(platform_cst816t_t *touch,
    platform_cst816t_sample_t *sample)
{
    TEST_ASSERT(touch == &g_fakeRuntime.touch);
    g_fakeRuntime.touchReadCount++;
    if (g_fakeRuntime.touchReadResult == PLATFORM_ERR_OK) {
        *sample = g_fakeRuntime.sample;
    }
    return g_fakeRuntime.touchReadResult;
}

platform_error_t platform_notify_set_from_isr(platform_thread_t *thread, uint32_t flags)
{
    TEST_ASSERT(thread == &g_fakeRuntime.thread && thread->native != NULL);
    TEST_ASSERT(flags == 1U);
    g_fakeRuntime.notifyCount++;
    g_fakeRuntime.pendingFlags |= flags;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_notify_wait(uint32_t flags, platform_bool_t waitAll,
    platform_bool_t clearOnExit, uint32_t timeoutMs, uint32_t *receivedFlags)
{
    TEST_ASSERT(flags == 1U && waitAll == PLATFORM_FALSE && clearOnExit == PLATFORM_TRUE);
    TEST_ASSERT(timeoutMs == PLATFORM_OS_NO_WAIT);
    if (g_fakeRuntime.pendingFlags != 0U) {
        *receivedFlags = g_fakeRuntime.pendingFlags;
        g_fakeRuntime.pendingFlags = 0U;
        return PLATFORM_ERR_OK;
    }
    return g_fakeRuntime.notifyWaitResult;
}

platform_error_t platform_time_get_ms(uint32_t *timeMs)
{
    *timeMs = 100U;
    return PLATFORM_ERR_OK;
}

int main(void)
{
    static int (*const tests[])(void) = {
        test_init_only_binds_dependencies,
        test_start_draws_boot_then_main_with_initial_placeholders,
        test_main_ui_uses_rounded_corner_safe_horizontal_layout,
        test_queue_coalescing_renders_only_latest_cache,
        test_start_drains_pending_before_first_dynamic_render,
        test_stopped_state_retains_latest_measurement,
        test_failures_remain_inside_display_subsystem,
        test_touch_start_reads_once_and_remains_independent,
        test_touch_init_failure_keeps_lcd_running,
        test_irq_before_thread_ready_is_ignored,
        test_empty_queue_services_touch_and_failure_releases_cache,
        test_message_flood_is_bounded_and_touch_runs_each_round
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
    (void)printf("Display: %u/12 PASS\n", (unsigned)(12U - failures));
    return (failures == 0U) ? 0 : 1;
}
