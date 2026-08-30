/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_os_types.h
 * @brief 定义 Platform OS 的公共对象、超时和通知类型。
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *****************************************************************************/

#ifndef PLATFORM_OS_TYPES_H
#define PLATFORM_OS_TYPES_H

#include "platform_error.h"

/* 所有 Platform OS 对象在创建前或删除后均使用此初始值。 */
#define PLATFORM_OS_OBJECT_INITIALIZER    { (void *)0 }
#define PLATFORM_OS_NO_WAIT               (0U)
#define PLATFORM_OS_WAIT_FOREVER          (0xFFFFFFFFU)
#define PLATFORM_NOTIFY_VALID_MASK        (0x7FFFFFFFU)

/* native 仅由 Impl 使用，Platform 公共层不解释其具体类型。 */
typedef struct {
    void *native;
} platform_thread_t;

typedef struct {
    void *native;
} platform_mutex_t;

typedef struct {
    void *native;
} platform_semaphore_t;

typedef struct {
    void *native;
} platform_queue_t;

typedef struct {
    void *native;
} platform_timer_t;

#endif
