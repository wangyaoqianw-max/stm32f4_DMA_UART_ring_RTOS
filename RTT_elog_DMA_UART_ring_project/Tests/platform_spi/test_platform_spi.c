/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_spi.c
 * @brief 验证 Platform SPI Bus、Device 与显式事务合同
 * @author YaoQian Wang
 * @date 2026-09-05
 * @version V1.0
 *
 *****************************************************************************/

#include "platform_spi.h"

#define TEST_ASSERT(condition)       \
    do {                             \
        if (!(condition)) {          \
            return __LINE__;         \
        }                            \
    } while (0)

typedef struct
{
    platform_error_t applyResult;
    platform_error_t writeResult;
    platform_spi_bus_t *lastBus;
    platform_spi_device_config_t lastConfig;
    const uint8_t *lastData;
    platform_size_t lastLength;
    uint32_t applyCount;
    uint32_t writeCount;
} fake_spi_context_t;

typedef struct
{
    platform_error_t writeResult;
    platform_gpio_level_t lastLevel;
    uint32_t writeCount;
} fake_gpio_context_t;

static platform_error_t fake_lifecycle(void *self)
{
    (void)self;
    return PLATFORM_ERR_OK;
}

static platform_error_t fake_spi_apply_config(
    platform_spi_bus_t *bus,
    const platform_spi_device_config_t *config)
{
    fake_spi_context_t *context = (fake_spi_context_t *)bus->implContext;

    context->lastBus = bus;
    context->lastConfig = *config;
    context->applyCount++;
    return context->applyResult;
}

static platform_error_t fake_spi_write(
    platform_spi_bus_t *bus,
    const uint8_t *data,
    platform_size_t dataLength)
{
    fake_spi_context_t *context = (fake_spi_context_t *)bus->implContext;

    context->lastBus = bus;
    context->lastData = data;
    context->lastLength = dataLength;
    context->writeCount++;
    return context->writeResult;
}

static platform_error_t fake_gpio_configure(
    platform_gpio_t *gpio,
    const platform_gpio_config_t *config)
{
    (void)gpio;
    (void)config;
    return PLATFORM_ERR_OK;
}

static platform_error_t fake_gpio_write(
    platform_gpio_t *gpio,
    platform_gpio_level_t level)
{
    fake_gpio_context_t *context = (fake_gpio_context_t *)gpio->implContext;

    context->lastLevel = level;
    context->writeCount++;
    return context->writeResult;
}

static platform_error_t fake_gpio_read(
    platform_gpio_t *gpio,
    platform_gpio_level_t *level)
{
    (void)gpio;
    *level = PLATFORM_GPIO_LEVEL_LOW;
    return PLATFORM_ERR_OK;
}

static platform_error_t fake_gpio_deinit(platform_gpio_t *gpio)
{
    (void)gpio;
    return PLATFORM_ERR_OK;
}

static const platform_lifecycle_ops_t g_fakeLifecycleOps = {
    fake_lifecycle,
    fake_lifecycle,
    fake_lifecycle,
    fake_lifecycle,
    fake_lifecycle
};

static const platform_spi_bus_ops_t g_fakeSpiOps = {
    fake_spi_apply_config,
    fake_spi_write
};

static const platform_gpio_ops_t g_fakeGpioOps = {
    fake_gpio_configure,
    fake_gpio_write,
    fake_gpio_read,
    fake_gpio_deinit
};

static fake_spi_context_t make_spi_context(
    platform_error_t applyResult,
    platform_error_t writeResult)
{
    fake_spi_context_t context = {0};

    context.applyResult = applyResult;
    context.writeResult = writeResult;
    return context;
}

static platform_spi_bus_init_params_t make_bus_params(
    fake_spi_context_t *context)
{
    platform_spi_bus_init_params_t params = {
        "spi-test",
        PLATFORM_DEVICE_CAP_NONE,
        &g_fakeLifecycleOps,
        &g_fakeSpiOps,
        context
    };

    return params;
}

static platform_spi_device_config_t make_device_config(void)
{
    platform_spi_device_config_t config = {
        PLATFORM_SPI_MODE_3,
        PLATFORM_SPI_BIT_ORDER_MSB_FIRST,
        8U,
        20000000U
    };

    return config;
}

