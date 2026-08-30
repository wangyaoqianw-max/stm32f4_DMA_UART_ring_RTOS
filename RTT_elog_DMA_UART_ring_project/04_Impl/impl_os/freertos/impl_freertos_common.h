#ifndef IMPL_FREERTOS_COMMON_H
#define IMPL_FREERTOS_COMMON_H

#include "cmsis_os2.h"
#include "platform_os.h"

uint32_t impl_freertos_timeout_to_ticks(uint32_t timeoutMs);
platform_error_t impl_freertos_map_status(osStatus_t status);

#endif
