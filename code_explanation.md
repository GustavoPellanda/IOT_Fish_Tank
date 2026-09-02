# Code Explanation

So far, the project initializes the firmware modules and starts the FreeRTOS tasks. `main.c` configures the sensor drivers, configures the actuator drivers, creates the shared sensor queue through the control module, and then starts the periodic sensor acquisition tasks.

Each sensor has its own FreeRTOS task. Each task periodically reads one sensor, creates one `sensor_event_t`, marks whether the reading is valid, and publishes the event to the shared sensor queue. A `sensor_event_t` represents one reading from one sensor at a time. The `type` field identifies the sensor, and the `value` union stores the corresponding value.

The control task consumes events from the shared queue. At the moment, it only prints valid readings and reports invalid sensor events. Later, this task is expected to become the central control point that interprets sensor data, applies the control rules/state machine, and decides when to activate the pumps and heater.

## Next Steps

- Control rules for pumps and heater.
- State machine implementation.
- Sensor filtering and validation.
- Manual/automatic mode handling.
- MQTT communication.
- Web dashboard and command events.
- OTA update support.
- Logging, watchdog, health monitoring, and NVS persistence.

## Sensors

| Sensor | Driver | Current behavior |
| --- | --- | --- |
| Temperature sensor | `src/sensors/temperature` | Reads a DS18B20-style 1-Wire temperature sensor and reports Celsius. |
| Level sensor | `src/sensors/level` | Sends an ultrasonic trigger pulse, measures echo time, and reports distance in centimeters. |
| Luminosity sensor | `src/sensors/luminosity` | Uses ESP-IDF ADC oneshot mode and reports the raw ADC value. |

## Actuators

| Actuator | Driver | Current behavior |
| --- | --- | --- |
| Fill pump | `src/actuators/pumps` | Configured as a relay output and turned off during initialization. |
| Drain pump | `src/actuators/pumps` | Configured as a relay output and turned off during initialization. |
| Heater | `src/actuators/heater` | Configured as a relay output and turned off during initialization. |

The actuator drivers are initialized, but the control task does not activate them yet.

## ESP32 Pins

| Function | ESP32 pin/configuration |
| --- | --- |
| Temperature data | `GPIO_NUM_32` |
| Level trigger | `GPIO_NUM_5` |
| Level echo | `GPIO_NUM_18` |
| Luminosity ADC unit | `ADC_UNIT_1` |
| Luminosity ADC channel | `ADC_CHANNEL_6` |
| Fill pump relay | `GPIO_NUM_26` |
| Drain pump relay | `GPIO_NUM_27` |
| Heater relay | `GPIO_NUM_25` |
| Relay active level | `true` / active high |
| Level sensor timeout | `30000 us` |

## FreeRTOS Tasks

| Task | Source | Core | Priority | Stack size | Period |
| --- | --- | --- | --- | --- | --- |
| `control_task` | `src/core/control_task.c` | 1 | 3 | 3072 bytes | Waits on queue |
| `temperature_task` | `src/core/sensor_tasks.c` | 1 | 2 | 3072 bytes | 2000 ms |
| `level_task` | `src/core/sensor_tasks.c` | 1 | 2 | 3072 bytes | 1000 ms |
| `luminosity_task` | `src/core/sensor_tasks.c` | 1 | 2 | 3072 bytes | 2000 ms |