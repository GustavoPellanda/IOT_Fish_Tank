#ifndef MANUAL_CONTROL_H
#define MANUAL_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/*
Translate dashboard requests into queued control commands. ESP_OK means the
request was accepted by the queue; execution can still be rejected by the
state machine. The returned identifier matches the result in control status.
*/
esp_err_t manual_control_set_mode(
    bool enabled,            // True requests MANUAL, false requests automatic control
    uint32_t *command_id_out // Identifier returned after queue acceptance
);

esp_err_t manual_control_set_pump(
    int pump_id,             // Dashboard pump identifier: fill=1, drain=2
    bool enabled,            // Requested pump state
    uint32_t *command_id_out // Identifier returned after queue acceptance
);

#endif

/*
 * How manual control requests work:
 * The web server passes validated mode and pump requests to this module.
 * Dashboard pump IDs are translated into the state machine's pump identifiers,
 * then copied into the control task's queue. Only the state machine changes
 * operating states and relay outputs after checking its safety conditions.
 */
