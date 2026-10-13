#include "manual_control.h"

#include "core/control_task.h"
#include "esp_log.h"

static const char *TAG = "manual_control";

// Forward a dashboard mode selection to the control task:
esp_err_t manual_control_set_mode(bool enabled, uint32_t *command_id_out) {
    control_command_t command = {
        .type = CONTROL_COMMAND_SET_MANUAL_MODE,
        .enabled = enabled
    };

    esp_err_t result = control_task_send_command(&command, command_id_out);
    if(result == ESP_OK) {
        ESP_LOGI(TAG, "Mode request queued | id=%lu mode=%s", (unsigned long)*command_id_out, enabled ? "MANUAL" : "AUTOMATIC");
    }
    else {
        ESP_LOGW(TAG, "Mode request not queued | mode=%s error=%s", enabled ? "MANUAL" : "AUTOMATIC", esp_err_to_name(result));
    }

    return result;
}

// Translate dashboard pump identifiers without allowing direct relay access:
esp_err_t manual_control_set_pump(int pump_id, bool enabled, uint32_t *command_id_out) {
    if(pump_id != 1 && pump_id != 2) {
        ESP_LOGW(TAG, "Pump request rejected | invalid pump_id=%d", pump_id);
        return ESP_ERR_INVALID_ARG;
    }

    control_command_t command = {
        .type = CONTROL_COMMAND_SET_PUMP_STATE,
        .pump = pump_id == 1 ? MANUAL_PUMP_FILL : MANUAL_PUMP_DRAIN, // Dashboard IDs are independent of driver enum values.
        .enabled = enabled
    };

    esp_err_t result = control_task_send_command(&command, command_id_out);
    if(result == ESP_OK) {
        ESP_LOGI(
            TAG,
            "Pump request queued | id=%lu pump=%s enabled=%s",
            (unsigned long)*command_id_out,
            pump_id == 1 ? "FILL" : "DRAIN",
            enabled ? "true" : "false"
        );
    }
    else {
        ESP_LOGW(
            TAG,
            "Pump request not queued | pump=%s enabled=%s error=%s",
            pump_id == 1 ? "FILL" : "DRAIN",
            enabled ? "true" : "false",
            esp_err_to_name(result)
        );
    }

    return result;
}
