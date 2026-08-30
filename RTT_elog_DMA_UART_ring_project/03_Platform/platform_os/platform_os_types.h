#ifndef PLATFORM_OS_TYPES_H
#define PLATFORM_OS_TYPES_H

#include "platform_error.h"

#define PLATFORM_OS_OBJECT_INITIALIZER { (void *)0 }
#define PLATFORM_OS_NO_WAIT            (0U)
#define PLATFORM_OS_WAIT_FOREVER       (0xFFFFFFFFU)
#define PLATFORM_NOTIFY_VALID_MASK     (0x7FFFFFFFU)

typedef struct { void *native; } platform_thread_t;
typedef struct { void *native; } platform_mutex_t;
typedef struct { void *native; } platform_semaphore_t;
typedef struct { void *native; } platform_queue_t;
typedef struct { void *native; } platform_timer_t;

#endif
