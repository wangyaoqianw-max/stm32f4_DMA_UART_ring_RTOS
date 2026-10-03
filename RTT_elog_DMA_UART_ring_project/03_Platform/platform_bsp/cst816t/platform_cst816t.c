/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * @file platform_cst816t.c
 * @brief CST816T 触屏复位、配置与采样
 * @author YaoQian Wang
 * @date 2026-10-03
 * @version V1.0
 *****************************************************************************/

#include "platform_cst816t.h"

#include "platform_def.h"
#include "platform_time.h"

#include <stddef.h>

#define CST816T_ADDRESS          (0x15U)
#define CST816T_CHIP_ID_REGISTER (0xA7U)
#define CST816T_FIRMWARE_REGISTER (0xA9U)
#define CST816T_CHIP_ID          (0xB5U)
#define CST816T_SAMPLE_LENGTH    (7U)
#define CST816T_RESET_LOW_MS     (10U)
#define CST816T_RESET_READY_MS   (100U)

static platform_error_t cst816t_read_register(
    platform_i2c_t *i2c, uint8_t reg, uint8_t *data, uint16_t length)
{
    return platform_i2c_write_read(i2c, CST816T_ADDRESS, &reg, 1U, data, length);
}

static platform_error_t cst816t_reset(platform_gpio_t *reset)
{
    const platform_gpio_config_t config = {
        PLATFORM_GPIO_DIRECTION_OUTPUT, PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL, PLATFORM_GPIO_LEVEL_HIGH
    };
    platform_error_t result = platform_gpio_configure(reset, &config);

    if (result == PLATFORM_ERR_OK) {
        result = platform_gpio_write(reset, PLATFORM_GPIO_LEVEL_LOW);
    }
    if (result == PLATFORM_ERR_OK) {
        result = platform_time_delay_ms(CST816T_RESET_LOW_MS);
    }
    if (result == PLATFORM_ERR_OK) {
        result = platform_gpio_write(reset, PLATFORM_GPIO_LEVEL_HIGH);
    }
    if (result == PLATFORM_ERR_OK) {
        result = platform_time_delay_ms(CST816T_RESET_READY_MS);
    }
    return result;
}

platform_error_t platform_cst816t_init(
    platform_cst816t_t *device, platform_i2c_t *i2c, platform_gpio_t *reset)
{
    /* FA: EnTouch + EnChange；FE: 禁用自动睡眠，避免待机后I²C不应答。 */
    const uint8_t irqConfig[] = {0xFAU, 0x60U};
    const uint8_t sleepConfig[] = {0xFEU, 0x01U};
    uint8_t chipId = 0U;
    uint8_t firmwareVersion = 0U;
    platform_error_t result;

    if ((device == NULL) || (i2c == NULL) || (reset == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (device->initialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }
    if ((i2c->initialized != PLATFORM_TRUE) || (reset->initialized != PLATFORM_TRUE)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }
    result = cst816t_reset(reset);
    if (result == PLATFORM_ERR_OK) {
        result = cst816t_read_register(i2c, CST816T_CHIP_ID_REGISTER, &chipId, 1U);
    }
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (chipId != CST816T_CHIP_ID) {
        return PLATFORM_ERR_NOT_FOUND;
    }
    result = cst816t_read_register(i2c, CST816T_FIRMWARE_REGISTER, &firmwareVersion, 1U);
    if (result == PLATFORM_ERR_OK) {
        result = platform_i2c_write(i2c, CST816T_ADDRESS, irqConfig, sizeof(irqConfig));
    }
    if (result == PLATFORM_ERR_OK) {
        result = platform_i2c_write(i2c, CST816T_ADDRESS, sleepConfig, sizeof(sleepConfig));
    }
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    device->i2c = i2c;
    device->reset = reset;
    device->chipId = chipId;
    device->firmwareVersion = firmwareVersion;
    device->initialized = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_cst816t_read_sample(
    platform_cst816t_t *device, platform_cst816t_sample_t *sample)
{
    uint8_t frame[CST816T_SAMPLE_LENGTH] = {0};
    platform_cst816t_sample_t next = {PLATFORM_FALSE, 0U, 0U};
    platform_error_t result;

    if ((device == NULL) || (sample == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (device->initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }
    if ((device->i2c == NULL) || (device->i2c->initialized != PLATFORM_TRUE)) {
        return PLATFORM_ERR_INVALID_STATE;
    }
    result = cst816t_read_register(device->i2c, 0x00U, frame, sizeof(frame));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (frame[2] > 1U) {
        return PLATFORM_ERR_INVALID_STATE;
    }
    if (frame[2] == 1U) {
        next.pressed = PLATFORM_TRUE;
        next.x = (uint16_t)(((uint16_t)(frame[3] & 0x0FU) << 8U) | frame[4]);
        next.y = (uint16_t)(((uint16_t)(frame[5] & 0x0FU) << 8U) | frame[6]);
    }
    *sample = next;
    return PLATFORM_ERR_OK;
}
