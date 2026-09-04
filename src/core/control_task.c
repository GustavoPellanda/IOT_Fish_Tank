#include "control_task.h"

#include <stdio.h>

#include "core/sensor_events.h"
#include "core/state_machine.h"
#include "freertos/task.h"

#define CONTROL_QUEUE_LENGTH       12
#define CONTROL_TASK_STACK_SIZE    3072
#define CONTROL_TASK_PRIORITY      3
#define CONTROL_TASK_CORE          1

// Run the main control logic:
static void control_task(void *arg) {
    QueueHandle_t sensor_queue = (QueueHandle_t)arg;
    state_machine_t state_machine;
    sensor_event_t event;

    if(state_machine_init(&state_machine) != ESP_OK) {
        printf("[CONTROL] Failed to initialize state machine\n");
        vTaskDelete(NULL);
        return;
    }

    printf("[CONTROL] Control task started and waiting for sensor events\n");

    while(1) {
        if(xQueueReceive(sensor_queue, &event, portMAX_DELAY) == pdTRUE) {
            printf("[CONTROL] Received sensor event | type=%d valid=%s\n", event.type, event.valid ? "true" : "false");

            if(state_machine_process_sensor_event(&state_machine, &event) != ESP_OK) {
                printf("[CONTROL] State machine failed to process event | type=%d\n", event.type);
            }
        }
    }
}

// Create the sensor queue and start the control task:
esp_err_t control_task_start(QueueHandle_t *sensor_queue_out) {
    QueueHandle_t sensor_queue;
    BaseType_t task_result;

    if(sensor_queue_out == NULL) return ESP_ERR_INVALID_ARG;

    sensor_queue = xQueueCreate(CONTROL_QUEUE_LENGTH, sizeof(sensor_event_t));
    if(sensor_queue == NULL) return ESP_ERR_NO_MEM;

    task_result = xTaskCreatePinnedToCore(
        control_task,
        "control_task",
        CONTROL_TASK_STACK_SIZE,
        sensor_queue,
        CONTROL_TASK_PRIORITY,
        NULL,
        CONTROL_TASK_CORE
    );
    if(task_result != pdPASS) return ESP_ERR_NO_MEM;

    *sensor_queue_out = sensor_queue;

    return ESP_OK;
}
