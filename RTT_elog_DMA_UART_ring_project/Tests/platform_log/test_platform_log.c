/******************************************************************************
 * @file test_platform_log.c
 * @brief 验证 Platform Log 公共输出后端契约
 *****************************************************************************/

#include <stdarg.h>
#include <string.h>

#define LOG_TAG "platform-log-test"
#include "platform_log.h"

#define TEST_ASSERT(condition)          \
    do {                                \
        if (!(condition)) {             \
            return __LINE__;            \
        }                               \
    } while (0)

typedef struct
{
    uint8_t level;
    const char *tag;
    const char *file;
    const char *func;
    long line;
    const char *format;
    int argument;
    uint32_t callCount;
} fake_log_record_t;

static fake_log_record_t s_logRecord;

static void fake_log_output(uint8_t level,
                            const char *tag,
                            const char *file,
                            const char *func,
                            long line,
                            const char *format,
                            ...)
{
    va_list arguments;

    va_start(arguments, format);
    s_logRecord.level = level;
    s_logRecord.tag = tag;
    s_logRecord.file = file;
    s_logRecord.func = func;
    s_logRecord.line = line;
    s_logRecord.format = format;
    s_logRecord.argument = va_arg(arguments, int);
    s_logRecord.callCount++;
    va_end(arguments);
}

platform_log_output_fn_t Platform_Log_GetOutputFn(void)
{
    return fake_log_output;
}

static int test_info_macro_forwards_platform_metadata(void)
{
    memset(&s_logRecord, 0, sizeof(s_logRecord));

    platform_log_i("value=%d", 42);

    TEST_ASSERT(1U == s_logRecord.callCount);
    TEST_ASSERT(PLATFORM_LOG_LEVEL_INFO == s_logRecord.level);
    TEST_ASSERT(0 == strcmp(LOG_TAG, s_logRecord.tag));
    TEST_ASSERT(0 == strcmp("value=%d", s_logRecord.format));
    TEST_ASSERT(42 == s_logRecord.argument);
    TEST_ASSERT(NULL != s_logRecord.file);
    TEST_ASSERT(NULL != s_logRecord.func);
    TEST_ASSERT(0L < s_logRecord.line);

    return 0;
}

static int test_error_macro_forwards_platform_level(void)
{
    memset(&s_logRecord, 0, sizeof(s_logRecord));

    platform_log_e("error=%d", 7);

    TEST_ASSERT(1U == s_logRecord.callCount);
    TEST_ASSERT(PLATFORM_LOG_LEVEL_ERROR == s_logRecord.level);
    TEST_ASSERT(7 == s_logRecord.argument);

    return 0;
}

int main(void)
{
    int result = test_info_macro_forwards_platform_metadata();

    if (0 != result) {
        return result;
    }

    return test_error_macro_forwards_platform_level();
}
