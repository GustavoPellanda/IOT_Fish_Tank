#include "test_state_machine.h"

#include <stddef.h>

#include "actuators/heater/heater.h"
#include "actuators/pumps/pumps.h"
#include "core/state_machine.h"
#include "esp_timer.h"
#include "fake_actuators.h"
#include "unity.h"

static state_machine_t machine;

// Provide a temperature event without accessing the physical sensor:
static esp_err_t send_temperature(float temperature_c, bool valid) {
    sensor_event_t event = {
        .type = SENSOR_EVENT_TEMPERATURE,
        .valid = valid,
        .value.temperature_c = temperature_c
    };
    return state_machine_process_sensor_event(&machine, &event);
}

// Provide a distance event in centimeters without accessing the echo pin:
static esp_err_t send_level(float distance_cm, bool valid) {
    sensor_event_t event = {
        .type = SENSOR_EVENT_LEVEL,
        .valid = valid,
        .value.level_cm = distance_cm
    };
    return state_machine_process_sensor_event(&machine, &event);
}

// Provide an ADC event independently of the critical sensor inputs:
static esp_err_t send_luminosity(int raw_value, bool valid) {
    sensor_event_t event = {
        .type = SENSOR_EVENT_LUMINOSITY,
        .valid = valid,
        .value.luminosity_raw = raw_value
    };
    return state_machine_process_sensor_event(&machine, &event);
}

// Check the operating state together with every simulated actuator output:
static void assert_state_and_outputs(system_state_t state, bool fill, bool drain, bool heater) {
    const fake_actuators_state_t *outputs = fake_actuators_get_state();
    TEST_ASSERT_EQUAL_INT(state, machine.state);
    TEST_ASSERT_EQUAL_INT(fill, outputs->fill_enabled);
    TEST_ASSERT_EQUAL_INT(drain, outputs->drain_enabled);
    TEST_ASSERT_EQUAL_INT(heater, outputs->heater_enabled);
}

// Reset each scenario through the production initialization function:
static void reset_machine(void) {
    fake_actuators_reset();
    TEST_ASSERT_EQUAL(ESP_OK, state_machine_init(&machine));
}

// Prepare an independent machine and actuator history for every test:
void setUp(void) {
    reset_machine();
}

// Detect conflicting outputs at any point in a sequence, including between commands:
void tearDown(void) {
    const fake_actuators_state_t *outputs = fake_actuators_get_state();
    TEST_ASSERT_FALSE_MESSAGE(outputs->command_log_overflow, "Actuator command history overflowed");
    TEST_ASSERT_FALSE_MESSAGE(outputs->conflicting_outputs_seen, "Mutually exclusive actuators overlapped");
}

// Enter heating through valid sensor events instead of modifying the state directly:
static void start_heating(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_level(14.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);
}

// Verify initialization actively clears previously enabled outputs and sensor context:
static void test_initialization_disables_actuators(void) {
    TEST_ASSERT_EQUAL(ESP_OK, pumps_set_state(PUMP_FILL, true));
    TEST_ASSERT_EQUAL(ESP_OK, state_machine_init(&machine));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);

    TEST_ASSERT_EQUAL(ESP_OK, heater_set_enabled(true));
    TEST_ASSERT_EQUAL(ESP_OK, state_machine_init(&machine));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_FALSE(machine.temperature_received);
    TEST_ASSERT_FALSE(machine.level_received);
    TEST_ASSERT_FALSE(machine.luminosity_received);
    TEST_ASSERT_FALSE(machine.temperature_valid);
    TEST_ASSERT_FALSE(machine.level_valid);
    TEST_ASSERT_FALSE(machine.luminosity_valid);
    TEST_ASSERT_FALSE(machine.actuator_fault);
    TEST_ASSERT_TRUE(machine.operation_started_us == 0);
    TEST_ASSERT_FALSE(machine.operation_timed_out);
}

