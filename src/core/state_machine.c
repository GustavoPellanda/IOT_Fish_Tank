#include "state_machine.h"

#include <stdbool.h>
#include <math.h>
#include <stdio.h>

#include "actuators/heater/heater.h"
#include "actuators/pumps/pumps.h"
#include "esp_check.h"
#include "esp_timer.h"

#define LEVEL_EMPTY_DISTANCE_CM         25.0f
#define LEVEL_FULL_DISTANCE_CM          8.0f
#define LEVEL_TARGET_DISTANCE_CM        14.0f
#define TEMPERATURE_LOW_C               24.0f
#define TEMPERATURE_TARGET_C            26.0f
#define FILLING_TIMEOUT_US               300000000LL
#define DRAINING_TIMEOUT_US              300000000LL
#define HEATING_TIMEOUT_US               3600000000LL

// Return readable text for a system state:
const char *state_machine_state_to_string(system_state_t state) {
    switch(state) {
        case SYSTEM_STATE_IDLE:
            return "IDLE";
        case SYSTEM_STATE_FILLING:
            return "FILLING";
        case SYSTEM_STATE_DRAINING:
            return "DRAINING";
        case SYSTEM_STATE_HEATING:
            return "HEATING";
        case SYSTEM_STATE_MANUAL:
            return "MANUAL";
        case SYSTEM_STATE_ERROR:
            return "ERROR";
    }

    return "UNKNOWN";
}

// Print a state transition when the operating state changes:
static void set_state(state_machine_t *machine, system_state_t next_state, const char *reason) {
    if(machine->state == next_state) return;

    printf(
        "[STATE] Transition: %s -> %s | reason: %s\n",
        state_machine_state_to_string(machine->state),
        state_machine_state_to_string(next_state),
        reason
    );
    machine->state = next_state;
    if(next_state != SYSTEM_STATE_MANUAL) machine->manual_pump = MANUAL_PUMP_OFF;
    if(next_state == SYSTEM_STATE_FILLING || next_state == SYSTEM_STATE_DRAINING || next_state == SYSTEM_STATE_HEATING) {
        machine->operation_started_us = esp_timer_get_time();
    }
}

// Apply a pump output and record successful relay commands:
static esp_err_t set_pump_output(state_machine_t *machine, pump_id_t pump, bool enabled) {
    esp_err_t result = pumps_set_state(pump, enabled);
    if(result == ESP_OK) {
        if(pump == PUMP_FILL) machine->fill_enabled = enabled;
        else machine->drain_enabled = enabled;
    }

    return result;
}

// Apply the heater output and record successful relay commands:
static esp_err_t set_heater_output(state_machine_t *machine, bool enabled) {
    esp_err_t result = heater_set_enabled(enabled);
    if(result == ESP_OK) machine->heater_enabled = enabled;

    return result;
}

// Attempt every shutdown command and return the first failure:
static esp_err_t shutdown_outputs(state_machine_t *machine) {
    esp_err_t result = set_heater_output(machine, false);
    esp_err_t error = set_pump_output(machine, PUMP_FILL, false);
    if(result == ESP_OK) result = error;
    error = set_pump_output(machine, PUMP_DRAIN, false);
    if(result == ESP_OK) result = error;

    return result;
}

