#ifndef PLATFORM_THREAD_H
#define PLATFORM_THREAD_H

#include "platform_os_types.h"

typedef enum {
    PLATFORM_THREAD_PRIORITY_LOW = 0,
    PLATFORM_THREAD_PRIORITY_BELOW_NORMAL,
    PLATFORM_THREAD_PRIORITY_NORMAL,
    PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL,
    PLATFORM_THREAD_PRIORITY_HIGH
} platform_thread_priority_t;

typedef void (*platform_thread_entry_t)(void *argument);

typedef struct {
    const char *name;
    platform_thread_entry_t entry;
    void *argument;
    uint32_t stackSizeBytes;
    platform_thread_priority_t priority;
} platform_thread_config_t;

platform_error_t platform_thread_create(platform_thread_t *thread, const platform_thread_config_t *config);
platform_error_t platform_thread_get_current(platform_thread_t *thread);
platform_error_t platform_thread_set_priority(platform_thread_t *thread, platform_thread_priority_t priority);
platform_error_t platform_thread_get_priority(const platform_thread_t *thread, platform_thread_priority_t *priority);
platform_error_t platform_thread_suspend(platform_thread_t *thread);
platform_error_t platform_thread_resume(platform_thread_t *thread);
platform_error_t platform_thread_terminate(platform_thread_t *thread);
platform_error_t platform_thread_yield(void);

#endif