// Keep outputs off when a cold temperature arrives before the first level reading:
static void test_temperature_alone_cannot_start_heating(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(20.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_luminosity(1000, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);

    TEST_ASSERT_EQUAL(ESP_OK, send_level(14.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);
}

// Keep outputs off when an empty reading arrives before the first temperature:
static void test_level_alone_cannot_start_filling(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_level(30.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);

    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);
}

// Keep the exact idle thresholds inclusive when no actuator is needed:
static void test_idle_threshold_boundaries(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(24.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(8.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);

    TEST_ASSERT_EQUAL(ESP_OK, send_level(25.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
}

// Give filling priority over heating when the first complete inputs show low water:
static void test_low_water_prevents_heating_from_idle(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(25.1f, true));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);
}

// Keep filling through the hysteresis band and stop exactly at the target distance:
static void test_filling_stops_at_target(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(25.1f, true));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);

    TEST_ASSERT_EQUAL(ESP_OK, send_level(25.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_level(14.1f, true));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_level(14.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
}

// Give draining priority over heating when the tank is above the full threshold:
static void test_high_water_starts_draining_before_heating(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(7.9f, true));
    assert_state_and_outputs(SYSTEM_STATE_DRAINING, false, true, false);
}

// Keep draining through the hysteresis band and stop exactly at the target distance:
static void test_draining_stops_at_target(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(7.9f, true));
    assert_state_and_outputs(SYSTEM_STATE_DRAINING, false, true, false);

    TEST_ASSERT_EQUAL(ESP_OK, send_level(8.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_DRAINING, false, true, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_level(13.9f, true));
    assert_state_and_outputs(SYSTEM_STATE_DRAINING, false, true, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_level(14.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
}

// Check the heating start threshold and the separate stop threshold:
static void test_heating_temperature_hysteresis(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_level(14.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(24.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.9f, true));
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);

    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(25.9f, true));
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(26.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(24.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
}

// Stop heating on the same event that moves the distance above 25 cm:
static void test_low_water_interrupts_heating(void) {
    start_heating();
    TEST_ASSERT_EQUAL(ESP_OK, send_level(24.9f, true));
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);
    TEST_ASSERT_EQUAL(ESP_OK, send_level(25.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);

    TEST_ASSERT_EQUAL(ESP_OK, send_level(25.1f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
}

// Refill after a low-water shutdown and resume heating only after reaching the target:
static void test_heating_resumes_after_refilling(void) {
    start_heating();
    TEST_ASSERT_EQUAL(ESP_OK, send_level(30.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);

    TEST_ASSERT_EQUAL(ESP_OK, send_level(24.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_level(14.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);
}

// Preserve ongoing heating when the valid level changes without becoming too low:
static void test_high_water_does_not_interrupt_heating(void) {
    start_heating();
    TEST_ASSERT_EQUAL(ESP_OK, send_level(7.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);
}

// Reject a missing critical sensor immediately, even before both sensors report:
static void test_invalid_initial_sensor_enters_error(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(0.0f, false));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
}

// Stop every automatic operation when either critical sensor becomes invalid:
static void test_critical_sensor_fault_stops_each_automatic_state(void) {
    const struct {
        float temperature_c;
        float level_cm;
        system_state_t state;
    } scenarios[] = {
        {27.0f, 14.0f, SYSTEM_STATE_IDLE},
        {27.0f, 30.0f, SYSTEM_STATE_FILLING},
        {27.0f, 7.0f, SYSTEM_STATE_DRAINING},
        {23.0f, 14.0f, SYSTEM_STATE_HEATING}
    };

    for(size_t index = 0; index < sizeof(scenarios) / sizeof(scenarios[0]); index++) {
        for(int sensor = 0; sensor < 2; sensor++) {
            reset_machine();
            TEST_ASSERT_EQUAL(ESP_OK, send_level(scenarios[index].level_cm, true));
            TEST_ASSERT_EQUAL(ESP_OK, send_temperature(scenarios[index].temperature_c, true));
            TEST_ASSERT_EQUAL_INT(scenarios[index].state, machine.state);

            if(sensor == 0) TEST_ASSERT_EQUAL(ESP_OK, send_temperature(0.0f, false));
            else TEST_ASSERT_EQUAL(ESP_OK, send_level(0.0f, false));
            assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
            TEST_ASSERT_FALSE(fake_actuators_get_state()->conflicting_outputs_seen);
        }
    }
}

// Keep the last accepted values when failed readings contain unusable payloads:
static void test_invalid_readings_preserve_last_valid_values(void) {
    start_heating();
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(-1000.0f, false));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(-1000.0f, false));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 23.0f, machine.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.0f, machine.level_cm);
    TEST_ASSERT_FALSE(machine.temperature_valid);
    TEST_ASSERT_FALSE(machine.level_valid);
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
}

// Treat luminosity failure as noncritical while preserving its last accepted reading:
static void test_luminosity_fault_does_not_stop_heating(void) {
    start_heating();
    TEST_ASSERT_EQUAL(ESP_OK, send_luminosity(1200, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_luminosity(-1, false));
    TEST_ASSERT_TRUE(machine.luminosity_received);
    TEST_ASSERT_FALSE(machine.luminosity_valid);
    TEST_ASSERT_EQUAL_INT(1200, machine.luminosity_raw);
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);
}

// Wait for every reported critical fault to clear before leaving the error state:
static void test_recovery_waits_for_both_critical_sensors(void) {
    start_heating();
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(0.0f, false));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(0.0f, false));
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_luminosity(1200, true));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);

    TEST_ASSERT_EQUAL(ESP_OK, send_level(14.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);
}

// Prevent recovery from bypassing the requirement for the first level reading:
static void test_recovery_still_waits_for_missing_sensor(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(0.0f, false));
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_luminosity(1200, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_FALSE(machine.level_received);
}

// Reject null arguments without issuing actuator commands:
static void test_null_arguments_return_invalid_arg(void) {
    sensor_event_t event = {.type = SENSOR_EVENT_LEVEL, .valid = false};
    size_t commands_before = fake_actuators_get_state()->command_count;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, state_machine_init(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, state_machine_process_sensor_event(NULL, &event));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, state_machine_process_sensor_event(&machine, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, state_machine_retry_shutdown(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, state_machine_check_operation_timeout(NULL, 0));
    TEST_ASSERT_EQUAL_UINT(commands_before, fake_actuators_get_state()->command_count);
}

// Report an actuator failure during initialization to the caller:
static void test_initialization_propagates_driver_failure(void) {
    fake_actuators_fail_next(FAKE_ACTUATOR_HEATER, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, state_machine_init(&machine));
    TEST_ASSERT_EQUAL_INT(SYSTEM_STATE_ERROR, machine.state);
    TEST_ASSERT_TRUE(machine.actuator_fault);
    TEST_ASSERT_EQUAL(ESP_OK, state_machine_retry_shutdown(&machine));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_FALSE(machine.actuator_fault);
}

// Propagate a failed filling command without claiming the pump was enabled:
static void test_pump_failure_is_returned_to_caller(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    fake_actuators_fail_next(FAKE_ACTUATOR_FILL_PUMP, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_level(30.0f, true));
    TEST_ASSERT_FALSE(fake_actuators_get_state()->fill_enabled);
    TEST_ASSERT_FALSE(fake_actuators_get_state()->heater_enabled);
    TEST_ASSERT_EQUAL_INT(SYSTEM_STATE_ERROR, machine.state);
    TEST_ASSERT_TRUE(machine.actuator_fault);
}

// Propagate a failed heater command without changing its simulated output:
static void test_heater_failure_is_returned_to_caller(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_level(14.0f, true));
    fake_actuators_fail_next(FAKE_ACTUATOR_HEATER, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_temperature(23.0f, true));
    TEST_ASSERT_FALSE(fake_actuators_get_state()->heater_enabled);
    TEST_ASSERT_EQUAL_INT(SYSTEM_STATE_ERROR, machine.state);
    TEST_ASSERT_TRUE(machine.actuator_fault);
}

// Disable heating at low water even if a subsequent pump command fails:
static void test_low_water_disables_heater_before_pump_failure(void) {
    start_heating();
    size_t commands_before = fake_actuators_get_state()->command_count;
    fake_actuators_fail_next(FAKE_ACTUATOR_FILL_PUMP, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_level(30.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
    TEST_ASSERT_TRUE(machine.actuator_fault);

    const fake_actuator_command_t *first_command = &fake_actuators_get_state()->commands[commands_before];
    TEST_ASSERT_EQUAL_INT(FAKE_ACTUATOR_HEATER, first_command->actuator);
    TEST_ASSERT_FALSE(first_command->enabled);
}

// Continue pump shutdown commands when the heater shutdown itself fails:
static void test_failed_heater_shutdown_still_attempts_pump_shutdown(void) {
    start_heating();
    size_t commands_before = fake_actuators_get_state()->command_count;
    fake_actuators_fail_next(FAKE_ACTUATOR_HEATER, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_level(30.0f, true));
    TEST_ASSERT_EQUAL_UINT(commands_before + 3, fake_actuators_get_state()->command_count);
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, true);
    TEST_ASSERT_TRUE(machine.actuator_fault);

    // A later event completes shutdown before another event can start filling:
    TEST_ASSERT_EQUAL(ESP_OK, send_level(30.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_FALSE(machine.actuator_fault);
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);
}

// Stop an active fill pump despite a failed heater-off command on entry to error:
static void test_heater_shutdown_failure_does_not_leave_fill_pump_running(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(30.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);

    size_t commands_before = fake_actuators_get_state()->command_count;
    fake_actuators_fail_next(FAKE_ACTUATOR_HEATER, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_temperature(0.0f, false));
    TEST_ASSERT_EQUAL_UINT(commands_before + 3, fake_actuators_get_state()->command_count);
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
}

// Stop an active drain pump despite a failed fill-off command on entry to error:
static void test_fill_shutdown_failure_does_not_leave_drain_pump_running(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(7.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_DRAINING, false, true, false);

    size_t commands_before = fake_actuators_get_state()->command_count;
    fake_actuators_fail_next(FAKE_ACTUATOR_FILL_PUMP, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_temperature(0.0f, false));
    TEST_ASSERT_EQUAL_UINT(commands_before + 3, fake_actuators_get_state()->command_count);
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
}

// Preserve the first driver error while attempting all shutdown commands in order:
static void test_complete_shutdown_returns_first_error(void) {
    size_t commands_before = fake_actuators_get_state()->command_count;
    fake_actuators_fail_next(FAKE_ACTUATOR_HEATER, ESP_ERR_TIMEOUT);
    fake_actuators_fail_next(FAKE_ACTUATOR_FILL_PUMP, ESP_ERR_INVALID_STATE);
    fake_actuators_fail_next(FAKE_ACTUATOR_DRAIN_PUMP, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, state_machine_init(&machine));

    const fake_actuators_state_t *outputs = fake_actuators_get_state();
    TEST_ASSERT_EQUAL_UINT(commands_before + 3, outputs->command_count);
    TEST_ASSERT_EQUAL_INT(FAKE_ACTUATOR_HEATER, outputs->commands[commands_before].actuator);
    TEST_ASSERT_EQUAL_INT(FAKE_ACTUATOR_FILL_PUMP, outputs->commands[commands_before + 1].actuator);
    TEST_ASSERT_EQUAL_INT(FAKE_ACTUATOR_DRAIN_PUMP, outputs->commands[commands_before + 2].actuator);
    TEST_ASSERT_FALSE(outputs->commands[commands_before].enabled);
    TEST_ASSERT_FALSE(outputs->commands[commands_before + 1].enabled);
    TEST_ASSERT_FALSE(outputs->commands[commands_before + 2].enabled);
}

// Keep heating disabled if a pump cannot acknowledge its off command:
static void test_pump_shutdown_failure_prevents_heater_enable(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_level(14.0f, true));
    fake_actuators_fail_next(FAKE_ACTUATOR_DRAIN_PUMP, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_temperature(23.0f, true));
    TEST_ASSERT_FALSE(fake_actuators_get_state()->heater_enabled);
}

// Avoid overlapping pumps when filling follows a failed drain shutdown:
static void test_filling_after_failed_drain_shutdown_does_not_overlap_pumps(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(7.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_DRAINING, false, true, false);

    fake_actuators_fail_next(FAKE_ACTUATOR_DRAIN_PUMP, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_level(30.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, true, false);
    TEST_ASSERT_TRUE(machine.actuator_fault);

    TEST_ASSERT_EQUAL(ESP_OK, state_machine_retry_shutdown(&machine));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);
    TEST_ASSERT_FALSE(fake_actuators_get_state()->conflicting_outputs_seen);
}

// Block every enable command when the incompatible drain pump cannot be disabled:
static void test_failed_drain_shutdown_prevents_fill_enable(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(7.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_DRAINING, false, true, false);

    fake_actuators_fail_next(FAKE_ACTUATOR_DRAIN_PUMP, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_level(30.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, true, false);
    TEST_ASSERT_TRUE(machine.actuator_fault);

    size_t commands_before = fake_actuators_get_state()->command_count;
    fake_actuators_fail_next(FAKE_ACTUATOR_DRAIN_PUMP, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_temperature(27.0f, true));

    const fake_actuators_state_t *outputs = fake_actuators_get_state();
    TEST_ASSERT_FALSE(outputs->fill_enabled);
    TEST_ASSERT_TRUE(outputs->drain_enabled);
    TEST_ASSERT_FALSE(outputs->heater_enabled);
    for(size_t index = commands_before; index < outputs->command_count; index++) {
        TEST_ASSERT_FALSE(outputs->commands[index].enabled);
    }
    TEST_ASSERT_FALSE(outputs->conflicting_outputs_seen);
    TEST_ASSERT_EQUAL_INT(SYSTEM_STATE_ERROR, machine.state);
    TEST_ASSERT_TRUE(machine.actuator_fault);
}

// Retry repeated heater shutdown failures without enabling another actuator:
static void test_repeated_shutdown_failures_block_automatic_control(void) {
    start_heating();
    fake_actuators_fail_next(FAKE_ACTUATOR_HEATER, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_temperature(26.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, true);

    for(int attempt = 0; attempt < 3; attempt++) {
        size_t commands_before = fake_actuators_get_state()->command_count;
        fake_actuators_fail_next(FAKE_ACTUATOR_HEATER, ESP_FAIL);
        TEST_ASSERT_EQUAL(ESP_FAIL, send_level(30.0f, true));
        assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, true);
        TEST_ASSERT_TRUE(machine.actuator_fault);

        const fake_actuators_state_t *outputs = fake_actuators_get_state();
        for(size_t index = commands_before; index < outputs->command_count; index++) {
            TEST_ASSERT_FALSE(outputs->commands[index].enabled);
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, state_machine_retry_shutdown(&machine));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_FALSE(machine.actuator_fault);
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);
}

// Keep a failed periodic shutdown pending until a later retry succeeds:
static void test_shutdown_retry_failure_without_events_remains_pending(void) {
    start_heating();
    fake_actuators_fail_next(FAKE_ACTUATOR_HEATER, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_temperature(26.0f, true));

    fake_actuators_fail_next(FAKE_ACTUATOR_HEATER, ESP_ERR_TIMEOUT);
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, state_machine_retry_shutdown(&machine));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, true);
    TEST_ASSERT_TRUE(machine.actuator_fault);

    TEST_ASSERT_EQUAL(ESP_OK, state_machine_retry_shutdown(&machine));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_FALSE(machine.actuator_fault);
}

// Complete shutdown without leaving error while a critical sensor remains invalid:
static void test_shutdown_recovery_still_requires_valid_critical_sensors(void) {
    start_heating();
    fake_actuators_fail_next(FAKE_ACTUATOR_HEATER, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, send_temperature(0.0f, false));
    TEST_ASSERT_TRUE(machine.actuator_fault);

    TEST_ASSERT_EQUAL(ESP_OK, state_machine_retry_shutdown(&machine));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
    TEST_ASSERT_FALSE(machine.actuator_fault);
    TEST_ASSERT_EQUAL(ESP_OK, send_level(14.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
}

// Preserve the triggering error even when emergency shutdown also fails:
static void test_active_state_error_is_preserved_when_emergency_shutdown_fails(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    fake_actuators_fail_next(FAKE_ACTUATOR_HEATER, ESP_ERR_INVALID_STATE);
    fake_actuators_fail_next(FAKE_ACTUATOR_FILL_PUMP, ESP_ERR_TIMEOUT);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, send_level(30.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
    TEST_ASSERT_TRUE(machine.actuator_fault);

    TEST_ASSERT_EQUAL(ESP_OK, state_machine_retry_shutdown(&machine));
    TEST_ASSERT_FALSE(machine.actuator_fault);
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
}

// Leave the actuator drivers untouched when there is no pending shutdown:
static void test_shutdown_retry_without_fault_does_not_issue_commands(void) {
    start_heating();
    size_t commands_before = fake_actuators_get_state()->command_count;
    TEST_ASSERT_EQUAL(ESP_OK, state_machine_retry_shutdown(&machine));
    TEST_ASSERT_EQUAL_UINT(commands_before, fake_actuators_get_state()->command_count);
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);
}

// Stop filling exactly five minutes after its initial activation:
static void test_filling_timeout_boundary(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(30.0f, true));
    int64_t started_us = machine.operation_started_us;
    size_t commands_before = fake_actuators_get_state()->command_count;

    TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, started_us + 300000000LL - 1));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);
    TEST_ASSERT_EQUAL_UINT(commands_before, fake_actuators_get_state()->command_count);
    TEST_ASSERT_FALSE(machine.operation_timed_out);

    TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, started_us + 300000000LL));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
    TEST_ASSERT_TRUE(machine.operation_timed_out);
    TEST_ASSERT_FALSE(machine.actuator_fault);
}

