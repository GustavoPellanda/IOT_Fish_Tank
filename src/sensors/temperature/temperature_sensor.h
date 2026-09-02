#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include "driver/gpio.h"
#include "esp_err.h"

/* Driver for the DS18B20 temperature sensor */

typedef struct {
    gpio_num_t data_gpio;   // GPIO connected to the DS18B20 data line
} temperature_sensor_config_t;

esp_err_t temperature_sensor_init(
    const temperature_sensor_config_t *config   // Sensor wiring and bus configuration
);

esp_err_t temperature_sensor_read_celsius(
    float *temperature_c    // Water temperature in degrees Celsius
);

#endif

/*
 * How the sensor works:
 * The DS18B20 is connected to a single GPIO configured as a 1-Wire bus.
 * During initialization, the bus is prepared and the sensor is discovered.
 * When reading, the driver sends the conversion command, waits for the sensor
 * to measure the temperature, reads the raw 12-bit value from its scratchpad,
 * and converts it into degrees Celsius before returning it to the caller.
 */