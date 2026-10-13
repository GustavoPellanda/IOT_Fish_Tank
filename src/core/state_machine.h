#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

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

typedef enum {
    MANUAL_PUMP_OFF,
    MANUAL_PUMP_FILL,
    MANUAL_PUMP_DRAIN
} manual_pump_state_t;

typedef enum {
    CONTROL_COMMAND_SET_MANUAL_MODE,
    CONTROL_COMMAND_SET_PUMP_STATE
} control_command_type_t;

typedef struct {
    control_command_type_t type; // Requested mode or pump change
    manual_pump_state_t pump;    // Pump selected for a pump command
    bool enabled;                // Desired manual mode or pump state
} control_command_t;

typedef struct {
    system_state_t state;            // Current operating state
    float temperature_c;             // Last temperature reading in Celsius
    float level_cm;                  // Last level distance reading in centimeters
    int luminosity_raw;              // Last raw luminosity ADC reading
    bool temperature_received;       // Temperature sensor has published at least one event
    bool level_received;             // Level sensor has published at least one event
    bool luminosity_received;        // Luminosity sensor has published at least one event
    bool temperature_valid;          // Last temperature reading validity
    bool level_valid;                // Last level reading validity
    bool luminosity_valid;           // Last luminosity reading validity
    bool actuator_fault;             // Actuator fault awaiting successful shutdown recovery
    int64_t operation_started_us;    // Monotonic time when the current operation started
    bool operation_timed_out;        // Operation timeout latched until initialization
    manual_pump_state_t manual_pump; // Requested pump while MANUAL is active
    bool fill_enabled;               // Last successfully applied fill relay command
    bool drain_enabled;              // Last successfully applied drain relay command
    bool heater_enabled;             // Last successfully applied heater relay command
} state_machine_t;

esp_err_t state_machine_init(
    state_machine_t *machine    // State machine instance to initialize
);

esp_err_t state_machine_process_sensor_event(
    state_machine_t *machine,       // State machine instance to update
    const sensor_event_t *event     // Sensor event received by the control task
);

esp_err_t state_machine_process_command(
    state_machine_t *machine,       // State machine owned by the control task
    const control_command_t *command // Manual request to validate and apply
);

esp_err_t state_machine_retry_shutdown(
    state_machine_t *machine    // State machine with a pending actuator shutdown
);

esp_err_t state_machine_check_operation_timeout(
    state_machine_t *machine,   // State machine whose active operation is supervised
    int64_t now_us              // Current monotonic time in microseconds
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
