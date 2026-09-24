#include "fake_actuators.h"

#include "actuators/heater/heater.h"
#include "actuators/pumps/pumps.h"

static fake_actuators_state_t outputs;
static esp_err_t next_errors[FAKE_ACTUATOR_COUNT];

// Clear simulated outputs, command history, and pending driver failures:
void fake_actuators_reset(void) {
    outputs = (fake_actuators_state_t){0};
    for(int index = 0; index < FAKE_ACTUATOR_COUNT; index++) {
        next_errors[index] = ESP_OK;
    }
}

const fake_actuators_state_t *fake_actuators_get_state(void) {
    return &outputs;
}

void fake_actuators_fail_next(fake_actuator_id_t actuator, esp_err_t error) {
    next_errors[actuator] = error;
}

// Record a driver call and apply the output only when the simulated call succeeds:
static esp_err_t apply_command(fake_actuator_id_t actuator, bool enabled) {
    if(outputs.command_count < FAKE_ACTUATOR_COMMAND_CAPACITY) {
        outputs.commands[outputs.command_count++] = (fake_actuator_command_t){
            .actuator = actuator,
            .enabled = enabled
        };
    }
    else {
        outputs.command_log_overflow = true;
    }

    esp_err_t error = next_errors[actuator];
    next_errors[actuator] = ESP_OK;
    if(error != ESP_OK) return error;

    switch(actuator) {
        case FAKE_ACTUATOR_FILL_PUMP:
            outputs.fill_enabled = enabled;
            break;
        case FAKE_ACTUATOR_DRAIN_PUMP:
            outputs.drain_enabled = enabled;
            break;
        case FAKE_ACTUATOR_HEATER:
            outputs.heater_enabled = enabled;
            break;
        case FAKE_ACTUATOR_COUNT:
            return ESP_ERR_INVALID_ARG;
    }

    if((outputs.fill_enabled && outputs.drain_enabled) ||
       (outputs.heater_enabled && (outputs.fill_enabled || outputs.drain_enabled))) {
        outputs.conflicting_outputs_seen = true;
    }

    return ESP_OK;
}

// Replace the production pump setter with a recorded command:
esp_err_t pumps_set_state(pump_id_t pump, bool enabled) {
    if(pump == PUMP_FILL) return apply_command(FAKE_ACTUATOR_FILL_PUMP, enabled);
    if(pump == PUMP_DRAIN) return apply_command(FAKE_ACTUATOR_DRAIN_PUMP, enabled);
    return ESP_ERR_INVALID_ARG;
}

// Replace the production heater setter with a recorded command:
esp_err_t heater_set_enabled(bool enabled) {
    return apply_command(FAKE_ACTUATOR_HEATER, enabled);
}