// Apply the actuator output requested by the current state:
static esp_err_t apply_outputs_for_state(state_machine_t *machine) {
    bool fill_enabled = false;
    bool drain_enabled = false;
    bool heater_enabled = false;

    switch(machine->state) {
        case SYSTEM_STATE_FILLING:
            fill_enabled = true;
            break;
        case SYSTEM_STATE_DRAINING:
            drain_enabled = true;
            break;
        case SYSTEM_STATE_HEATING:
            heater_enabled = true;
            break;
        case SYSTEM_STATE_MANUAL:
            fill_enabled = machine->manual_pump == MANUAL_PUMP_FILL;
            drain_enabled = machine->manual_pump == MANUAL_PUMP_DRAIN;
            break;
        case SYSTEM_STATE_IDLE:
        case SYSTEM_STATE_ERROR:
            break;
    }

    printf(
        "[ACTUATOR] Requested output | fill_pump=%s drain_pump=%s heater=%s\n",
        fill_enabled ? "ON" : "OFF",
        drain_enabled ? "ON" : "OFF",
        heater_enabled ? "ON" : "OFF"
    );

    if(!fill_enabled && !drain_enabled && !heater_enabled) return shutdown_outputs(machine);

    // Disable incompatible outputs before enabling the requested actuator:
    if(!heater_enabled) ESP_RETURN_ON_ERROR(set_heater_output(machine, false), "state_machine", "failed to disable heater");
    if(!fill_enabled) ESP_RETURN_ON_ERROR(set_pump_output(machine, PUMP_FILL, false), "state_machine", "failed to disable fill pump");
    if(!drain_enabled) ESP_RETURN_ON_ERROR(set_pump_output(machine, PUMP_DRAIN, false), "state_machine", "failed to disable drain pump");

    // Enable the requested actuator only after every required shutdown succeeds:
    if(fill_enabled) ESP_RETURN_ON_ERROR(set_pump_output(machine, PUMP_FILL, true), "state_machine", "failed to enable fill pump");
    if(drain_enabled) ESP_RETURN_ON_ERROR(set_pump_output(machine, PUMP_DRAIN, true), "state_machine", "failed to enable drain pump");
    if(heater_enabled) ESP_RETURN_ON_ERROR(set_heater_output(machine, true), "state_machine", "failed to enable heater");

    return ESP_OK;
}

// Record driver failures and request complete shutdown after an active-state error:
static esp_err_t apply_state_outputs(state_machine_t *machine) {
    system_state_t requested_state = machine->state;
    esp_err_t result = apply_outputs_for_state(machine);

    if(result != ESP_OK) {
        machine->actuator_fault = true;
        set_state(machine, SYSTEM_STATE_ERROR, "actuator command failed");
        if(requested_state == SYSTEM_STATE_FILLING || requested_state == SYSTEM_STATE_DRAINING || requested_state == SYSTEM_STATE_HEATING || requested_state == SYSTEM_STATE_MANUAL) {
            shutdown_outputs(machine);
        }
    }

    return result;
}

// Update the latest sensor values stored in the state machine:
static void update_sensor_context(state_machine_t *machine, const sensor_event_t *event) {
    switch(event->type) {
        case SENSOR_EVENT_TEMPERATURE:
            machine->temperature_received = true;
            machine->temperature_valid = event->valid;
            if(event->valid) machine->temperature_c = event->value.temperature_c;
            printf(
                "[STATE] Sensor update | temperature valid=%s value=%.2f C\n",
                event->valid ? "true" : "false",
                event->valid ? event->value.temperature_c : machine->temperature_c
            );
            break;
        case SENSOR_EVENT_LEVEL:
            machine->level_received = true;
            machine->level_valid = event->valid;
            if(event->valid) machine->level_cm = event->value.level_cm;
            printf(
                "[STATE] Sensor update | level valid=%s value=%.2f cm\n",
                event->valid ? "true" : "false",
                event->valid ? event->value.level_cm : machine->level_cm
            );
            break;
        case SENSOR_EVENT_LUMINOSITY:
            machine->luminosity_received = true;
            machine->luminosity_valid = event->valid;
            if(event->valid) machine->luminosity_raw = event->value.luminosity_raw;
            printf(
                "[STATE] Sensor update | luminosity valid=%s value=%d\n",
                event->valid ? "true" : "false",
                event->valid ? event->value.luminosity_raw : machine->luminosity_raw
            );
            break;
    }
}

// Check if the machine already received the critical sensor inputs:
static bool has_required_sensor_data(const state_machine_t *machine) {
    return machine->temperature_received && machine->level_received;
}

// Check if a received critical sensor is reporting invalid data:
static bool has_critical_sensor_fault(const state_machine_t *machine) {
    return (machine->temperature_received && !machine->temperature_valid) || (machine->level_received && !machine->level_valid);
}

