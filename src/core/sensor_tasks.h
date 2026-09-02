#ifndef SENSOR_TASKS_H
#define SENSOR_TASKS_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/* Tasks for the sensor readings acquisitions. */

// Queue in which the sensor tasks will publish the validated sensor readings:
esp_err_t sensor_tasks_start(
    QueueHandle_t sensor_queue 
);

#endif
