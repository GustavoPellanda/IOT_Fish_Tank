#include "luminosity_sensor.h"
#include <stddef.h>
#include "esp_check.h"

static adc_oneshot_unit_handle_t adc_handle = NULL; // Handle for the ADC unit used by the LDR divider
static adc_channel_t adc_channel;   // ADC channel connected to the LDR divider

// Initialize the luminosity sensor:
esp_err_t luminosity_sensor_init(const luminosity_sensor_config_t *config) {
    if(config == NULL) return ESP_ERR_INVALID_ARG;

    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = config->adc_unit
    }; // Initialize the ADC unit with the specified configuration
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = config->adc_attenuation,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    }; // Configure the ADC channel with the specified attenuation and default bit width

    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_config, &adc_handle), "luminosity_sensor", "falha ao criar ADC");
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(adc_handle, config->adc_channel, &channel_config), "luminosity_sensor", "falha ao configurar canal ADC");
    adc_channel = config->adc_channel;

    return ESP_OK;
}

// Read the raw LDR value:
esp_err_t luminosity_sensor_read_raw(int *raw_value) {
    if(raw_value == NULL) return ESP_ERR_INVALID_ARG;
    if(adc_handle == NULL) return ESP_ERR_INVALID_STATE;

    return adc_oneshot_read(adc_handle, adc_channel, raw_value);
}
