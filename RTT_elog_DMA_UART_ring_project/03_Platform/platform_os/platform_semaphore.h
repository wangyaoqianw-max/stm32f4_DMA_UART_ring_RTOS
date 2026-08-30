#ifndef PLATFORM_SEMAPHORE_H
#define PLATFORM_SEMAPHORE_H

#include "platform_os_types.h"

platform_error_t platform_semaphore_create(platform_semaphore_t *semaphore, uint32_t maxCount, uint32_t initialCount);
platform_error_t platform_semaphore_take(platform_semaphore_t *semaphore, uint32_t timeoutMs);
platform_error_t platform_semaphore_give(platform_semaphore_t *semaphore);
platform_error_t platform_semaphore_give_from_isr(platform_semaphore_t *semaphore);
platform_error_t platform_semaphore_delete(platform_semaphore_t *semaphore);

#endif
