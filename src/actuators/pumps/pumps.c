#include "pumps.h"

#include <stddef.h>

#include "esp_check.h"

static pumps_config_t pumps_config = {
    .fill_gpio = GPIO_NUM_NC,
    .drain_gpio = GPIO_NUM_NC,
    .active_level = true
};

// Return the GPIO associated with the selected pump:
static gpio_num_t get_pump_gpio(pump_id_t pump) {
    if(pump == PUMP_FILL) return pumps_config.fill_gpio;
    return pumps_config.drain_gpio;
}

// Initialize the pump relays:
esp_err_t pumps_init(const pumps_config_t *config) {
    if(config == NULL) return ESP_ERR_INVALID_ARG;

    pumps_config = *config;
    gpio_config_t gpio_config_data = {
        .pin_bit_mask = (1ULL << pumps_config.fill_gpio) | (1ULL << pumps_config.drain_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_RETURN_ON_ERROR(gpio_config(&gpio_config_data), "pumps", "falha ao configurar GPIO");
    ESP_RETURN_ON_ERROR(pumps_set_state(PUMP_FILL, false), "pumps", "falha ao desligar bomba");
    ESP_RETURN_ON_ERROR(pumps_set_state(PUMP_DRAIN, false), "pumps", "falha ao desligar bomba");

    return ESP_OK;
}

// Change a pump state:
esp_err_t pumps_set_state(pump_id_t pump, bool enabled) {
    gpio_num_t gpio = get_pump_gpio(pump);
    int level = enabled ? pumps_config.active_level : !pumps_config.active_level;

    if(gpio == GPIO_NUM_NC) return ESP_ERR_INVALID_STATE;

    return gpio_set_level(gpio, level);
}