// Evaluate the idle state transitions:
static void handle_idle_state(state_machine_t *machine) {
    if(has_critical_sensor_fault(machine)) {
        set_state(machine, SYSTEM_STATE_ERROR, "critical sensor invalid while idle");
        return;
    }

    if(!has_required_sensor_data(machine)) {
        printf("[STATE] Staying in IDLE | waiting for first temperature and level readings\n");
        return;
    }

    if(machine->level_cm > LEVEL_EMPTY_DISTANCE_CM) {
        set_state(machine, SYSTEM_STATE_FILLING, "level distance above empty threshold");
        return;
    }

    if(machine->level_cm < LEVEL_FULL_DISTANCE_CM) {
        set_state(machine, SYSTEM_STATE_DRAINING, "level distance below full threshold");
        return;
    }

    if(machine->temperature_c < TEMPERATURE_LOW_C) {
        set_state(machine, SYSTEM_STATE_HEATING, "temperature below low threshold");
        return;
    }

    printf("[STATE] Staying in IDLE | readings inside automatic limits\n");
}

// Evaluate the filling state transitions:
static void handle_filling_state(state_machine_t *machine) {
    if(has_critical_sensor_fault(machine)) {
        set_state(machine, SYSTEM_STATE_ERROR, "critical sensor invalid while filling");
        return;
    }

    if(machine->level_cm <= LEVEL_TARGET_DISTANCE_CM) {
        set_state(machine, SYSTEM_STATE_IDLE, "fill target level reached");
        return;
    }

    printf("[STATE] Staying in FILLING | level distance %.2f cm above target %.2f cm\n", machine->level_cm, LEVEL_TARGET_DISTANCE_CM);
}

// Evaluate the draining state transitions:
static void handle_draining_state(state_machine_t *machine) {
    if(has_critical_sensor_fault(machine)) {
        set_state(machine, SYSTEM_STATE_ERROR, "critical sensor invalid while draining");
        return;
    }

    if(machine->level_cm >= LEVEL_TARGET_DISTANCE_CM) {
        set_state(machine, SYSTEM_STATE_IDLE, "drain target level reached");
        return;
    }

    printf("[STATE] Staying in DRAINING | level distance %.2f cm below target %.2f cm\n", machine->level_cm, LEVEL_TARGET_DISTANCE_CM);
}

// Evaluate the heating state transitions:
static void handle_heating_state(state_machine_t *machine) {
    if(has_critical_sensor_fault(machine)) {
        set_state(machine, SYSTEM_STATE_ERROR, "critical sensor invalid while heating");
        return;
    }

    // A larger distance means less water; stop heating at the existing empty threshold:
    if(machine->level_cm > LEVEL_EMPTY_DISTANCE_CM) {
        set_state(machine, SYSTEM_STATE_IDLE, "water level too low for heating");
        return;
    }

    if(machine->temperature_c >= TEMPERATURE_TARGET_C) {
        set_state(machine, SYSTEM_STATE_IDLE, "temperature target reached");
        return;
    }

    printf("[STATE] Staying in HEATING | temperature %.2f C below target %.2f C\n", machine->temperature_c, TEMPERATURE_TARGET_C);
}

// Evaluate the manual state transitions:
static void handle_manual_state(state_machine_t *machine) {
    if(has_critical_sensor_fault(machine) || !isfinite(machine->temperature_c) || !isfinite(machine->level_cm) || machine->level_cm < 0.0f) {
        set_state(machine, SYSTEM_STATE_ERROR, "critical sensor invalid while manual mode is active");
        return;
    }

    if((machine->manual_pump == MANUAL_PUMP_FILL && machine->level_cm <= LEVEL_FULL_DISTANCE_CM) ||
       (machine->manual_pump == MANUAL_PUMP_DRAIN && machine->level_cm >= LEVEL_EMPTY_DISTANCE_CM)) {
        machine->manual_pump = MANUAL_PUMP_OFF;
        machine->operation_started_us = 0;
        printf("[STATE] Manual pump stopped at the water level limit\n");
    }
}

