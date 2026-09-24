#include "test_state_machine.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Start the suite after PlatformIO has time to attach its serial test runner:
void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    run_state_machine_tests();
}