static int prepare_gpio(
    platform_gpio_t *gpio,
    fake_gpio_context_t *context)
{
    platform_gpio_init_params_t params = {
        "spi-cs",
        &g_fakeGpioOps,
        context
    };
    const platform_gpio_config_t config = {
        PLATFORM_GPIO_DIRECTION_OUTPUT,
        PLATFORM_GPIO_PULL_NONE,
        PLATFORM_GPIO_OUTPUT_PUSH_PULL,
        PLATFORM_GPIO_LEVEL_HIGH
    };

    if (platform_gpio_init(gpio, &params) != PLATFORM_ERR_OK) {
        return __LINE__;
    }
    if (platform_gpio_configure(gpio, &config) != PLATFORM_ERR_OK) {
        return __LINE__;
    }

    return 0;
}

static int test_bus_init_constructs_generic_spi_device(void)
{
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    fake_spi_context_t context = make_spi_context(PLATFORM_ERR_OK,
                                                  PLATFORM_ERR_OK);
    platform_spi_bus_init_params_t params = make_bus_params(&context);

    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_bus_init(&bus, &params));
    TEST_ASSERT(PLATFORM_TRUE ==
                platform_object_is_valid(&bus.device.object,
                                         PLATFORM_OBJECT_DEVICE));
    TEST_ASSERT(PLATFORM_DEVICE_CLASS_SPI == bus.device.dev_class);
    TEST_ASSERT(PLATFORM_OBJECT_CREATED == bus.device.object.state);
    TEST_ASSERT(&g_fakeSpiOps == bus.ops);
    TEST_ASSERT(&context == bus.implContext);
    TEST_ASSERT(NULL == bus.activeDevice);
    TEST_ASSERT(PLATFORM_ERR_ALREADY_INITIALIZED ==
                platform_spi_bus_init(&bus, &params));

    return 0;
}

static int test_bus_init_rejects_missing_contract_fields(void)
{
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    fake_spi_context_t context = make_spi_context(PLATFORM_ERR_OK,
                                                  PLATFORM_ERR_OK);
    platform_spi_bus_init_params_t params = make_bus_params(&context);
    platform_spi_bus_ops_t incompleteOps = g_fakeSpiOps;

    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_spi_bus_init(NULL, &params));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_spi_bus_init(&bus, NULL));

    params.name = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_spi_bus_init(&bus, &params));
    params = make_bus_params(&context);
    params.lifecycle = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_spi_bus_init(&bus, &params));
    params = make_bus_params(&context);
    params.ops = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_spi_bus_init(&bus, &params));
    params = make_bus_params(&context);
    incompleteOps.applyConfig = NULL;
    params.ops = &incompleteOps;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_spi_bus_init(&bus, &params));
    incompleteOps = g_fakeSpiOps;
    incompleteOps.write = NULL;
    params.ops = &incompleteOps;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_spi_bus_init(&bus, &params));

    return 0;
}

static int test_device_init_validates_config_and_sets_cs_inactive(void)
{
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    platform_spi_device_t device = PLATFORM_SPI_DEVICE_INITIALIZER;
    platform_gpio_t cs = PLATFORM_GPIO_INITIALIZER;
    fake_spi_context_t spiContext = make_spi_context(PLATFORM_ERR_OK,
                                                     PLATFORM_ERR_OK);
    fake_gpio_context_t gpioContext = {
        PLATFORM_ERR_OK,
        PLATFORM_GPIO_LEVEL_LOW,
        0U
    };
    platform_spi_bus_init_params_t params = make_bus_params(&spiContext);
    platform_spi_device_config_t config = make_device_config();

    TEST_ASSERT(0 == prepare_gpio(&cs, &gpioContext));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_bus_init(&bus, &params));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_spi_device_init(&device,
                                         "display",
                                         &bus,
                                         &cs,
                                         PLATFORM_GPIO_LEVEL_LOW,
                                         &config));
    TEST_ASSERT(PLATFORM_TRUE == device.initialized);
    TEST_ASSERT(&bus == device.bus);
    TEST_ASSERT(&cs == device.cs);
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_HIGH == gpioContext.lastLevel);
    TEST_ASSERT(1U == gpioContext.writeCount);

    TEST_ASSERT(PLATFORM_ERR_ALREADY_INITIALIZED ==
                platform_spi_device_init(&device,
                                         "display",
                                         &bus,
                                         &cs,
                                         PLATFORM_GPIO_LEVEL_LOW,
                                         &config));

    return 0;
}

