#include "control_task.h"

#include <stdio.h>

#include "core/sensor_events.h"
#include "freertos/task.h"

#define CONTROL_QUEUE_LENGTH       12
#define CONTROL_TASK_STACK_SIZE    3072
#define CONTROL_TASK_PRIORITY      3
#define CONTROL_TASK_CORE          1

// Process an event received from the sensors:
static void process_sensor_event(const sensor_event_t *event) {
    if(!event->valid) {
        printf("[CONTROL] Sensor event invalid: %d\n", event->type);
        return;
    }

    switch(event->type) {
        case SENSOR_EVENT_TEMPERATURE:
            printf("[CONTROL] Temperature: %.2f C\n", event->value.temperature_c);
            break;
        case SENSOR_EVENT_LEVEL:
            printf("[CONTROL] Level distance: %.2f cm\n", event->value.level_cm);
            break;
        case SENSOR_EVENT_LUMINOSITY:
            printf("[CONTROL] Luminosity: %d\n", event->value.luminosity_raw);
            break;
    }
}

// Run the main control logic:
static void control_task(void *arg) {
    QueueHandle_t sensor_queue = (QueueHandle_t)arg;
    sensor_event_t event;

    while(1) {
        if(xQueueReceive(sensor_queue, &event, portMAX_DELAY) == pdTRUE) {
            process_sensor_event(&event);
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
