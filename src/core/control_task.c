#include "control_task.h"

#include <stdio.h>

#include "core/sensor_events.h"
#include "core/state_machine.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define CONTROL_QUEUE_LENGTH       12
#define CONTROL_TASK_STACK_SIZE    3072
#define CONTROL_TASK_PRIORITY      3
#define CONTROL_TASK_CORE          1
#define CONTROL_QUEUE_WAIT_MS      100
#define COMMAND_QUEUE_LENGTH       8
#define COMMANDS_PER_ITERATION     4
#define STATUS_COPY_WAIT_MS        20

typedef struct {
    control_command_t command;
    uint32_t id;
} queued_command_t;

static QueueHandle_t command_queue;
static SemaphoreHandle_t status_mutex;
static control_status_t current_status;
static uint32_t next_command_id;

// Publish a consistent snapshot without exposing the state machine to another task:
static void publish_status(const state_machine_t *machine, control_status_t *status) {
    status->initialized = true;
    status->state = machine->state;
    status->temperature_c = machine->temperature_c;
    status->level_cm = machine->level_cm;
    status->luminosity_raw = machine->luminosity_raw;
    status->temperature_received = machine->temperature_received;
    status->level_received = machine->level_received;
    status->luminosity_received = machine->luminosity_received;
    status->temperature_valid = machine->temperature_valid;
    status->level_valid = machine->level_valid;
    status->luminosity_valid = machine->luminosity_valid;
    status->fill_enabled = machine->fill_enabled;
    status->drain_enabled = machine->drain_enabled;
    status->heater_enabled = machine->heater_enabled;
    status->actuator_fault = machine->actuator_fault;
    status->operation_timed_out = machine->operation_timed_out;
    xSemaphoreTake(status_mutex, portMAX_DELAY);
    current_status = *status;
    xSemaphoreGive(status_mutex);
}

// Record when a sensor event was consumed, including invalid readings:
static void record_sensor_update(control_status_t *status, sensor_event_type_t type) {
    int64_t now_us = esp_timer_get_time();
    switch(type) {
        case SENSOR_EVENT_TEMPERATURE:
            status->temperature_updated_us = now_us;
            break;
        case SENSOR_EVENT_LEVEL:
            status->level_updated_us = now_us;
            break;
        case SENSOR_EVENT_LUMINOSITY:
            status->luminosity_updated_us = now_us;
            break;
    }
}

// Run the main control logic:
static void control_task(void *arg) {
    QueueHandle_t sensor_queue = (QueueHandle_t)arg;
    state_machine_t state_machine;
    sensor_event_t event;
    control_status_t status = {0};
    queued_command_t request;

    if(state_machine_init(&state_machine) != ESP_OK) {
        printf("[CONTROL] Failed to initialize state machine\n");
    }

    printf("[CONTROL] Control task started and waiting for sensor events\n");
    publish_status(&state_machine, &status);

    while(1) {
        BaseType_t event_received = xQueueReceive(sensor_queue, &event, pdMS_TO_TICKS(CONTROL_QUEUE_WAIT_MS));
        if(state_machine_check_operation_timeout(&state_machine, esp_timer_get_time()) != ESP_OK) {
            printf("[CONTROL] Failed to shut down an expired operation\n");
        }

        if(event_received == pdTRUE) {
            record_sensor_update(&status, event.type);
            printf("[CONTROL] Received sensor event | type=%d valid=%s\n", event.type, event.valid ? "true" : "false");

            if(state_machine_process_sensor_event(&state_machine, &event) != ESP_OK) {
                printf("[CONTROL] State machine failed to process event | type=%d\n", event.type);
            }
        }
        else if(state_machine_retry_shutdown(&state_machine) != ESP_OK) {
            printf("[CONTROL] Failed to retry actuator shutdown\n");
        }
        for(int count = 0; count < COMMANDS_PER_ITERATION; count++) {
            if(xQueueReceive(command_queue, &request, 0) != pdTRUE) break;
            status.last_command_result = state_machine_process_command(&state_machine, &request.command);
            status.last_command_id = request.id;
            printf("[CONTROL] Command %lu processed | result=%s\n", (unsigned long)request.id, esp_err_to_name(status.last_command_result));
        }
        publish_status(&state_machine, &status);
    }
}

// Create the sensor queue and start the control task:
esp_err_t control_task_start(QueueHandle_t *sensor_queue_out) {
    QueueHandle_t sensor_queue;
    BaseType_t task_result;

    if(sensor_queue_out == NULL) return ESP_ERR_INVALID_ARG;
    if(command_queue != NULL || status_mutex != NULL) return ESP_ERR_INVALID_STATE;

    sensor_queue = xQueueCreate(CONTROL_QUEUE_LENGTH, sizeof(sensor_event_t));
    if(sensor_queue == NULL) return ESP_ERR_NO_MEM;
    command_queue = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(queued_command_t));
    status_mutex = xSemaphoreCreateMutex();
    if(command_queue == NULL || status_mutex == NULL) {
        if(command_queue != NULL) vQueueDelete(command_queue);
        if(status_mutex != NULL) vSemaphoreDelete(status_mutex);
        command_queue = NULL;
        status_mutex = NULL;
        vQueueDelete(sensor_queue);
        return ESP_ERR_NO_MEM;
    }

    task_result = xTaskCreatePinnedToCore(
        control_task,
        "control_task",
        CONTROL_TASK_STACK_SIZE,
        sensor_queue,
        CONTROL_TASK_PRIORITY,
        NULL,
        CONTROL_TASK_CORE
    );
    if(task_result != pdPASS) {
        vQueueDelete(command_queue);
        vSemaphoreDelete(status_mutex);
        vQueueDelete(sensor_queue);
        command_queue = NULL;
        status_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    *sensor_queue_out = sensor_queue;

    return ESP_OK;
}

// Enqueue an identified request without blocking the HTTP task on control execution:
esp_err_t control_task_send_command(const control_command_t *command, uint32_t *command_id_out) {
    if(command == NULL || command_id_out == NULL) return ESP_ERR_INVALID_ARG;
    if(command_queue == NULL || status_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if(command->type != CONTROL_COMMAND_SET_MANUAL_MODE && command->type != CONTROL_COMMAND_SET_PUMP_STATE) return ESP_ERR_INVALID_ARG;
    if(command->type == CONTROL_COMMAND_SET_PUMP_STATE && command->pump != MANUAL_PUMP_FILL && command->pump != MANUAL_PUMP_DRAIN) return ESP_ERR_INVALID_ARG;
    if(xSemaphoreTake(status_mutex, pdMS_TO_TICKS(STATUS_COPY_WAIT_MS)) != pdTRUE) return ESP_ERR_TIMEOUT;

    queued_command_t request = {
        .command = *command,
        .id = next_command_id + 1
    };
    if(request.id == 0) request.id = 1;
    BaseType_t queued = xQueueSend(command_queue, &request, 0);
    if(queued == pdTRUE) {
        next_command_id = request.id;
        *command_id_out = request.id;
    }
    xSemaphoreGive(status_mutex);

    return queued == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

// Return a short-lived copy of the published status:
esp_err_t control_task_get_status(control_status_t *status_out) {
    if(status_out == NULL) return ESP_ERR_INVALID_ARG;
    if(status_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if(xSemaphoreTake(status_mutex, pdMS_TO_TICKS(STATUS_COPY_WAIT_MS)) != pdTRUE) return ESP_ERR_TIMEOUT;

    *status_out = current_status;
    xSemaphoreGive(status_mutex);

    return status_out->initialized ? ESP_OK : ESP_ERR_INVALID_STATE;
}
