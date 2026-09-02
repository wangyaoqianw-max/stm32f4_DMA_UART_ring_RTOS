/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file i2c_smoke.c
 * @brief Software I2C 目标板 DHT20 原始事务 Smoke Test
 * @author Codex
 * @date 2026-09-02
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "i2c_smoke.h"

#include "main.h"
#include "platform_bsp_gpio.h"
#include "platform_i2c.h"
#include "platform_time.h"
#include "service_log.h"

#include <stdio.h>
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define DHT20_I2C_ADDRESS                 (0x38U)
#define DHT20_MEASURE_COMMAND_LENGTH      (3U)
#define DHT20_FRAME_LENGTH                 (7U)
#define DHT20_MEASURE_DELAY_MS             (80U)
#define LOG_TAG                            "i2c_smoke"
//******************************** Defines *********************************//

//******************************** Functions *********************************//
static void i2c_smoke_report_failure(const char *stage, platform_error_t result)
{
    printf("I2C_SMOKE,FAIL,%s,%d\r\n", stage, (int)result);
    SERVICE_LOG_E("i2c smoke failed: %s, result=%d", stage, (int)result);
}

void i2c_smoke_run(void)
{
    platform_gpio_t scl = PLATFORM_GPIO_INITIALIZER;
    platform_gpio_t sda = PLATFORM_GPIO_INITIALIZER;
    platform_i2c_t i2c = PLATFORM_I2C_INITIALIZER;
    const uint8_t measureCommand[DHT20_MEASURE_COMMAND_LENGTH] = {
        0xACU,
        0x33U,
        0x00U
    };
    uint8_t frame[DHT20_FRAME_LENGTH] = {0};
    platform_error_t result = PLATFORM_ERR_OK;

    printf("I2C_SMOKE,START\r\n");
    SERVICE_LOG_I("i2c smoke start");

    result = platform_bsp_gpio_construct_soft_i2c_scl(&scl);
    if (result != PLATFORM_ERR_OK) {
        i2c_smoke_report_failure("construct_scl", result);
        return;
    }

    result = platform_bsp_gpio_construct_soft_i2c_sda(&sda);
    if (result != PLATFORM_ERR_OK) {
        i2c_smoke_report_failure("construct_sda", result);
        return;
    }

    result = platform_i2c_init(&i2c, "phase3_smoke", &scl, &sda);
    if (result != PLATFORM_ERR_OK) {
        i2c_smoke_report_failure("init", result);
        return;
    }

    printf("I2C_SMOKE,INIT,PASS\r\n");
    SERVICE_LOG_I("i2c smoke init pass");

    result = platform_i2c_write(&i2c,
                                DHT20_I2C_ADDRESS,
                                measureCommand,
                                DHT20_MEASURE_COMMAND_LENGTH);
    if (result != PLATFORM_ERR_OK) {
        i2c_smoke_report_failure("measure_command", result);
        return;
    }

    result = platform_time_delay_ms(DHT20_MEASURE_DELAY_MS);
    if (result != PLATFORM_ERR_OK) {
        i2c_smoke_report_failure("measure_delay", result);
        return;
    }

    result = platform_i2c_read(&i2c,
                               DHT20_I2C_ADDRESS,
                               frame,
                               DHT20_FRAME_LENGTH);
    if (result != PLATFORM_ERR_OK) {
        i2c_smoke_report_failure("read_frame", result);
        return;
    }

    printf("I2C_SMOKE,TXRX,PASS,status=0x%02X\r\n", frame[0]);
    SERVICE_LOG_I("i2c smoke txrx pass, status=0x%02X", frame[0]);
    printf("I2C_SMOKE,PASS\r\n");
    SERVICE_LOG_I("i2c smoke pass");
}
//******************************** Functions *********************************//