// Stop draining exactly five minutes after its initial activation:
static void test_draining_timeout_boundary(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(7.0f, true));
    int64_t started_us = machine.operation_started_us;
    size_t commands_before = fake_actuators_get_state()->command_count;

    TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, started_us + 300000000LL - 1));
    assert_state_and_outputs(SYSTEM_STATE_DRAINING, false, true, false);
    TEST_ASSERT_EQUAL_UINT(commands_before, fake_actuators_get_state()->command_count);
    TEST_ASSERT_FALSE(machine.operation_timed_out);

    TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, started_us + 300000000LL));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
    TEST_ASSERT_TRUE(machine.operation_timed_out);
    TEST_ASSERT_FALSE(machine.actuator_fault);
}

// Preserve the full sixty-minute heating limit beyond signed 32-bit microseconds:
static void test_heating_timeout_boundary(void) {
    start_heating();
    int64_t started_us = machine.operation_started_us;
    size_t commands_before = fake_actuators_get_state()->command_count;

    TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, started_us + 3600000000LL - 1));
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);
    TEST_ASSERT_EQUAL_UINT(commands_before, fake_actuators_get_state()->command_count);
    TEST_ASSERT_FALSE(machine.operation_timed_out);

    TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, started_us + 3600000000LL));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
    TEST_ASSERT_TRUE(machine.operation_timed_out);
    TEST_ASSERT_FALSE(machine.actuator_fault);
}

