#include "state_machine.h"

#include <stdbool.h>
#include <stdio.h>

#include "actuators/heater/heater.h"
#include "actuators/pumps/pumps.h"
#include "esp_check.h"

#define LEVEL_EMPTY_DISTANCE_CM         25.0f
#define LEVEL_FULL_DISTANCE_CM          8.0f
#define LEVEL_TARGET_DISTANCE_CM        14.0f
#define TEMPERATURE_LOW_C               24.0f
#define TEMPERATURE_TARGET_C            26.0f

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
}

// Apply the actuator output requested by the current state:
static esp_err_t apply_outputs_for_state(system_state_t state) {
    bool fill_enabled = false;
    bool drain_enabled = false;
    bool heater_enabled = false;

    switch(state) {
        case SYSTEM_STATE_FILLING:
            fill_enabled = true;
            break;
        case SYSTEM_STATE_DRAINING:
            drain_enabled = true;
            break;
        case SYSTEM_STATE_HEATING:
            heater_enabled = true;
            break;
        case SYSTEM_STATE_IDLE:
        case SYSTEM_STATE_MANUAL:
        case SYSTEM_STATE_ERROR:
            break;
    }

    printf(
        "[ACTUATOR] Requested output | fill_pump=%s drain_pump=%s heater=%s\n",
        fill_enabled ? "ON" : "OFF",
        drain_enabled ? "ON" : "OFF",
        heater_enabled ? "ON" : "OFF"
    );
    ESP_RETURN_ON_ERROR(pumps_set_state(PUMP_FILL, fill_enabled), "state_machine", "failed to set fill pump");
    ESP_RETURN_ON_ERROR(pumps_set_state(PUMP_DRAIN, drain_enabled), "state_machine", "failed to set drain pump");
    ESP_RETURN_ON_ERROR(heater_set_enabled(heater_enabled), "state_machine", "failed to set heater");

    return ESP_OK;
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

    if(machine->temperature_c >= TEMPERATURE_TARGET_C) {
        set_state(machine, SYSTEM_STATE_IDLE, "temperature target reached");
        return;
    }

    printf("[STATE] Staying in HEATING | temperature %.2f C below target %.2f C\n", machine->temperature_c, TEMPERATURE_TARGET_C);
}

// Evaluate the manual state transitions:
static void handle_manual_state(state_machine_t *machine) {
    if(has_critical_sensor_fault(machine)) {
        set_state(machine, SYSTEM_STATE_ERROR, "critical sensor invalid while manual mode is active");
        return;
    }

    printf("[STATE] Staying in MANUAL | manual command handling is not implemented yet\n");
}

// Evaluate the error state transitions:
static void handle_error_state(state_machine_t *machine) {
    if(has_critical_sensor_fault(machine)) {
        printf("[STATE] Staying in ERROR | waiting for critical sensors to recover\n");
        return;
    }

    set_state(machine, SYSTEM_STATE_IDLE, "critical sensors recovered");
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

    printf("[STATE] Initialized state machine | initial_state=%s\n", state_machine_state_to_string(machine->state));

    return apply_outputs_for_state(machine->state);
}

// Process one sensor event and update the system state:
esp_err_t state_machine_process_sensor_event(state_machine_t *machine, const sensor_event_t *event) {
    if(machine == NULL || event == NULL) return ESP_ERR_INVALID_ARG;

    printf("[STATE] Processing sensor event | type=%d current_state=%s\n", event->type, state_machine_state_to_string(machine->state));
    update_sensor_context(machine, event);
    evaluate_state(machine);

    return apply_outputs_for_state(machine->state);
}