// Evaluate the error state transitions:
static void handle_error_state(state_machine_t *machine) {
    if(machine->operation_timed_out) {
        printf("[STATE] Staying in ERROR | operation timeout requires restart\n");
        return;
    }

    if(machine->actuator_fault) {
        printf("[STATE] Staying in ERROR | waiting for actuator shutdown\n");
        return;
    }

    if(has_critical_sensor_fault(machine)) {
        printf("[STATE] Staying in ERROR | waiting for critical sensors to recover\n");
        return;
    }

    set_state(machine, SYSTEM_STATE_IDLE, "error conditions cleared");
}

// Evaluate the current state after a context update:
static void evaluate_state(state_machine_t *machine) {
    printf("[STATE] Evaluating state: %s\n", state_machine_state_to_string(machine->state));

    switch(machine->state) {
        case SYSTEM_STATE_IDLE:
            handle_idle_state(machine);
            break;
        case SYSTEM_STATE_FILLING:
            handle_filling_state(machine);
            break;
        case SYSTEM_STATE_DRAINING:
            handle_draining_state(machine);
            break;
        case SYSTEM_STATE_HEATING:
            handle_heating_state(machine);
            break;
        case SYSTEM_STATE_MANUAL:
            handle_manual_state(machine);
            break;
        case SYSTEM_STATE_ERROR:
            handle_error_state(machine);
            break;
    }
}

// Initialize the state machine context and actuator outputs:
esp_err_t state_machine_init(state_machine_t *machine) {
    if(machine == NULL) return ESP_ERR_INVALID_ARG;

    machine->state = SYSTEM_STATE_IDLE;
    machine->temperature_c = 0.0f;
    machine->level_cm = 0.0f;
    machine->luminosity_raw = 0;
    machine->temperature_received = false;
    machine->level_received = false;
    machine->luminosity_received = false;
    machine->temperature_valid = false;
    machine->level_valid = false;
    machine->luminosity_valid = false;
    machine->actuator_fault = false;
    machine->operation_started_us = 0;
    machine->operation_timed_out = false;
    machine->manual_pump = MANUAL_PUMP_OFF;
    machine->fill_enabled = false;
    machine->drain_enabled = false;
    machine->heater_enabled = false;

    printf("[STATE] Initialized state machine | initial_state=%s\n", state_machine_state_to_string(machine->state));

    return apply_state_outputs(machine);
}

// Process one sensor event and update the system state:
esp_err_t state_machine_process_sensor_event(state_machine_t *machine, const sensor_event_t *event) {
    if(machine == NULL || event == NULL) return ESP_ERR_INVALID_ARG;

    printf("[STATE] Processing sensor event | type=%d current_state=%s\n", event->type, state_machine_state_to_string(machine->state));
    update_sensor_context(machine, event);
    if(machine->actuator_fault) return state_machine_retry_shutdown(machine);
    evaluate_state(machine);

    return apply_state_outputs(machine);
}

// Retry pending shutdown without enabling outputs or requiring a sensor event:
esp_err_t state_machine_retry_shutdown(state_machine_t *machine) {
    if(machine == NULL) return ESP_ERR_INVALID_ARG;
    if(!machine->actuator_fault) return ESP_OK;

    esp_err_t result = shutdown_outputs(machine);
    if(result != ESP_OK) return result;

    machine->actuator_fault = false;
    handle_error_state(machine);

    return ESP_OK;
}

