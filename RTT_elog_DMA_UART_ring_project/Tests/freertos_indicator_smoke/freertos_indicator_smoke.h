/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file freertos_indicator_smoke.h
 * @brief 临时 FreeRTOS 目标板提示灯 Smoke Test 入口。
 * @author Codex
 * @date 2026-09-03
 * @version V1.0
 *
 *****************************************************************************/

#ifndef FREERTOS_INDICATOR_SMOKE_H
#define FREERTOS_INDICATOR_SMOKE_H

//******************************** Functions ********************************//
/**
 * @brief 在已启动的 FreeRTOS Task Context 中运行一次临时提示灯 Smoke Test。
 * @return 无。
 * @note 验证结束后 LED 保持 OFF；删除本头文件、源文件、Keil smoke group 和
 *       freertos.c 的调用即可完整移除该临时路径。
 */
void freertos_indicator_smoke_run(void);
//******************************** Functions ********************************//

#endif
