#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "core/state_machine.h"

/* Task for controlling the actuator systems based on sensor readings. */

typedef struct {
    bool initialized;             // Control task has published its initial status
    system_state_t state;         // Current operating state
    float temperature_c;
    float level_cm;
    int luminosity_raw;
    bool temperature_received;
    bool level_received;
    bool luminosity_received;
    bool temperature_valid;
    bool level_valid;
    bool luminosity_valid;
    bool fill_enabled;            // Last successfully applied relay commands
    bool drain_enabled;
    bool heater_enabled;
    bool actuator_fault;
    bool operation_timed_out;
    int64_t temperature_updated_us;
    int64_t level_updated_us;
    int64_t luminosity_updated_us;
    uint32_t last_command_id;      // Identifier of the last processed request
    esp_err_t last_command_result; // Result of execution, separate from queue acceptance
} control_status_t;

esp_err_t control_task_start(
    QueueHandle_t *sensor_queue_out // Queue used by sensor tasks to publish readings
);

// Copy a request into the control queue without waiting for its execution:
esp_err_t control_task_send_command(
    const control_command_t *command, // Mode or pump request to enqueue
    uint32_t *command_id_out          // Identifier used to observe the result
);

esp_err_t control_task_get_status(
    control_status_t *status_out // Consistent copy of the latest published status
);

#endif