// Stop an expired operation and block automatic recovery until initialization:
esp_err_t state_machine_check_operation_timeout(state_machine_t *machine, int64_t now_us) {
    if(machine == NULL || now_us < 0) return ESP_ERR_INVALID_ARG;
    if(machine->operation_timed_out) return ESP_OK;

    int64_t timeout_us;
    switch(machine->state) {
        case SYSTEM_STATE_FILLING:
            timeout_us = FILLING_TIMEOUT_US;
            break;
        case SYSTEM_STATE_DRAINING:
            timeout_us = DRAINING_TIMEOUT_US;
            break;
        case SYSTEM_STATE_HEATING:
            timeout_us = HEATING_TIMEOUT_US;
            break;
        case SYSTEM_STATE_MANUAL:
            if(machine->manual_pump == MANUAL_PUMP_OFF) return ESP_OK;
            timeout_us = machine->manual_pump == MANUAL_PUMP_FILL ? FILLING_TIMEOUT_US : DRAINING_TIMEOUT_US;
            break;
        default:
            return ESP_OK;
    }
    if(now_us < machine->operation_started_us) return ESP_ERR_INVALID_ARG;
    if(now_us - machine->operation_started_us < timeout_us) return ESP_OK;

    machine->operation_timed_out = true;
    set_state(machine, SYSTEM_STATE_ERROR, "operation duration limit reached");

    return apply_state_outputs(machine);
}

// Validate a manual request and apply it without bypassing fault supervision:
esp_err_t state_machine_process_command(state_machine_t *machine, const control_command_t *command) {
    if(machine == NULL || command == NULL) return ESP_ERR_INVALID_ARG;
    if(command->type != CONTROL_COMMAND_SET_MANUAL_MODE && command->type != CONTROL_COMMAND_SET_PUMP_STATE) return ESP_ERR_INVALID_ARG;
    if(command->type == CONTROL_COMMAND_SET_PUMP_STATE && command->pump != MANUAL_PUMP_FILL && command->pump != MANUAL_PUMP_DRAIN) return ESP_ERR_INVALID_ARG;

    esp_err_t result = state_machine_check_operation_timeout(machine, esp_timer_get_time());
    if(result != ESP_OK) return result;
    if(machine->actuator_fault || machine->operation_timed_out || machine->state == SYSTEM_STATE_ERROR) return ESP_ERR_INVALID_STATE;

    if(command->type == CONTROL_COMMAND_SET_MANUAL_MODE) {
        if(!command->enabled) {
            if(machine->state != SYSTEM_STATE_MANUAL) return ESP_OK;
            set_state(machine, SYSTEM_STATE_IDLE, "manual mode disabled");
            return apply_state_outputs(machine);
        }
        if(!has_required_sensor_data(machine) || has_critical_sensor_fault(machine) || !isfinite(machine->temperature_c) || !isfinite(machine->level_cm) || machine->level_cm < 0.0f) return ESP_ERR_INVALID_STATE;
        if(machine->state == SYSTEM_STATE_MANUAL) return ESP_OK;

        machine->manual_pump = MANUAL_PUMP_OFF;
        set_state(machine, SYSTEM_STATE_MANUAL, "manual mode enabled");
        return apply_state_outputs(machine);
    }
    if(machine->state != SYSTEM_STATE_MANUAL) return ESP_ERR_INVALID_STATE;

    manual_pump_state_t previous_pump = machine->manual_pump;
    handle_manual_state(machine);
    if(machine->state == SYSTEM_STATE_ERROR) {
        result = apply_state_outputs(machine);
        return result == ESP_OK ? ESP_ERR_INVALID_STATE : result;
    }
    if(machine->manual_pump != previous_pump) {
        result = apply_state_outputs(machine);
        if(result != ESP_OK) return result;
    }
    if(command->enabled) {
        if(command->pump == MANUAL_PUMP_FILL && machine->level_cm <= LEVEL_FULL_DISTANCE_CM) return ESP_ERR_INVALID_STATE;
        if(command->pump == MANUAL_PUMP_DRAIN && machine->level_cm >= LEVEL_EMPTY_DISTANCE_CM) return ESP_ERR_INVALID_STATE;
        if(machine->manual_pump != command->pump) {
            machine->manual_pump = command->pump;
            machine->operation_started_us = esp_timer_get_time();
        }
    }
    else if(machine->manual_pump == command->pump) {
        machine->manual_pump = MANUAL_PUMP_OFF;
        machine->operation_started_us = 0;
    }

    return apply_state_outputs(machine);
}
