/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_i2c.c
 * @brief Platform I2C 同步事务公共接口实现
 * @author Codex
 * @date 2026-09-02
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_i2c.h"

#include "platform_def.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Functions *********************************//
static platform_error_t platform_i2c_validate_initialized(
    const platform_i2c_t *i2c)
{
    if (i2c == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (i2c->initialized == 0U) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t platform_i2c_validate_address(uint8_t address)
{
    if (address > 0x7FU) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return PLATFORM_ERR_OK;
}

platform_error_t platform_i2c_init(
    platform_i2c_t *i2c,
    const char *name,
    platform_gpio_t *scl,
    platform_gpio_t *sda)
{
    if (i2c == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if ((scl == NULL) || (sda == NULL)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (i2c->initialized != 0U) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    i2c->name = name;
    i2c->scl = scl;
    i2c->sda = sda;
    i2c->initialized = PLATFORM_TRUE;

    return PLATFORM_ERR_OK;
}

platform_error_t platform_i2c_write(
    platform_i2c_t *i2c,
    uint8_t address,
    const uint8_t *data,
    uint16_t length)
{
    platform_error_t result = platform_i2c_validate_initialized(i2c);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_i2c_validate_address(address);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (data == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (length == 0U) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return PLATFORM_ERR_NOT_SUPPORTED;
}

platform_error_t platform_i2c_read(
    platform_i2c_t *i2c,
    uint8_t address,
    uint8_t *data,
    uint16_t length)
{
    platform_error_t result = platform_i2c_validate_initialized(i2c);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_i2c_validate_address(address);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (data == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (length == 0U) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return PLATFORM_ERR_NOT_SUPPORTED;
}

platform_error_t platform_i2c_write_read(
    platform_i2c_t *i2c,
    uint8_t address,
    const uint8_t *txData,
    uint16_t txLength,
    uint8_t *rxData,
    uint16_t rxLength)
{
    platform_error_t result = platform_i2c_validate_initialized(i2c);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_i2c_validate_address(address);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (txData == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (txLength == 0U) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (rxData == NULL) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (rxLength == 0U) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return PLATFORM_ERR_NOT_SUPPORTED;
}

platform_error_t platform_i2c_deinit(platform_i2c_t *i2c)
{
    platform_error_t result = platform_i2c_validate_initialized(i2c);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    i2c->initialized = PLATFORM_FALSE;

    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