// Keep each operation's original deadline despite repeated sensor events:
static void test_sensor_events_do_not_extend_operation_timeout(void) {
    const float levels[] = {30.0f, 7.0f, 14.0f};
    const float temperatures[] = {27.0f, 27.0f, 23.0f};
    const int64_t limits_us[] = {300000000LL, 300000000LL, 3600000000LL};

    for(size_t index = 0; index < 3; index++) {
        reset_machine();
        TEST_ASSERT_EQUAL(ESP_OK, send_temperature(temperatures[index], true));
        TEST_ASSERT_EQUAL(ESP_OK, send_level(levels[index], true));
        int64_t started_us = machine.operation_started_us;

        TEST_ASSERT_EQUAL(ESP_OK, send_temperature(temperatures[index], true));
        TEST_ASSERT_EQUAL(ESP_OK, send_level(levels[index], true));
        TEST_ASSERT_EQUAL(ESP_OK, send_luminosity(1000, true));
        TEST_ASSERT_TRUE(machine.operation_started_us == started_us);
        TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, started_us + limits_us[index]));
        assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
        TEST_ASSERT_TRUE(machine.operation_timed_out);
    }
}

// Give a new filling operation its own deadline after a successful previous fill:
static void test_new_operation_gets_a_fresh_start_time(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(30.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(14.0f, true));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);

    int64_t before_new_operation_us = esp_timer_get_time();
    TEST_ASSERT_EQUAL(ESP_OK, send_level(30.0f, true));
    TEST_ASSERT_TRUE(machine.operation_started_us >= before_new_operation_us);
    int64_t started_us = machine.operation_started_us;
    TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, started_us + 300000000LL - 1));
    assert_state_and_outputs(SYSTEM_STATE_FILLING, true, false, false);
    TEST_ASSERT_FALSE(machine.operation_timed_out);
    TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, started_us + 300000000LL));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
}

