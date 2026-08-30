/******************************************************************************
 * @file impl_freertos_time.c
 * @brief CMSIS-RTOS2 time and delay adapter
 *****************************************************************************/

#include "impl_freertos_common.h"

uint32_t impl_freertos_timeout_to_ticks(uint32_t timeoutMs)
{
    uint64_t ticks;
    uint32_t frequency;

    if (PLATFORM_OS_WAIT_FOREVER == timeoutMs) {
        return osWaitForever;
    }

    if (PLATFORM_OS_NO_WAIT == timeoutMs) {
        return 0U;
    }

    frequency = osKernelGetTickFreq();
    if (0U == frequency) {
        return 0U;
    }

    ticks = (((uint64_t)timeoutMs * frequency) + 999U) / 1000U;
    return (ticks > 0xFFFFFFFEULL) ? 0xFFFFFFFEU : (uint32_t)ticks;
}

platform_error_t impl_freertos_map_status(osStatus_t status)
{
    switch (status) {
        case osOK:
            return PLATFORM_ERR_OK;
        case osErrorTimeout:
            return PLATFORM_ERR_TIMEOUT;
        case osErrorParameter:
            return PLATFORM_ERR_INVALID_PARAM;
        case osErrorNoMemory:
            return PLATFORM_ERR_NO_MEMORY;
        case osErrorISR:
            return PLATFORM_ERR_INVALID_STATE;
        case osErrorResource:
            return PLATFORM_ERR_NO_RESOURCE;
        default:
            return PLATFORM_ERR_UNKNOWN;
    }
}

platform_error_t platform_time_delay_ms(uint32_t delayMs)
{
    if (0U == delayMs) {
        return PLATFORM_ERR_OK;
    }

    if (PLATFORM_OS_WAIT_FOREVER == delayMs) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return impl_freertos_map_status(osDelay(impl_freertos_timeout_to_ticks(delayMs)));
}

platform_error_t platform_time_get_ms(uint32_t *timeMs)
{
    uint32_t frequency;

    if ((void *)0 == timeMs) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    frequency = osKernelGetTickFreq();
    if (0U == frequency) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    *timeMs = (uint32_t)(((uint64_t)osKernelGetTickCount() * 1000U) / frequency);
    return PLATFORM_ERR_OK;
}
