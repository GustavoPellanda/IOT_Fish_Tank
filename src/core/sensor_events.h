#ifndef SENSOR_EVENTS_H
#define SENSOR_EVENTS_H

#include <stdbool.h>

/* Structure for sensor events */

typedef enum {
    SENSOR_EVENT_TEMPERATURE,
    SENSOR_EVENT_LEVEL,
    SENSOR_EVENT_LUMINOSITY
} sensor_event_type_t;

typedef struct {
    sensor_event_type_t type;
    bool valid;
    union {
        float temperature_c;
        float level_cm;
        int luminosity_raw;
    } value;
} sensor_event_t;

#endif

/*
 * Sensors format their readings into these events.
 * Each event includes a type indicating the originating sensor,
 * a validity flag, and a union containing the measured value.
 * The value field changes depending on the sensor type.
 */