// Apply operation limits only to filling, draining, and heating:
static void test_inactive_states_have_no_operation_timeout(void) {
    const system_state_t states[] = {SYSTEM_STATE_IDLE, SYSTEM_STATE_MANUAL, SYSTEM_STATE_ERROR};
    size_t commands_before = fake_actuators_get_state()->command_count;

    for(size_t index = 0; index < 3; index++) {
        machine.state = states[index];
        TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, 7200000000LL));
        assert_state_and_outputs(states[index], false, false, false);
        TEST_ASSERT_FALSE(machine.operation_timed_out);
    }
    TEST_ASSERT_EQUAL_UINT(commands_before, fake_actuators_get_state()->command_count);
}

// Keep timeout latched through target readings, sensor recovery, and shutdown retries:
static void test_operation_timeout_blocks_automatic_recovery(void) {
    start_heating();
    TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, machine.operation_started_us + 3600000000LL));
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(26.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(30.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(0.0f, false));
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(23.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_luminosity(1000, true));
    TEST_ASSERT_EQUAL(ESP_OK, state_machine_retry_shutdown(&machine));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
    TEST_ASSERT_TRUE(machine.operation_timed_out);

    size_t commands_before = fake_actuators_get_state()->command_count;
    TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, machine.operation_started_us + 7200000000LL));
    TEST_ASSERT_EQUAL_UINT(commands_before, fake_actuators_get_state()->command_count);
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
}

