#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/* Task for controlling the actuator systems based on sensor readings. */

esp_err_t control_task_start(
    QueueHandle_t *sensor_queue_out // Queue used by sensor tasks to publish readings
);

#endif
