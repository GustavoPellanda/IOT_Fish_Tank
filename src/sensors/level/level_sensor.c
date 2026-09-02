#include "level_sensor.h"

#include <stddef.h>

#include "esp_check.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#define SOUND_SPEED_CM_PER_US 0.0343f

static level_sensor_config_t sensor_config = {
    .trigger_gpio = GPIO_NUM_NC,
    .echo_gpio = GPIO_NUM_NC,
    .timeout_us = 30000
};

// Wait for the echo pin to reach the expected level:
static esp_err_t wait_for_echo_level(int expected_level, int64_t timeout_at_us) {
    while(gpio_get_level(sensor_config.echo_gpio) != expected_level) {
        if(esp_timer_get_time() > timeout_at_us) return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

// Initialize the level sensor:
esp_err_t level_sensor_init(const level_sensor_config_t *config) {
    if(config == NULL) return ESP_ERR_INVALID_ARG;

    sensor_config = *config;
    gpio_config_t trigger_config = {
        .pin_bit_mask = 1ULL << sensor_config.trigger_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config_t echo_config = {
        .pin_bit_mask = 1ULL << sensor_config.echo_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_RETURN_ON_ERROR(gpio_config(&trigger_config), "level_sensor", "falha ao configurar trigger");
    ESP_RETURN_ON_ERROR(gpio_config(&echo_config), "level_sensor", "falha ao configurar echo");
    gpio_set_level(sensor_config.trigger_gpio, 0);

    return ESP_OK;
}

// Read the distance to the water surface:
esp_err_t level_sensor_read_distance_cm(float *distance_cm) {
    int64_t timeout_at_us;
    int64_t pulse_start_us;
    int64_t pulse_end_us;

    if(distance_cm == NULL) return ESP_ERR_INVALID_ARG;
    if(sensor_config.trigger_gpio == GPIO_NUM_NC) return ESP_ERR_INVALID_STATE;

    // Send the HC-SR04 trigger pulse:
    gpio_set_level(sensor_config.trigger_gpio, 0);
    esp_rom_delay_us(2);
    gpio_set_level(sensor_config.trigger_gpio, 1);
    esp_rom_delay_us(10);
    gpio_set_level(sensor_config.trigger_gpio, 0);

    // Measure the echo pulse width with a timeout on both edges:
    timeout_at_us = esp_timer_get_time() + sensor_config.timeout_us;
    ESP_RETURN_ON_ERROR(wait_for_echo_level(1, timeout_at_us), "level_sensor", "timeout aguardando echo");

    pulse_start_us = esp_timer_get_time();
    timeout_at_us = pulse_start_us + sensor_config.timeout_us;
    ESP_RETURN_ON_ERROR(wait_for_echo_level(0, timeout_at_us), "level_sensor", "timeout medindo echo");

    pulse_end_us = esp_timer_get_time();
    *distance_cm = (float)(pulse_end_us - pulse_start_us) * SOUND_SPEED_CM_PER_US / 2.0f;

    return ESP_OK;
}