// Retry failed timeout shutdowns while preserving the restart requirement for every actuator:
static void test_timeout_shutdown_failures_remain_blocked_after_recovery(void) {
    const float levels[] = {30.0f, 7.0f, 14.0f};
    const float temperatures[] = {27.0f, 27.0f, 23.0f};
    const int64_t limits_us[] = {300000000LL, 300000000LL, 3600000000LL};
    const fake_actuator_id_t actuators[] = {FAKE_ACTUATOR_FILL_PUMP, FAKE_ACTUATOR_DRAIN_PUMP, FAKE_ACTUATOR_HEATER};

    for(size_t index = 0; index < 3; index++) {
        reset_machine();
        TEST_ASSERT_EQUAL(ESP_OK, send_temperature(temperatures[index], true));
        TEST_ASSERT_EQUAL(ESP_OK, send_level(levels[index], true));
        size_t commands_before = fake_actuators_get_state()->command_count;

        fake_actuators_fail_next(actuators[index], ESP_FAIL);
        TEST_ASSERT_EQUAL(ESP_FAIL, state_machine_check_operation_timeout(&machine, machine.operation_started_us + limits_us[index]));
        TEST_ASSERT_EQUAL_UINT(commands_before + 3, fake_actuators_get_state()->command_count);
        assert_state_and_outputs(SYSTEM_STATE_ERROR, index == 0, index == 1, index == 2);
        TEST_ASSERT_TRUE(machine.operation_timed_out);
        TEST_ASSERT_TRUE(machine.actuator_fault);

        fake_actuators_fail_next(actuators[index], ESP_ERR_TIMEOUT);
        TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, state_machine_retry_shutdown(&machine));
        TEST_ASSERT_TRUE(machine.actuator_fault);
        TEST_ASSERT_EQUAL(ESP_OK, state_machine_retry_shutdown(&machine));
        assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
        TEST_ASSERT_FALSE(machine.actuator_fault);
        TEST_ASSERT_TRUE(machine.operation_timed_out);
        TEST_ASSERT_EQUAL(ESP_OK, send_level(levels[index], true));
        assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
    }
}

