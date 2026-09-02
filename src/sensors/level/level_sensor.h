#ifndef LEVEL_SENSOR_H
#define LEVEL_SENSOR_H

#include "driver/gpio.h"
#include "esp_err.h"

/* Driver for the HC-SR04 level sensor */

typedef struct {
    gpio_num_t trigger_gpio;    // GPIO connected to the HC-SR04 trigger pin
    gpio_num_t echo_gpio;       // GPIO connected to the HC-SR04 echo pin
    uint32_t timeout_us;        // Maximum echo wait time in microseconds
} level_sensor_config_t;

esp_err_t level_sensor_init(
    const level_sensor_config_t *config // Sensor wiring and timeout configuration
);

esp_err_t level_sensor_read_distance_cm(
    float *distance_cm  // Distance from sensor to water surface
);

#endif

/*
 * How the sensor works:
 * The HC-SR04 uses one GPIO as trigger and another as echo input.
 * During initialization, the trigger pin is configured as output and the echo
 * pin as input. When reading, the driver sends a short trigger pulse, measures
 * how long the echo signal stays high, and converts that pulse duration into
 * the distance from the sensor to the water surface.
 */
