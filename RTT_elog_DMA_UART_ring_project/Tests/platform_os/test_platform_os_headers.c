/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_os_headers.c
 * @brief 验证 Platform OS 公共头文件不依赖 CMSIS 或 FreeRTOS 头文件。
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *****************************************************************************/

#include "platform_os.h"

int main(void)
{
    platform_thread_t thread = PLATFORM_OS_OBJECT_INITIALIZER;
    platform_mutex_t mutex = PLATFORM_OS_OBJECT_INITIALIZER;
    platform_semaphore_t semaphore = PLATFORM_OS_OBJECT_INITIALIZER;
    platform_queue_t queue = PLATFORM_OS_OBJECT_INITIALIZER;
    platform_timer_t timer = PLATFORM_OS_OBJECT_INITIALIZER;

    return ((thread.native != (void *)0) ||
            (mutex.native != (void *)0) ||
            (semaphore.native != (void *)0) ||
            (queue.native != (void *)0) ||
            (timer.native != (void *)0));
}