// Clear a latched timeout only by initializing the machine as on firmware startup:
static void test_initialization_clears_operation_timeout(void) {
    start_heating();
    TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, machine.operation_started_us + 3600000000LL));
    TEST_ASSERT_TRUE(machine.operation_timed_out);

    TEST_ASSERT_EQUAL(ESP_OK, state_machine_init(&machine));
    assert_state_and_outputs(SYSTEM_STATE_IDLE, false, false, false);
    TEST_ASSERT_TRUE(machine.operation_started_us == 0);
    TEST_ASSERT_FALSE(machine.operation_timed_out);
    start_heating();
    TEST_ASSERT_FALSE(machine.operation_timed_out);
}

// Reject negative or backwards monotonic times without changing an active operation:
static void test_invalid_operation_check_times_do_not_change_outputs(void) {
    start_heating();
    size_t commands_before = fake_actuators_get_state()->command_count;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, state_machine_check_operation_timeout(&machine, -1));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, state_machine_check_operation_timeout(&machine, machine.operation_started_us - 1));
    TEST_ASSERT_EQUAL_UINT(commands_before, fake_actuators_get_state()->command_count);
    assert_state_and_outputs(SYSTEM_STATE_HEATING, false, false, true);
    TEST_ASSERT_FALSE(machine.operation_timed_out);
}

