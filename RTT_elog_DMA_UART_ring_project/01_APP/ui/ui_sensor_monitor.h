/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 * All Rights Reserved.
 * @file ui_sensor_monitor.h
 * @brief 定义 Guider 传感器页面的数据更新和控制绑定接口。
 * @author YaoQian Wang
 * @date 2026-10-04
 * @version V1.0
 *****************************************************************************/
#ifndef UI_SENSOR_MONITOR_H
#define UI_SENSOR_MONITOR_H

#include "app_ipc_types.h"

/** @brief 在 Display Task 创建页面并绑定非阻塞请求回调。 */
platform_error_t ui_sensor_monitor_create(app_control_event_handler_t handler, void *context);
/** @brief 以 Control 已确认状态更新按钮及操作结果。 */
void ui_sensor_monitor_update_control(const app_control_ui_status_t *status, platform_bool_t requestPending);
/** @brief 用完整成功快照更新全部八项数值。 */
void ui_sensor_monitor_update_measurement(const app_acquisition_data_t *measurement);
/** @brief 显示请求失败，不改变已确认状态。 */
void ui_sensor_monitor_show_request_failure(void);
/** @brief 显示采样失败，保留整组历史数值。 */
void ui_sensor_monitor_show_sample_failure(void);

#endif
