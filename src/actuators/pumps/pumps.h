#ifndef PUMPS_H
#define PUMPS_H

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"

/* Driver for controlling the pumps */

// Enumeration for identifying which of the pumps is being controlled:
typedef enum {
    PUMP_FILL,
    PUMP_DRAIN
} pump_id_t;

typedef struct {
    gpio_num_t fill_gpio;   // GPIO connected to the clean water pump relay
    gpio_num_t drain_gpio;  // GPIO connected to the dirty water pump relay
    bool active_level;  // Relay activation level
} pumps_config_t;

esp_err_t pumps_init(
    const pumps_config_t *config    // Pump relay wiring and activation level
);

esp_err_t pumps_set_state(
    pump_id_t pump, // Pump to be changed
    bool enabled    // Desired relay state
);

#endif

/*
 * How the actuator works:
 * The fill and drain pumps are controlled through relay outputs connected to
 * GPIO pins. During initialization, both GPIOs are configured as outputs and
 * both pumps are turned off. When changing a pump state, the driver writes the
 * configured active level to enable the relay, or the opposite level to disable it.
 */
