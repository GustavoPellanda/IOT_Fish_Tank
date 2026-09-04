#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdbool.h>

#include "core/sensor_events.h"
#include "esp_err.h"

/* State machine for deciding the tank operating state and actuator outputs. */

typedef enum {
    SYSTEM_STATE_IDLE,
    SYSTEM_STATE_FILLING,
    SYSTEM_STATE_DRAINING,
    SYSTEM_STATE_HEATING,
    SYSTEM_STATE_MANUAL,
    SYSTEM_STATE_ERROR
} system_state_t;

typedef struct {
    system_state_t state;       // Current operating state
    float temperature_c;        // Last temperature reading in Celsius
    float level_cm;             // Last level distance reading in centimeters
    int luminosity_raw;         // Last raw luminosity ADC reading
    bool temperature_received;  // Temperature sensor has published at least one event
    bool level_received;        // Level sensor has published at least one event
    bool luminosity_received;   // Luminosity sensor has published at least one event
    bool temperature_valid;     // Last temperature reading validity
    bool level_valid;           // Last level reading validity
    bool luminosity_valid;      // Last luminosity reading validity
} state_machine_t;

esp_err_t state_machine_init(
    state_machine_t *machine    // State machine instance to initialize
);

esp_err_t state_machine_process_sensor_event(
    state_machine_t *machine,       // State machine instance to update
    const sensor_event_t *event     // Sensor event received by the control task
);

const char *state_machine_state_to_string(
    system_state_t state    // State to convert into readable text
);

#endif

/*
 * The state machine stores the latest validated sensor information,
 * evaluates the operating conditions, performs safe state transitions,
 * and applies actuator outputs through the actuator drivers.
 */
