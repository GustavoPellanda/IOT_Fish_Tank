#include "sensor_tasks.h"

#include <stdio.h>

#include "core/sensor_events.h"
#include "freertos/task.h"
#include "sensors/level/level_sensor.h"
#include "sensors/luminosity/luminosity_sensor.h"
#include "sensors/temperature/temperature_sensor.h"

#define SENSOR_TASK_STACK_SIZE          3072
#define SENSOR_TASK_PRIORITY            2
#define SENSOR_TASK_CORE                1
#define TEMPERATURE_PERIOD_MS           2000
#define LEVEL_PERIOD_MS                 1000
#define LUMINOSITY_PERIOD_MS            2000

// Publish a sensor event to the control task:
static void publish_sensor_event(QueueHandle_t sensor_queue, const sensor_event_t *event) {
    if(xQueueSend(sensor_queue, event, 0) != pdTRUE) {
        printf("[SENSOR] Control queue full, event dropped: %d\n", event->type);
    }
}

// Read the temperature sensor periodically:
static void temperature_task(void *arg) {
    QueueHandle_t sensor_queue = (QueueHandle_t)arg;
    sensor_event_t event = {
        .type = SENSOR_EVENT_TEMPERATURE
    };

    while(1) {
        event.valid = temperature_sensor_read_celsius(&event.value.temperature_c) == ESP_OK;
        publish_sensor_event(sensor_queue, &event);
        vTaskDelay(pdMS_TO_TICKS(TEMPERATURE_PERIOD_MS));
    }
}

// Read the level sensor periodically:
static void level_task(void *arg) {
    QueueHandle_t sensor_queue = (QueueHandle_t)arg;
    sensor_event_t event = {
        .type = SENSOR_EVENT_LEVEL
    };

    while(1) {
        event.valid = level_sensor_read_distance_cm(&event.value.level_cm) == ESP_OK;
        publish_sensor_event(sensor_queue, &event);
        vTaskDelay(pdMS_TO_TICKS(LEVEL_PERIOD_MS));
    }
}

// Read the luminosity sensor periodically:
static void luminosity_task(void *arg) {
    QueueHandle_t sensor_queue = (QueueHandle_t)arg;
    sensor_event_t event = {
        .type = SENSOR_EVENT_LUMINOSITY
    };

    while(1) {
        event.valid = luminosity_sensor_read_raw(&event.value.luminosity_raw) == ESP_OK;
        publish_sensor_event(sensor_queue, &event);
        vTaskDelay(pdMS_TO_TICKS(LUMINOSITY_PERIOD_MS));
    }
}

// Start the sensor acquisition tasks:
esp_err_t sensor_tasks_start(QueueHandle_t sensor_queue) {
    if(sensor_queue == NULL) return ESP_ERR_INVALID_ARG;

    if(xTaskCreatePinnedToCore(
        temperature_task,
        "temperature_task",
        SENSOR_TASK_STACK_SIZE,
        sensor_queue,
        SENSOR_TASK_PRIORITY,
        NULL,
        SENSOR_TASK_CORE
    ) != pdPASS) return ESP_ERR_NO_MEM;

    if(xTaskCreatePinnedToCore(
        level_task,
        "level_task",
        SENSOR_TASK_STACK_SIZE,
        sensor_queue,
        SENSOR_TASK_PRIORITY,
        NULL,
        SENSOR_TASK_CORE
    ) != pdPASS) return ESP_ERR_NO_MEM;

    if(xTaskCreatePinnedToCore(
        luminosity_task,
        "luminosity_task",
        SENSOR_TASK_STACK_SIZE,
        sensor_queue,
        SENSOR_TASK_PRIORITY,
        NULL,
        SENSOR_TASK_CORE
    ) != pdPASS) return ESP_ERR_NO_MEM;

    return ESP_OK;
}
