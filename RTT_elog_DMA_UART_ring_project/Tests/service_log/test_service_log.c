/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_service_log.c
 * @brief 验证 Service Log 公共接口和日志参数转发。
 * @author Codex
 * @date 2026-08-31
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define LOG_TAG "service-log-test"
#include "service_log.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define TEST_ASSERT(condition)          \
    do {                                \
        if (!(condition)) {             \
            return __LINE__;            \
        }                               \
    } while (0)
//******************************** Defines *********************************//

//******************************** Types ***********************************//
typedef struct
{
    uint32_t initCallCount;
    uint32_t setLevelCallCount;
    uint32_t enableCallCount;
    uint32_t outputCallCount;
    platform_log_level_t level;
    bool enabled;
    const char *tag;
    const char *format;
    int argument;
} fake_platform_log_t;
//******************************** Types ***********************************//

//******************************** Variables ********************************//
static fake_platform_log_t g_log;
//******************************** Variables ********************************//

//******************************** Functions *********************************//
platform_error_t platform_log_init(void)
{
    g_log.initCallCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_log_set_level(platform_log_level_t level)
{
    g_log.setLevelCallCount++;
    g_log.level = level;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_log_enable_output(bool enable)
{
    g_log.enableCallCount++;
    g_log.enabled = enable;
    return PLATFORM_ERR_OK;
}

static void fake_output(uint8_t level,
                        const char *tag,
                        const char *file,
                        const char *func,
                        long line,
                        const char *format,
                        ...)
{
    va_list args;

    (void)level;
    (void)file;
    (void)func;
    (void)line;

    va_start(args, format);
    g_log.tag = tag;
    g_log.format = format;
    g_log.argument = va_arg(args, int);
    g_log.outputCallCount++;
    va_end(args);
}

platform_log_output_fn_t platform_log_get_output_fn(void)
{
    return fake_output;
}

int main(void)
{
    service_log_level_t level = SERVICE_LOG_LEVEL_INFO;

    TEST_ASSERT(SERVICE_LOG_LEVEL_MAX > level);
    TEST_ASSERT(PLATFORM_ERR_OK == service_log_init());
    TEST_ASSERT(PLATFORM_ERR_OK == service_log_set_level(SERVICE_LOG_LEVEL_DEBUG));
    TEST_ASSERT(PLATFORM_ERR_OK == service_log_enable_output(true));

    SERVICE_LOG_I("value=%d", 42);

    TEST_ASSERT(0 == strcmp("service-log-test", g_log.tag));
    TEST_ASSERT(0 == strcmp("value=%d", g_log.format));
    TEST_ASSERT(42 == g_log.argument);

    return 0;
}