// Detect an expired operation even when its first check occurs after the deadline:
static void test_operation_timeout_after_deadline(void) {
    TEST_ASSERT_EQUAL(ESP_OK, send_temperature(27.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, send_level(30.0f, true));
    TEST_ASSERT_EQUAL(ESP_OK, state_machine_check_operation_timeout(&machine, machine.operation_started_us + 300100000LL));
    assert_state_and_outputs(SYSTEM_STATE_ERROR, false, false, false);
    TEST_ASSERT_TRUE(machine.operation_timed_out);
}

// Execute the suite with test locations reported from this source file:
void run_state_machine_tests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initialization_disables_actuators);
    RUN_TEST(test_temperature_alone_cannot_start_heating);
    RUN_TEST(test_level_alone_cannot_start_filling);
    RUN_TEST(test_idle_threshold_boundaries);
    RUN_TEST(test_low_water_prevents_heating_from_idle);
    RUN_TEST(test_filling_stops_at_target);
    RUN_TEST(test_high_water_starts_draining_before_heating);
    RUN_TEST(test_draining_stops_at_target);
    RUN_TEST(test_heating_temperature_hysteresis);
    RUN_TEST(test_low_water_interrupts_heating);
    RUN_TEST(test_heating_resumes_after_refilling);
    RUN_TEST(test_high_water_does_not_interrupt_heating);
    RUN_TEST(test_invalid_initial_sensor_enters_error);
    RUN_TEST(test_critical_sensor_fault_stops_each_automatic_state);
    RUN_TEST(test_invalid_readings_preserve_last_valid_values);
    RUN_TEST(test_luminosity_fault_does_not_stop_heating);
    RUN_TEST(test_recovery_waits_for_both_critical_sensors);
    RUN_TEST(test_recovery_still_waits_for_missing_sensor);
    RUN_TEST(test_null_arguments_return_invalid_arg);
    RUN_TEST(test_initialization_propagates_driver_failure);
    RUN_TEST(test_pump_failure_is_returned_to_caller);
    RUN_TEST(test_heater_failure_is_returned_to_caller);
    RUN_TEST(test_low_water_disables_heater_before_pump_failure);
    RUN_TEST(test_failed_heater_shutdown_still_attempts_pump_shutdown);
    RUN_TEST(test_heater_shutdown_failure_does_not_leave_fill_pump_running);
    RUN_TEST(test_fill_shutdown_failure_does_not_leave_drain_pump_running);
    RUN_TEST(test_complete_shutdown_returns_first_error);
    RUN_TEST(test_pump_shutdown_failure_prevents_heater_enable);
    RUN_TEST(test_filling_after_failed_drain_shutdown_does_not_overlap_pumps);
    RUN_TEST(test_failed_drain_shutdown_prevents_fill_enable);
    RUN_TEST(test_repeated_shutdown_failures_block_automatic_control);
    RUN_TEST(test_shutdown_retry_failure_without_events_remains_pending);
    RUN_TEST(test_shutdown_recovery_still_requires_valid_critical_sensors);
    RUN_TEST(test_active_state_error_is_preserved_when_emergency_shutdown_fails);
    RUN_TEST(test_shutdown_retry_without_fault_does_not_issue_commands);
    RUN_TEST(test_filling_timeout_boundary);
    RUN_TEST(test_draining_timeout_boundary);
    RUN_TEST(test_heating_timeout_boundary);
    RUN_TEST(test_sensor_events_do_not_extend_operation_timeout);
    RUN_TEST(test_new_operation_gets_a_fresh_start_time);
    RUN_TEST(test_inactive_states_have_no_operation_timeout);
    RUN_TEST(test_operation_timeout_blocks_automatic_recovery);
    RUN_TEST(test_timeout_shutdown_failures_remain_blocked_after_recovery);
    RUN_TEST(test_initialization_clears_operation_timeout);
    RUN_TEST(test_invalid_operation_check_times_do_not_change_outputs);
    RUN_TEST(test_operation_timeout_after_deadline);
    UNITY_END();
}
