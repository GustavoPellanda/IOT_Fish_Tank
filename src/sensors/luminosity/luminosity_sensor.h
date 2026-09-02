#ifndef LUMINOSITY_SENSOR_H
#define LUMINOSITY_SENSOR_H

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

/* Driver for the LDR luminosity sensor */

typedef struct {
    adc_unit_t adc_unit;            // ADC unit used by the LDR divider
    adc_channel_t adc_channel;      // ADC channel connected to the LDR divider
    adc_atten_t adc_attenuation;    // Input attenuation range
} luminosity_sensor_config_t;

esp_err_t luminosity_sensor_init(
    const luminosity_sensor_config_t *config   // ADC configuration for the LDR reading
);

esp_err_t luminosity_sensor_read_raw(
    int *raw_value  // Raw ADC sample
);

#endif

/*
 * The sensor is an LDR used as a voltage divider connected to an ADC input.
 * During init, the ADC unit/channel/attenuation are configured, and later
 * luminosity_sensor_read_raw() reads the raw ADC value corresponding to the
 * current light level (more light changes the LDR resistance and therefore the ADC voltage).
 */
