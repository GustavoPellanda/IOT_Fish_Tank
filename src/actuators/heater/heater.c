#include "heater.h"
#include <stddef.h>
#include "esp_check.h"

static heater_config_t heater_config = {
    .relay_gpio = GPIO_NUM_NC,
    .active_level = true // Default to active high, can be changed during initialization
};

// Initialize the heater relay:
esp_err_t heater_init(const heater_config_t *config) {
    if(config == NULL) return ESP_ERR_INVALID_ARG;

    heater_config = *config;
    gpio_config_t gpio_config_data = {
        .pin_bit_mask = 1ULL << heater_config.relay_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_RETURN_ON_ERROR(gpio_config(&gpio_config_data), "heater", "falha ao configurar GPIO");
    return heater_set_enabled(false);
}

// Change the heater state:
esp_err_t heater_set_enabled(bool enabled) {
    int level = enabled ? heater_config.active_level : !heater_config.active_level; // Determine the GPIO level based on the desired state and active level

    if(heater_config.relay_gpio == GPIO_NUM_NC) return ESP_ERR_INVALID_STATE;

    return gpio_set_level(heater_config.relay_gpio, level);
}
