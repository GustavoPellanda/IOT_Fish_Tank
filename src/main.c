#include <stdio.h>

#include "actuators/heater/heater.h"
#include "actuators/pumps/pumps.h"
#include "core/control_task.h"
#include "core/sensor_tasks.h"
#include "esp_check.h"
#include "sensors/level/level_sensor.h"
#include "sensors/luminosity/luminosity_sensor.h"
#include "sensors/temperature/temperature_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define TEMPERATURE_DATA_GPIO      GPIO_NUM_32
#define LEVEL_TRIGGER_GPIO         GPIO_NUM_5
#define LEVEL_ECHO_GPIO            GPIO_NUM_18
#define LEVEL_TIMEOUT_US           30000
#define LUMINOSITY_ADC_UNIT        ADC_UNIT_1
#define LUMINOSITY_ADC_CHANNEL     ADC_CHANNEL_6
#define PUMP_FILL_GPIO             GPIO_NUM_26
#define PUMP_DRAIN_GPIO            GPIO_NUM_27
#define HEATER_RELAY_GPIO          GPIO_NUM_25
#define RELAY_ACTIVE_LEVEL         true

// Initialize the sensor drivers:
static esp_err_t init_sensors(void) {
    temperature_sensor_config_t temperature_config = {
        .data_gpio = TEMPERATURE_DATA_GPIO
    };
    level_sensor_config_t level_config = {
        .trigger_gpio = LEVEL_TRIGGER_GPIO,
        .echo_gpio = LEVEL_ECHO_GPIO,
        .timeout_us = LEVEL_TIMEOUT_US
    };
    luminosity_sensor_config_t luminosity_config = {
        .adc_unit = LUMINOSITY_ADC_UNIT,
        .adc_channel = LUMINOSITY_ADC_CHANNEL,
        .adc_attenuation = ADC_ATTEN_DB_12
    };

    ESP_RETURN_ON_ERROR(temperature_sensor_init(&temperature_config), "main", "failed to initialize temperature sensor");
    ESP_RETURN_ON_ERROR(level_sensor_init(&level_config), "main", "failed to initialize level sensor");
    ESP_RETURN_ON_ERROR(luminosity_sensor_init(&luminosity_config), "main", "failed to initialize luminosity sensor");

    return ESP_OK;
}

// Initialize the actuator drivers:
static esp_err_t init_actuators(void) {
    pumps_config_t pumps_config = {
        .fill_gpio = PUMP_FILL_GPIO,
        .drain_gpio = PUMP_DRAIN_GPIO,
        .active_level = RELAY_ACTIVE_LEVEL
    };
    heater_config_t heater_config = {
        .relay_gpio = HEATER_RELAY_GPIO,
        .active_level = RELAY_ACTIVE_LEVEL
    };

    ESP_RETURN_ON_ERROR(pumps_init(&pumps_config), "main", "failed to initialize pumps");
    ESP_RETURN_ON_ERROR(heater_init(&heater_config), "main", "failed to initialize heater");

    return ESP_OK;
}

// Initialize the firmware base:
void app_main(void) {
    QueueHandle_t sensor_queue;

    ESP_ERROR_CHECK(init_sensors());
    ESP_ERROR_CHECK(init_actuators());
    ESP_ERROR_CHECK(control_task_start(&sensor_queue));
    ESP_ERROR_CHECK(sensor_tasks_start(sensor_queue));
}
