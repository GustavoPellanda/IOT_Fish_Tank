#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Reads the temperature sensor value periodically:
static void temperature_task(void *arg) {
    (void)arg;

    while (1) {
        printf("[TEMP] Reading sensor\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Reads the water level sensor value periodically:
static void level_task(void *arg) {
    (void)arg;

    while (1) {
        printf("[LEVEL] Reading sensor\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Reads the luminosity sensor value periodically:
static void luminosity_task(void *arg) {
    (void)arg;

    while (1) {
        printf("[LIGHT] Reading sensor\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void) {
    xTaskCreate(
        temperature_task,
        "temp_task",
        2048,
        NULL,
        2,
        NULL
    );

    xTaskCreate(
        level_task,
        "level_task",
        2048,
        NULL,
        2,
        NULL
    );

    xTaskCreate(
        luminosity_task,
        "lum_task",
        2048,
        NULL,
        2,
        NULL
    );

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}