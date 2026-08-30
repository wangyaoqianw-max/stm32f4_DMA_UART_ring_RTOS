/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * @file test_platform_os.c
 * @brief 验证 FreeRTOS Platform OS Adapter 的时间映射与错误语义。
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *****************************************************************************/

#include "platform_os.h"

#include "impl_freertos_common.h"

static uint32_t s_tickFrequency;
static uint32_t s_tickCount;
static uint32_t s_delayTicks;
static osStatus_t s_delayStatus;
static osTimerId_t s_timerHandle;
static osTimerFunc_t s_timerFunction;
static osTimerType_t s_timerType;
static void *s_timerArgument;
static const char *s_timerName;
static uint32_t s_timerStartTicks;
static osStatus_t s_timerStatus;
static uint32_t s_timerIsRunning;
static uint32_t s_onceArgumentValue;
static uint32_t s_periodicArgumentValue;
static osThreadId_t s_threadHandle;
static osThreadFunc_t s_threadFunction;
static void *s_threadArgument;
static const osThreadAttr_t *s_threadAttributes;
static osStatus_t s_threadStatus;
static osPriority_t s_threadPriority;

uint32_t osKernelGetTickFreq(void)
{
    return s_tickFrequency;
}

uint32_t osKernelGetTickCount(void)
{
    return s_tickCount;
}

osStatus_t osDelay(uint32_t ticks)
{
    s_delayTicks = ticks;
    return s_delayStatus;
}

osThreadId_t osThreadNew(osThreadFunc_t function,
                         void *argument,
                         const osThreadAttr_t *attributes)
{
    s_threadFunction = function;
    s_threadArgument = argument;
    s_threadAttributes = attributes;
    return s_threadHandle;
}

osThreadId_t osThreadGetId(void)
{
    return s_threadHandle;
}

osStatus_t osThreadSetPriority(osThreadId_t threadId, osPriority_t priority)
{
    (void)threadId;
    s_threadPriority = priority;
    return s_threadStatus;
}

osPriority_t osThreadGetPriority(osThreadId_t threadId)
{
    (void)threadId;
    return s_threadPriority;
}

osStatus_t osThreadSuspend(osThreadId_t threadId)
{
    (void)threadId;
    return s_threadStatus;
}

osStatus_t osThreadResume(osThreadId_t threadId)
{
    (void)threadId;
    return s_threadStatus;
}

osStatus_t osThreadTerminate(osThreadId_t threadId)
{
    (void)threadId;
    return s_threadStatus;
}

osStatus_t osThreadYield(void)
{
    return s_threadStatus;
}

osTimerId_t osTimerNew(osTimerFunc_t function,
                       osTimerType_t type,
                       void *argument,
                       const osTimerAttr_t *attributes)
{
    s_timerFunction = function;
    s_timerType = type;
    s_timerArgument = argument;
    s_timerName = attributes->name;

    return s_timerHandle;
}

osStatus_t osTimerStart(osTimerId_t timerId, uint32_t ticks)
{
    (void)timerId;
    s_timerStartTicks = ticks;
    return s_timerStatus;
}

osStatus_t osTimerStop(osTimerId_t timerId)
{
    (void)timerId;
    return s_timerStatus;
}

uint32_t osTimerIsRunning(osTimerId_t timerId)
{
    (void)timerId;
    return s_timerIsRunning;
}

osStatus_t osTimerDelete(osTimerId_t timerId)
{
    (void)timerId;
    return s_timerStatus;
}

#define TEST_ASSERT(expression)              \
    do {                                     \
        if (!(expression)) {                 \
            return __LINE__;                 \
        }                                    \
    } while (0)

