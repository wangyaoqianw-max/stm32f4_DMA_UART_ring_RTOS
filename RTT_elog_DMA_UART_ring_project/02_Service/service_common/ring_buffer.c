/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file ring_buffer.c
 * @brief SPSC 字节 RingBuffer 实现
 * @author YaoQian Wang
 * @date 2026-08-30
 * @version V1.0
 *****************************************************************************/

//******************************** Includes *********************************//
#include "ring_buffer.h"
#include "platform_def.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
static platform_error_t ring_buffer_validate(const ring_buffer_t *ringBuffer)
{
    if (ringBuffer == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if ((ringBuffer->storage == NULL) || (ringBuffer->storageSize < 2U)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return PLATFORM_ERR_OK;
}

platform_error_t ring_buffer_init(ring_buffer_t *ringBuffer,
                                  uint8_t *storage,
                                  platform_size_t storageSize)
{
    if (ringBuffer == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (storage == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (storageSize < 2U) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    ringBuffer->storage = storage;
    ringBuffer->storageSize = storageSize;
    ringBuffer->readIndex = 0U;
    ringBuffer->writeIndex = 0U;

    return PLATFORM_ERR_OK;
}

platform_error_t ring_buffer_reset(ring_buffer_t *ringBuffer)
{
    platform_error_t result = ring_buffer_validate(ringBuffer);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    ringBuffer->readIndex = 0U;
    ringBuffer->writeIndex = 0U;

    return PLATFORM_ERR_OK;
}

platform_error_t ring_buffer_write(ring_buffer_t *ringBuffer,
                                   const uint8_t *data,
                                   platform_size_t dataLength,
                                   platform_size_t *writtenLength)
{
    platform_error_t result = PLATFORM_ERR_OK;

    if (ringBuffer == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (writtenLength == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    *writtenLength = 0U;
    result = ring_buffer_validate(ringBuffer);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (dataLength == 0U) {
        return PLATFORM_ERR_OK;
    }

    if (data == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    return PLATFORM_ERR_OK;
}

platform_error_t ring_buffer_read(ring_buffer_t *ringBuffer,
                                  uint8_t *buffer,
                                  platform_size_t bufferSize,
                                  platform_size_t *readLength)
{
    platform_error_t result = PLATFORM_ERR_OK;

    if (ringBuffer == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (readLength == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    *readLength = 0U;
    result = ring_buffer_validate(ringBuffer);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (bufferSize == 0U) {
        return PLATFORM_ERR_OK;
    }

    if (buffer == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    return PLATFORM_ERR_EMPTY;
}

platform_error_t ring_buffer_get_readable_size(const ring_buffer_t *ringBuffer,
                                               platform_size_t *readableSize)
{
    platform_error_t result = PLATFORM_ERR_OK;

    if (ringBuffer == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (readableSize == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    result = ring_buffer_validate(ringBuffer);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (ringBuffer->writeIndex >= ringBuffer->readIndex) {
        *readableSize = ringBuffer->writeIndex - ringBuffer->readIndex;
    } else {
        *readableSize = ringBuffer->storageSize - ringBuffer->readIndex + ringBuffer->writeIndex;
    }

    return PLATFORM_ERR_OK;
}

platform_error_t ring_buffer_get_free_size(const ring_buffer_t *ringBuffer,
                                           platform_size_t *freeSize)
{
    platform_error_t result = PLATFORM_ERR_OK;
    platform_size_t readableSize = 0U;

    if (ringBuffer == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (freeSize == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    result = ring_buffer_get_readable_size(ringBuffer, &readableSize);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    *freeSize = (ringBuffer->storageSize - 1U) - readableSize;

    return PLATFORM_ERR_OK;
}
//******************************** Functions ********************************//
