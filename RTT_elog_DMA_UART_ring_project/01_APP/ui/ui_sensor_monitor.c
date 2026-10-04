/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 * All Rights Reserved.
 * @file ui_sensor_monitor.c
 * @brief 适配原生 Guider 页面与 APP 数据、控制合同。
 * @author YaoQian Wang
 * @date 2026-10-04
 * @version V1.0
 *****************************************************************************/
#include "ui_sensor_monitor.h"
#include "generated/gg_utils.h"
#include <stdio.h>

static gg_ui_t g_monitorUI;
static app_control_event_handler_t g_handler;
static void *g_handlerContext;
static app_control_ui_status_t g_status;
static platform_bool_t g_requestPending;
static platform_bool_t g_initialized;

static void ui_sensor_monitor_feedback(const char *text, uint32_t color)
{
    lv_label_set_text(g_monitorUI.screen.label_feedback, text);
    lv_obj_set_style_text_color(g_monitorUI.screen.label_feedback, lv_color_hex(color), LV_PART_MAIN);
}

static void ui_sensor_monitor_button_state(lv_obj_t *button, lv_obj_t *label, platform_bool_t disabled)
{
    if (disabled == PLATFORM_TRUE) {
        lv_obj_add_state(button, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(button, LV_STATE_DISABLED);
    }
    lv_obj_set_style_text_color(label, lv_color_hex(disabled == PLATFORM_TRUE ? 0x8495A8 : 0xF2F6FA), LV_PART_MAIN);
}

static void ui_sensor_monitor_clicked(lv_event_t *event)
{
    lv_obj_t *button = lv_event_get_target_obj(event);
    app_ctrl_event_t command;

    if (g_requestPending == PLATFORM_TRUE || g_status.onceActive == PLATFORM_TRUE ||
        lv_obj_has_state(button, LV_STATE_DISABLED)) {
        return;
    }
    command = button == g_monitorUI.screen.button_once ? APP_CTRL_SAMPLE_ONCE :
        (g_status.state == APP_CONTROL_STATE_RUNNING ? APP_CTRL_STOP : APP_CTRL_START);
    if (g_handler(g_handlerContext, command) != PLATFORM_ERR_OK) {
        ui_sensor_monitor_show_request_failure();
    }
}

platform_error_t ui_sensor_monitor_create(app_control_event_handler_t handler, void *context)
{
    lv_obj_t *oldScreen;

    if (handler == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (g_initialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }
    if (lv_display_get_default() == NULL) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }
    g_handler = handler;
    g_handlerContext = context;
    oldScreen = lv_screen_active();
    /* 生成器已完整指定页面样式，避免默认主题附加状态过渡和动态分配。 */
    lv_display_set_theme(lv_display_get_default(), NULL);
    setup_ui(&g_monitorUI);
    /* 原生源已同步修正；避免改写生成文件，重新生成后此适配仍一致。 */
    lv_obj_set_y(g_monitorUI.screen.container_temperature_label_temperature_value, 20);
    lv_obj_set_height(g_monitorUI.screen.container_temperature_label_temperature_value, 29);
    lv_obj_set_y(g_monitorUI.screen.container_humidity_label_humidity_value, 20);
    lv_obj_set_height(g_monitorUI.screen.container_humidity_label_humidity_value, 29);
    lv_obj_delete(oldScreen);
    /* 禁用使用实色，避免默认半透明状态需要额外的整块离屏缓冲。 */
    lv_obj_set_style_opa(g_monitorUI.screen.button_start_stop, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(g_monitorUI.screen.button_start_stop, lv_color_hex(0x263544), LV_STATE_DISABLED);
    lv_obj_set_style_opa(g_monitorUI.screen.button_once, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(g_monitorUI.screen.button_once, lv_color_hex(0x263544), LV_STATE_DISABLED);
    lv_obj_add_event_cb(g_monitorUI.screen.button_start_stop, ui_sensor_monitor_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(g_monitorUI.screen.button_once, ui_sensor_monitor_clicked, LV_EVENT_CLICKED, NULL);
    g_initialized = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}

void ui_sensor_monitor_update_control(const app_control_ui_status_t *status, platform_bool_t requestPending)
{
    platform_bool_t stateChanged = g_status.state != status->state ? PLATFORM_TRUE : PLATFORM_FALSE;
    platform_bool_t busy = status->onceActive == PLATFORM_TRUE || requestPending == PLATFORM_TRUE ?
        PLATFORM_TRUE : PLATFORM_FALSE;

    g_status = *status;
    g_requestPending = requestPending;
    lv_label_set_text(g_monitorUI.screen.label_state,
        status->state == APP_CONTROL_STATE_RUNNING ? "RUNNING" : "STOPPED");
    lv_obj_set_style_text_color(g_monitorUI.screen.label_state,
        lv_color_hex(status->state == APP_CONTROL_STATE_RUNNING ? 0x76E2A5 : 0x9BAEC2), LV_PART_MAIN);
    lv_label_set_text(g_monitorUI.screen.button_start_stop_button_start_stop_label,
        status->state == APP_CONTROL_STATE_RUNNING ? "STOP" : "START");
    lv_obj_set_style_bg_color(g_monitorUI.screen.button_start_stop,
        lv_color_hex(status->state == APP_CONTROL_STATE_RUNNING ? 0xB91C1C : 0x15803D), LV_STATE_DEFAULT);
    lv_label_set_text(g_monitorUI.screen.button_once_button_once_label,
        status->onceActive == PLATFORM_TRUE ? "SAMPLING" : "ONCE");
    ui_sensor_monitor_button_state(g_monitorUI.screen.button_start_stop,
        g_monitorUI.screen.button_start_stop_button_start_stop_label, busy);
    ui_sensor_monitor_button_state(g_monitorUI.screen.button_once,
        g_monitorUI.screen.button_once_button_once_label,
        busy == PLATFORM_TRUE || status->state == APP_CONTROL_STATE_RUNNING ? PLATFORM_TRUE : PLATFORM_FALSE);
    if (status->onceActive == PLATFORM_TRUE) {
        ui_sensor_monitor_feedback("Sampling...", 0xF4C46A);
    } else if (status->responseValid == PLATFORM_TRUE && status->requestResult != PLATFORM_ERR_OK) {
        ui_sensor_monitor_show_request_failure();
    } else if (status->responseValid == PLATFORM_TRUE && status->response == APP_CONTROL_RESPONSE_ACQUISITION_FAILED) {
        ui_sensor_monitor_show_sample_failure();
    } else if (status->responseValid == PLATFORM_TRUE && status->response == APP_CONTROL_RESPONSE_OK_ONCE) {
        ui_sensor_monitor_feedback("Sample OK", 0x76E2A5);
    } else if (status->responseValid == PLATFORM_TRUE && status->response == APP_CONTROL_RESPONSE_BUSY) {
        ui_sensor_monitor_feedback("Busy", 0xF4C46A);
    } else if (stateChanged == PLATFORM_TRUE ||
        (status->responseValid == PLATFORM_TRUE &&
         (status->response == APP_CONTROL_RESPONSE_OK_START || status->response == APP_CONTROL_RESPONSE_OK_STOP ||
          status->response == APP_CONTROL_RESPONSE_ALREADY_RUNNING || status->response == APP_CONTROL_RESPONSE_ALREADY_STOPPED))) {
        ui_sensor_monitor_feedback(status->state == APP_CONTROL_STATE_RUNNING ? "Live sampling" : "Stopped", 0x9BAEC2);
    }
}

void ui_sensor_monitor_update_measurement(const app_acquisition_data_t *measurement)
{
    char text[16];

    (void)snprintf(text, sizeof(text), "%.1f", (double)measurement->environment.temperatureC);
    lv_label_set_text(g_monitorUI.screen.container_temperature_label_temperature_value, text);
    (void)snprintf(text, sizeof(text), "%.1f", (double)measurement->environment.humidityPercent);
    lv_label_set_text(g_monitorUI.screen.container_humidity_label_humidity_value, text);
    (void)snprintf(text, sizeof(text), "%+.2f", (double)measurement->motion.accelXG);
    lv_label_set_text(g_monitorUI.screen.container_acceleration_label_acceleration_x, text);
    (void)snprintf(text, sizeof(text), "%+.2f", (double)measurement->motion.accelYG);
    lv_label_set_text(g_monitorUI.screen.container_acceleration_label_acceleration_y, text);
    (void)snprintf(text, sizeof(text), "%+.2f", (double)measurement->motion.accelZG);
    lv_label_set_text(g_monitorUI.screen.container_acceleration_label_acceleration_z, text);
    (void)snprintf(text, sizeof(text), "%+.1f", (double)measurement->motion.gyroXDps);
    lv_label_set_text(g_monitorUI.screen.container_gyroscope_label_gyroscope_x, text);
    (void)snprintf(text, sizeof(text), "%+.1f", (double)measurement->motion.gyroYDps);
    lv_label_set_text(g_monitorUI.screen.container_gyroscope_label_gyroscope_y, text);
    (void)snprintf(text, sizeof(text), "%+.1f", (double)measurement->motion.gyroZDps);
    lv_label_set_text(g_monitorUI.screen.container_gyroscope_label_gyroscope_z, text);
    if (g_status.onceActive != PLATFORM_TRUE) {
        ui_sensor_monitor_feedback(g_status.state == APP_CONTROL_STATE_RUNNING ? "Live sampling" : "Sample OK", 0x76E2A5);
    }
}

void ui_sensor_monitor_show_request_failure(void)
{
    ui_sensor_monitor_feedback("Request failed", 0xF4C46A);
}

void ui_sensor_monitor_show_sample_failure(void)
{
    ui_sensor_monitor_feedback("Sample failed", 0xF4C46A);
}