static int test_device_init_rejects_invalid_parameters_and_config(void)
{
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    platform_spi_device_t device = PLATFORM_SPI_DEVICE_INITIALIZER;
    fake_spi_context_t context = make_spi_context(PLATFORM_ERR_OK,
                                                  PLATFORM_ERR_OK);
    platform_spi_bus_init_params_t params = make_bus_params(&context);
    platform_spi_device_config_t config = make_device_config();

    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_bus_init(&bus, &params));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER ==
                platform_spi_device_init(NULL, "device", &bus, NULL,
                                         PLATFORM_GPIO_LEVEL_LOW, &config));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER ==
                platform_spi_device_init(&device, "device", NULL, NULL,
                                         PLATFORM_GPIO_LEVEL_LOW, &config));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER ==
                platform_spi_device_init(&device, "device", &bus, NULL,
                                         PLATFORM_GPIO_LEVEL_LOW, NULL));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_spi_device_init(&device, NULL, &bus, NULL,
                                         PLATFORM_GPIO_LEVEL_LOW, &config));

    config.mode = PLATFORM_SPI_MODE_MAX;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_spi_device_init(&device, "device", &bus, NULL,
                                         PLATFORM_GPIO_LEVEL_LOW, &config));
    config = make_device_config();
    config.bitOrder = PLATFORM_SPI_BIT_ORDER_MAX;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_spi_device_init(&device, "device", &bus, NULL,
                                         PLATFORM_GPIO_LEVEL_LOW, &config));
    config = make_device_config();
    config.maxClockHz = 0U;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_spi_device_init(&device, "device", &bus, NULL,
                                         PLATFORM_GPIO_LEVEL_LOW, &config));
    config = make_device_config();
    config.dataBits = 16U;
    TEST_ASSERT(PLATFORM_ERR_NOT_SUPPORTED ==
                platform_spi_device_init(&device, "device", &bus, NULL,
                                         PLATFORM_GPIO_LEVEL_LOW, &config));
    config = make_device_config();
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM ==
                platform_spi_device_init(&device, "device", &bus, NULL,
                                         PLATFORM_GPIO_LEVEL_MAX, &config));
    TEST_ASSERT(PLATFORM_FALSE == device.initialized);

    return 0;
}

static int test_transactions_enforce_state_ownership_and_forward_write(void)
{
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    platform_spi_device_t first = PLATFORM_SPI_DEVICE_INITIALIZER;
    platform_spi_device_t second = PLATFORM_SPI_DEVICE_INITIALIZER;
    fake_spi_context_t context = make_spi_context(PLATFORM_ERR_OK,
                                                  PLATFORM_ERR_OK);
    platform_spi_bus_init_params_t params = make_bus_params(&context);
    platform_spi_device_config_t config = make_device_config();
    uint8_t data[2] = {0x12U, 0x34U};

    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_bus_init(&bus, &params));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_spi_device_init(&first, "first", &bus, NULL,
                                         PLATFORM_GPIO_LEVEL_LOW, &config));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_spi_device_init(&second, "second", &bus, NULL,
                                         PLATFORM_GPIO_LEVEL_LOW, &config));

    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE ==
                platform_spi_transaction_begin(&first));
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE ==
                platform_spi_write(&first, data, sizeof(data)));

    bus.device.object.state = PLATFORM_OBJECT_STARTED;
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE ==
                platform_spi_write(&first, data, sizeof(data)));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_transaction_begin(&first));
    TEST_ASSERT(&first == bus.activeDevice);
    TEST_ASSERT(1U == context.applyCount);
    TEST_ASSERT(PLATFORM_ERR_BUSY == platform_spi_transaction_begin(&second));
    TEST_ASSERT(1U == context.applyCount);
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE ==
                platform_spi_write(&second, data, sizeof(data)));
    TEST_ASSERT(PLATFORM_ERR_INVALID_STATE ==
                platform_spi_transaction_end(&second));
    TEST_ASSERT(PLATFORM_ERR_NULL_POINTER == platform_spi_write(&first, NULL, 2U));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == platform_spi_write(&first, data, 0U));

    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_write(&first, data, sizeof(data)));
    TEST_ASSERT(&bus == context.lastBus);
    TEST_ASSERT(data == context.lastData);
    TEST_ASSERT(sizeof(data) == context.lastLength);
    TEST_ASSERT(1U == context.writeCount);
    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_transaction_end(&first));
    TEST_ASSERT(NULL == bus.activeDevice);

    return 0;
}

