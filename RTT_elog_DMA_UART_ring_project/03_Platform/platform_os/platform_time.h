#ifndef PLATFORM_TIME_H
#define PLATFORM_TIME_H

#include "platform_os_types.h"

platform_error_t platform_time_delay_ms(uint32_t delayMs);
platform_error_t platform_time_get_ms(uint32_t *timeMs);

#endif
