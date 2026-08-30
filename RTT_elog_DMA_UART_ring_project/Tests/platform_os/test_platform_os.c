#include "platform_os.h"
#include "impl_freertos_common.h"

static uint32_t s_tickFrequency;
static uint32_t s_tickCount;
static uint32_t s_delayTicks;
static osStatus_t s_delayStatus;

uint32_t osKernelGetTickFreq(void) { return s_tickFrequency; }
uint32_t osKernelGetTickCount(void) { return s_tickCount; }
osStatus_t osDelay(uint32_t ticks) { s_delayTicks = ticks; return s_delayStatus; }

#define TEST_ASSERT(expression) do { if (!(expression)) { return __LINE__; } } while (0)

static int test_timeout_conversion(void)
{
    s_tickFrequency = 1000U;
    TEST_ASSERT(1U == impl_freertos_timeout_to_ticks(1U));
    s_tickFrequency = 250U;
    TEST_ASSERT(1U == impl_freertos_timeout_to_ticks(1U));
    TEST_ASSERT(2U == impl_freertos_timeout_to_ticks(5U));
    TEST_ASSERT(250U == impl_freertos_timeout_to_ticks(1000U));
    TEST_ASSERT(osWaitForever == impl_freertos_timeout_to_ticks(PLATFORM_OS_WAIT_FOREVER));
    return 0;
}

static int test_time_adapter(void)
{
    uint32_t timeMs = 0U;
    s_tickFrequency = 250U;
    s_delayStatus = osOK;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_time_delay_ms(5U));
    TEST_ASSERT(2U == s_delayTicks);
    s_tickCount = 500U;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_time_get_ms(&timeMs));
    TEST_ASSERT(2000U == timeMs);
    s_delayStatus = osErrorISR;
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE == platform_time_delay_ms(1U));
    return 0;
}

int main(void)
{
    int result = test_timeout_conversion();
    return (0 == result) ? test_time_adapter() : result;
}