static int test_config_failure_and_null_cs_have_no_hidden_side_effects(void)
{
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    platform_spi_device_t device = PLATFORM_SPI_DEVICE_INITIALIZER;
    fake_spi_context_t context = make_spi_context(PLATFORM_ERR_NOT_SUPPORTED,
                                                  PLATFORM_ERR_OK);
    platform_spi_bus_init_params_t params = make_bus_params(&context);
    platform_spi_device_config_t config = make_device_config();

    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_bus_init(&bus, &params));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_spi_device_init(&device, "no-cs", &bus, NULL,
                                         PLATFORM_GPIO_LEVEL_HIGH, &config));
    bus.device.object.state = PLATFORM_OBJECT_STARTED;

    TEST_ASSERT(PLATFORM_ERR_NOT_SUPPORTED ==
                platform_spi_transaction_begin(&device));
    TEST_ASSERT(NULL == bus.activeDevice);
    TEST_ASSERT(1U == context.applyCount);

    context.applyResult = PLATFORM_ERR_OK;
    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_transaction_begin(&device));
    TEST_ASSERT(&device == bus.activeDevice);
    TEST_ASSERT(PLATFORM_ERR_BUSY == platform_spi_device_deinit(&device));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_transaction_end(&device));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_device_deinit(&device));
    TEST_ASSERT(PLATFORM_FALSE == device.initialized);
    TEST_ASSERT(NULL == device.bus);
    TEST_ASSERT(NULL == device.cs);

    return 0;
}

static int test_cs_levels_and_end_failure_clear_software_ownership(void)
{
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    platform_spi_device_t device = PLATFORM_SPI_DEVICE_INITIALIZER;
    platform_gpio_t cs = PLATFORM_GPIO_INITIALIZER;
    fake_spi_context_t spiContext = make_spi_context(PLATFORM_ERR_OK,
                                                     PLATFORM_ERR_OK);
    fake_gpio_context_t gpioContext = {
        PLATFORM_ERR_OK,
        PLATFORM_GPIO_LEVEL_LOW,
        0U
    };
    platform_spi_bus_init_params_t params = make_bus_params(&spiContext);
    platform_spi_device_config_t config = make_device_config();

    TEST_ASSERT(0 == prepare_gpio(&cs, &gpioContext));
    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_bus_init(&bus, &params));
    TEST_ASSERT(PLATFORM_ERR_OK ==
                platform_spi_device_init(&device, "active-high", &bus, &cs,
                                         PLATFORM_GPIO_LEVEL_HIGH, &config));
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_LOW == gpioContext.lastLevel);
    bus.device.object.state = PLATFORM_OBJECT_STARTED;

    TEST_ASSERT(PLATFORM_ERR_OK == platform_spi_transaction_begin(&device));
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_HIGH == gpioContext.lastLevel);
    gpioContext.writeResult = PLATFORM_ERR_IO;
    TEST_ASSERT(PLATFORM_ERR_IO == platform_spi_transaction_end(&device));
    TEST_ASSERT(NULL == bus.activeDevice);
    TEST_ASSERT(PLATFORM_GPIO_LEVEL_LOW == gpioContext.lastLevel);

    return 0;
}

int main(void)
{
    int result = test_bus_init_constructs_generic_spi_device();

    if (result != 0) {
        return result;
    }
    result = test_bus_init_rejects_missing_contract_fields();
    if (result != 0) {
        return result;
    }
    result = test_device_init_validates_config_and_sets_cs_inactive();
    if (result != 0) {
        return result;
    }
    result = test_device_init_rejects_invalid_parameters_and_config();
    if (result != 0) {
        return result;
    }
    result = test_transactions_enforce_state_ownership_and_forward_write();
    if (result != 0) {
        return result;
    }
    result = test_config_failure_and_null_cs_have_no_hidden_side_effects();
    if (result != 0) {
        return result;
    }

    return test_cs_levels_and_end_failure_clear_software_ownership();
}
