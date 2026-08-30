#ifndef PLATFORM_MUTEX_H
#define PLATFORM_MUTEX_H

#include "platform_os_types.h"

typedef enum {
    PLATFORM_MUTEX_NORMAL = 0,
    PLATFORM_MUTEX_RECURSIVE
} platform_mutex_type_t;

platform_error_t platform_mutex_create(platform_mutex_t *mutex, platform_mutex_type_t type);
platform_error_t platform_mutex_lock(platform_mutex_t *mutex, uint32_t timeoutMs);
platform_error_t platform_mutex_unlock(platform_mutex_t *mutex);
platform_error_t platform_mutex_delete(platform_mutex_t *mutex);

#endif
