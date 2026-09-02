#ifndef HEATER_H
#define HEATER_H

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"

/* Driver for controlling the heater relay */

typedef struct {
    gpio_num_t relay_gpio;  // GPIO connected to the heater relay
    bool active_level;      // Relay activation level
} heater_config_t;

esp_err_t heater_init(
    const heater_config_t *config   // Heater relay wiring and activation level
);

esp_err_t heater_set_enabled(
    bool enabled    // Desired relay state
);

#endif

/*
 * How the actuator works:
 * The heater is controlled through a relay output connected to one GPIO pin.
 * During initialization, the GPIO is configured as output and the heater is
 * turned off. When changing the heater state, the driver writes the configured
 * active level to enable the relay, or the opposite level to disable it.
 */
