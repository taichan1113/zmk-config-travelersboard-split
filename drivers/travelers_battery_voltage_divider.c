/*
 * Battery sensor for travelers-board-split.
 *
 * This is intentionally a local Zephyr module driver. ZMK v0.3's equivalent
 * driver has a fixed 10 ms power-gpios delay, while this PCB requires 300 ms.
 */

#define DT_DRV_COMPAT travelers_battery_voltage_divider

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct travelers_bvd_config {
    uint8_t adc_channel;
    struct gpio_dt_spec power;
    uint32_t output_ohms;
    uint32_t full_ohms;
    uint32_t settle_delay_ms;
    uint16_t empty_millivolts;
    uint16_t full_millivolts;
};

struct travelers_bvd_data {
    const struct device *adc;
    struct adc_channel_cfg adc_channel_config;
    struct adc_sequence sequence;
    int16_t adc_raw;
    uint16_t millivolts;
    uint8_t state_of_charge;
};

static uint8_t travelers_bvd_percent(const struct travelers_bvd_config *config, uint16_t millivolts) {
    if (millivolts <= config->empty_millivolts) {
        return 0;
    }
    if (millivolts >= config->full_millivolts) {
        return 100;
    }

    return ((uint32_t)(millivolts - config->empty_millivolts) * 100U) /
           (config->full_millivolts - config->empty_millivolts);
}

static int travelers_bvd_sample_fetch(const struct device *dev, enum sensor_channel channel) {
    struct travelers_bvd_data *data = dev->data;
    const struct travelers_bvd_config *config = dev->config;

    if (channel != SENSOR_CHAN_GAUGE_VOLTAGE && channel != SENSOR_CHAN_GAUGE_STATE_OF_CHARGE &&
        channel != SENSOR_CHAN_ALL) {
        return -ENOTSUP;
    }

    int rc = gpio_pin_set_dt(&config->power, 1);
    if (rc != 0) {
        LOG_DBG("Failed to enable battery divider: %d", rc);
        return rc;
    }

    k_sleep(K_MSEC(config->settle_delay_ms));

    rc = adc_read(data->adc, &data->sequence);
    data->sequence.calibrate = false;

    int disable_rc = gpio_pin_set_dt(&config->power, 0);
    if (disable_rc != 0) {
        LOG_DBG("Failed to disable battery divider: %d", disable_rc);
        return disable_rc;
    }
    if (rc != 0) {
        LOG_DBG("Battery ADC read failed: %d", rc);
        return rc;
    }

    int32_t millivolts = data->adc_raw;
    rc = adc_raw_to_millivolts(adc_ref_internal(data->adc), data->adc_channel_config.gain,
                               data->sequence.resolution, &millivolts);
    if (rc != 0) {
        return rc;
    }

    millivolts = millivolts * (uint64_t)config->full_ohms / config->output_ohms;
    data->millivolts = millivolts;
    data->state_of_charge = travelers_bvd_percent(config, data->millivolts);
    LOG_DBG("Battery %u mV, %u%%", data->millivolts, data->state_of_charge);

    return 0;
}

static int travelers_bvd_channel_get(const struct device *dev, enum sensor_channel channel,
                                     struct sensor_value *value) {
    const struct travelers_bvd_data *data = dev->data;

    switch (channel) {
    case SENSOR_CHAN_GAUGE_VOLTAGE:
        value->val1 = data->millivolts / 1000;
        value->val2 = (data->millivolts % 1000) * 1000U;
        return 0;
    case SENSOR_CHAN_GAUGE_STATE_OF_CHARGE:
        value->val1 = data->state_of_charge;
        value->val2 = 0;
        return 0;
    default:
        return -ENOTSUP;
    }
}

static const struct sensor_driver_api travelers_bvd_api = {
    .sample_fetch = travelers_bvd_sample_fetch,
    .channel_get = travelers_bvd_channel_get,
};

static int travelers_bvd_init(const struct device *dev) {
    struct travelers_bvd_data *data = dev->data;
    const struct travelers_bvd_config *config = dev->config;

    if (!device_is_ready(data->adc) || !device_is_ready(config->power.port)) {
        return -ENODEV;
    }

    int rc = gpio_pin_configure_dt(&config->power, GPIO_OUTPUT_INACTIVE);
    if (rc != 0) {
        return rc;
    }

    data->sequence = (struct adc_sequence){
        .channels = BIT(0),
        .buffer = &data->adc_raw,
        .buffer_size = sizeof(data->adc_raw),
        .oversampling = 4,
        .calibrate = true,
        .resolution = 12,
    };

#ifdef CONFIG_ADC_NRFX_SAADC
    data->adc_channel_config = (struct adc_channel_cfg){
        .gain = ADC_GAIN_1_6,
        .reference = ADC_REF_INTERNAL,
        .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
        .input_positive = SAADC_CH_PSELP_PSELP_AnalogInput0 + config->adc_channel,
    };
#else
#error "travelers battery driver currently requires the nRF SAADC"
#endif

    return adc_channel_setup(data->adc, &data->adc_channel_config);
}

static struct travelers_bvd_data travelers_bvd_data = {
    .adc = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR(DT_DRV_INST(0))),
};

static const struct travelers_bvd_config travelers_bvd_config = {
    .adc_channel = DT_IO_CHANNELS_INPUT(DT_DRV_INST(0)),
    .power = GPIO_DT_SPEC_INST_GET(0, power_gpios),
    .output_ohms = DT_INST_PROP(0, output_ohms),
    .full_ohms = DT_INST_PROP(0, full_ohms),
    .settle_delay_ms = DT_INST_PROP(0, settle_delay_ms),
    .empty_millivolts = DT_INST_PROP(0, empty_millivolts),
    .full_millivolts = DT_INST_PROP(0, full_millivolts),
};

DEVICE_DT_INST_DEFINE(0, travelers_bvd_init, NULL, &travelers_bvd_data, &travelers_bvd_config,
                      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &travelers_bvd_api);
