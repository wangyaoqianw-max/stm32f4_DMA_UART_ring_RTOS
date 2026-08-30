#ifndef PLATFORM_NOTIFY_H
#define PLATFORM_NOTIFY_H

#include "platform_os_types.h"

platform_error_t platform_notify_set(platform_thread_t *thread, uint32_t flags);
platform_error_t platform_notify_set_from_isr(platform_thread_t *thread, uint32_t flags);
platform_error_t platform_notify_wait(uint32_t flags, platform_bool_t waitAll, platform_bool_t clearOnExit,
                                      uint32_t timeoutMs, uint32_t *receivedFlags);
platform_error_t platform_notify_clear(uint32_t flags, uint32_t *previousFlags);

#endif
