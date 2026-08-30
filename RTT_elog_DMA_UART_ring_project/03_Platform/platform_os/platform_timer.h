#ifndef PLATFORM_TIMER_H
#define PLATFORM_TIMER_H

#include "platform_os_types.h"

/* Timer callbacks execute in RTOS Timer Task context, never in ISR context. */
typedef enum { PLATFORM_TIMER_ONCE = 0, PLATFORM_TIMER_PERIODIC } platform_timer_type_t;
typedef void (*platform_timer_callback_t)(void *argument);
typedef struct {
    const char *name;
    platform_timer_type_t type;
    platform_timer_callback_t callback;
    void *argument;
} platform_timer_config_t;

platform_error_t platform_timer_create(platform_timer_t *timer, const platform_timer_config_t *config);
platform_error_t platform_timer_start(platform_timer_t *timer, uint32_t periodMs);
platform_error_t platform_timer_stop(platform_timer_t *timer);
platform_error_t platform_timer_is_running(const platform_timer_t *timer, platform_bool_t *running);
platform_error_t platform_timer_delete(platform_timer_t *timer);

#endif
