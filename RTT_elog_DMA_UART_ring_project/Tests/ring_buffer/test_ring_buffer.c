/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_ring_buffer.c
 * @brief RingBuffer 主机测试
 * @author YaoQian Wang
 * @date 2026-08-30
 * @version V1.0
 *****************************************************************************/

#include "ring_buffer.h"
#include "platform_def.h"

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            return __LINE__; \
        } \
    } while (0)

static int test_init_validates_parameters(void)
{
    ring_buffer_t ringBuffer = {0};
    uint8_t storage[8] = {0};

    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_init(NULL, storage, sizeof(storage)));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_init(&ringBuffer, NULL, sizeof(storage)));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == ring_buffer_init(&ringBuffer, storage, 1U));

    return 0;
}

static int test_init_sets_empty_state_and_sizes(void)
{
    ring_buffer_t ringBuffer = {0};
    uint8_t storage[8] = {0};
    platform_size_t readableSize = 1U;
    platform_size_t freeSize = 0U;

    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_init(&ringBuffer, storage, sizeof(storage)));
    TEST_ASSERT(storage == ringBuffer.storage);
    TEST_ASSERT(sizeof(storage) == ringBuffer.storageSize);
    TEST_ASSERT(0U == ringBuffer.readIndex);
    TEST_ASSERT(0U == ringBuffer.writeIndex);
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_get_readable_size(&ringBuffer, &readableSize));
    TEST_ASSERT(0U == readableSize);
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_get_free_size(&ringBuffer, &freeSize));
    TEST_ASSERT(7U == freeSize);

    return 0;
}

static int test_other_apis_validate_object_and_outputs(void)
{
    ring_buffer_t ringBuffer = {0};
    uint8_t storage[8] = {0};
    platform_size_t length = 0U;

    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_reset(NULL));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_write(NULL, NULL, 0U, &length));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_read(NULL, NULL, 0U, &length));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_get_readable_size(NULL, &length));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_get_free_size(NULL, &length));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == ring_buffer_reset(&ringBuffer));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == ring_buffer_get_readable_size(&ringBuffer, &length));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == ring_buffer_get_free_size(&ringBuffer, &length));
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_init(&ringBuffer, storage, sizeof(storage)));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_get_readable_size(&ringBuffer, NULL));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_get_free_size(&ringBuffer, NULL));

    return 0;
}

static int test_reset_preserves_storage_and_clears_indexes(void)
{
    ring_buffer_t ringBuffer = {0};
    uint8_t storage[8] = {0xA5U, 0x5AU, 0x3CU, 0xC3U, 0x96U, 0x69U, 0xF0U, 0x0FU};
    uint8_t expected[8] = {0xA5U, 0x5AU, 0x3CU, 0xC3U, 0x96U, 0x69U, 0xF0U, 0x0FU};
    platform_size_t index = 0U;

    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_init(&ringBuffer, storage, sizeof(storage)));
    ringBuffer.readIndex = 3U;
    ringBuffer.writeIndex = 5U;
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_reset(&ringBuffer));
    TEST_ASSERT(0U == ringBuffer.readIndex);
    TEST_ASSERT(0U == ringBuffer.writeIndex);

    for (index = 0U; index < sizeof(storage); index++) {
        TEST_ASSERT(expected[index] == storage[index]);
    }

    return 0;
}

static int test_write_and_read_preserve_order(void)
{
    ring_buffer_t ringBuffer = {0};
    uint8_t storage[8] = {0};
    uint8_t input[4] = {0x11U, 0x22U, 0x33U, 0x44U};
    uint8_t output[4] = {0};
    platform_size_t writtenLength = 0U;
    platform_size_t readLength = 0U;
    platform_size_t index = 0U;

    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_init(&ringBuffer, storage, sizeof(storage)));
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_write(&ringBuffer,
                                                      input,
                                                      sizeof(input),
                                                      &writtenLength));
    TEST_ASSERT(sizeof(input) == writtenLength);
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_read(&ringBuffer,
                                                     output,
                                                     sizeof(output),
                                                     &readLength));
    TEST_ASSERT(sizeof(output) == readLength);

    for (index = 0U; index < sizeof(output); index++) {
        TEST_ASSERT(input[index] == output[index]);
    }

    return 0;
}

static int test_read_supports_partial_and_empty_results(void)
{
    ring_buffer_t ringBuffer = {0};
    uint8_t storage[8] = {0};
    uint8_t input[4] = {1U, 2U, 3U, 4U};
    uint8_t output[2] = {0};
    platform_size_t writtenLength = 0U;
    platform_size_t readLength = 9U;

    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_init(&ringBuffer, storage, sizeof(storage)));
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_write(&ringBuffer,
                                                      input,
                                                      sizeof(input),
                                                      &writtenLength));
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_read(&ringBuffer,
                                                     output,
                                                     sizeof(output),
                                                     &readLength));
    TEST_ASSERT(sizeof(output) == readLength);
    TEST_ASSERT(1U == output[0]);
    TEST_ASSERT(2U == output[1]);
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_read(&ringBuffer,
                                                     output,
                                                     sizeof(output),
                                                     &readLength));
    TEST_ASSERT(sizeof(output) == readLength);
    TEST_ASSERT(3U == output[0]);
    TEST_ASSERT(4U == output[1]);
    TEST_ASSERT(PLATFORM_ERR_EMPTY == ring_buffer_read(&ringBuffer,
                                                        output,
                                                        sizeof(output),
                                                        &readLength));
    TEST_ASSERT(0U == readLength);

    return 0;
}

static int test_read_write_validate_lengths_and_pointers(void)
{
    ring_buffer_t ringBuffer = {0};
    uint8_t storage[8] = {0};
    uint8_t value = 0x5AU;
    platform_size_t length = 9U;

    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_init(&ringBuffer, storage, sizeof(storage)));
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_write(&ringBuffer, NULL, 0U, &length));
    TEST_ASSERT(0U == length);
    TEST_ASSERT(PLATFORM_ERR_OK == ring_buffer_read(&ringBuffer, NULL, 0U, &length));
    TEST_ASSERT(0U == length);
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_write(&ringBuffer, NULL, 1U, &length));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_read(&ringBuffer, NULL, 1U, &length));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_write(&ringBuffer, &value, 1U, NULL));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == ring_buffer_read(&ringBuffer, &value, 1U, NULL));

    return 0;
}

int main(void)
{
    int result = 0;

    result = test_init_validates_parameters();
    if (0 != result) {
        return result;
    }

    result = test_init_sets_empty_state_and_sizes();
    if (0 != result) {
        return result;
    }

    result = test_other_apis_validate_object_and_outputs();
    if (0 != result) {
        return result;
    }

    result = test_reset_preserves_storage_and_clears_indexes();
    if (0 != result) {
        return result;
    }

    result = test_write_and_read_preserve_order();
    if (0 != result) {
        return result;
    }

    result = test_read_supports_partial_and_empty_results();
    if (0 != result) {
        return result;
    }

    result = test_read_write_validate_lengths_and_pointers();
    if (0 != result) {
        return result;
    }

    return 0;
}
