#ifndef FAKE_ACTUATORS_H
#define FAKE_ACTUATORS_H

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#define FAKE_ACTUATOR_COMMAND_CAPACITY 64

typedef enum {
    FAKE_ACTUATOR_FILL_PUMP,
    FAKE_ACTUATOR_DRAIN_PUMP,
    FAKE_ACTUATOR_HEATER,
    FAKE_ACTUATOR_COUNT
} fake_actuator_id_t;

typedef struct {
    fake_actuator_id_t actuator;
    bool enabled;
} fake_actuator_command_t;

typedef struct {
    bool fill_enabled;
    bool drain_enabled;
    bool heater_enabled;
    bool conflicting_outputs_seen; // Records overlap even if later commands clear it
    bool command_log_overflow;
    size_t command_count;
    fake_actuator_command_t commands[FAKE_ACTUATOR_COMMAND_CAPACITY];
} fake_actuators_state_t;

/* In-memory replacements for the actuator drivers; no GPIO is accessed. */
void fake_actuators_reset(void);
const fake_actuators_state_t *fake_actuators_get_state(void);

void fake_actuators_fail_next(
    fake_actuator_id_t actuator, // Driver whose next call must fail
    esp_err_t error              // Error returned without changing its simulated output
);

#endif