static int test_timeout_conversion(void)
{
    s_tickFrequency = 1000U;
    TEST_ASSERT(1U == impl_freertos_timeout_to_ticks(1U));
    s_tickFrequency = 250U;
    TEST_ASSERT(1U == impl_freertos_timeout_to_ticks(1U));
    TEST_ASSERT(2U == impl_freertos_timeout_to_ticks(5U));
    TEST_ASSERT(250U == impl_freertos_timeout_to_ticks(1000U));
    TEST_ASSERT(osWaitForever ==
                impl_freertos_timeout_to_ticks(PLATFORM_OS_WAIT_FOREVER));
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

static void test_timer_callback(void *argument)
{
    (void)argument;
}

static int test_timer_adapter(void)
{
    platform_timer_t timer = PLATFORM_OS_OBJECT_INITIALIZER;
    platform_timer_config_t onceConfig = {
        "once",
        PLATFORM_TIMER_ONCE,
        test_timer_callback,
        &s_onceArgumentValue
    };
    platform_timer_config_t periodicConfig = {
        "periodic",
        PLATFORM_TIMER_PERIODIC,
        test_timer_callback,
        &s_periodicArgumentValue
    };
    platform_bool_t isRunning = 0U;

    s_tickFrequency = 250U;
    s_timerHandle = (osTimerId_t)0x1U;
    s_timerStatus = osOK;
    s_timerIsRunning = 1U;

    TEST_ASSERT(PLATFORM_ERR_OK == platform_timer_create(&timer, &onceConfig));
    TEST_ASSERT(timer.native == s_timerHandle);
    TEST_ASSERT(s_timerFunction == onceConfig.callback);
    TEST_ASSERT(s_timerArgument == onceConfig.argument);
    TEST_ASSERT(s_timerName == onceConfig.name);
    TEST_ASSERT(s_timerType == osTimerOnce);
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_timer_start(&timer, 0U));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_timer_start(&timer, 5U));
    TEST_ASSERT(s_timerStartTicks == 2U);
    TEST_ASSERT(PLATFORM_ERR_OK == platform_timer_stop(&timer));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_timer_is_running(&timer, &isRunning));
    TEST_ASSERT(isRunning == 1U);
    TEST_ASSERT(PLATFORM_ERR_OK == platform_timer_delete(&timer));
    TEST_ASSERT(timer.native == (void *)0);

    TEST_ASSERT(PLATFORM_ERR_OK == platform_timer_create(&timer, &periodicConfig));
    TEST_ASSERT(s_timerType == osTimerPeriodic);
    s_timerHandle = (osTimerId_t)0;
    timer.native = (void *)0;
    TEST_ASSERT(PLATFORM_ERR_NO_MEMORY == platform_timer_create(&timer, &onceConfig));
    TEST_ASSERT(timer.native == (void *)0);

    return 0;
}

static void test_thread_entry(void *argument)
{
    (void)argument;
}

static int test_thread_adapter(void)
{
    uint32_t argumentValue = 0U;
    platform_thread_t thread = PLATFORM_OS_OBJECT_INITIALIZER;
    platform_thread_config_t config = {
        "worker",
        test_thread_entry,
        &argumentValue,
        256U,
        PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL
    };

    s_threadHandle = (osThreadId_t)0x1U;
    s_threadStatus = osOK;
    s_threadPriority = osPriorityAboveNormal;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_thread_create(&thread, &config));
    TEST_ASSERT(thread.native == s_threadHandle);
    TEST_ASSERT(s_threadFunction == config.entry);
    TEST_ASSERT(s_threadArgument == config.argument);
    TEST_ASSERT(s_threadAttributes->name == config.name);
    TEST_ASSERT(s_threadAttributes->stack_size == config.stackSizeBytes);
    TEST_ASSERT(s_threadAttributes->priority == osPriorityAboveNormal);
    TEST_ASSERT(PLATFORM_ERR_OK == platform_thread_suspend(&thread));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_thread_resume(&thread));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_thread_set_priority(&thread,
                                                                 PLATFORM_THREAD_PRIORITY_LOW));
    TEST_ASSERT(s_threadPriority == osPriorityLow);
    TEST_ASSERT(PLATFORM_ERR_OK == platform_thread_terminate(&thread));
    TEST_ASSERT(thread.native == (void *)0);
    return 0;
}

int main(void)
{
    int result = test_timeout_conversion();

    if (result != 0) {
        return result;
    }

    result = test_time_adapter();
    if (result != 0) {
        return result;
    }

    result = test_timer_adapter();
    if (result != 0) {
        return result;
    }

    return test_thread_adapter();
}
