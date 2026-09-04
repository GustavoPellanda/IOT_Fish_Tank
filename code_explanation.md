# Code Explanation

So far, the project initializes the firmware modules, starts the FreeRTOS tasks, and executes the first version of the control state machine. `main.c` configures the sensor drivers, configures the actuator drivers, creates the shared sensor queue through the control module, and then starts the periodic sensor acquisition tasks.

Each sensor has its own FreeRTOS task. Each task periodically reads one sensor, creates one `sensor_event_t`, marks whether the reading is valid, and publishes the event to the shared sensor queue. A `sensor_event_t` represents one reading from one sensor at a time. The `type` field identifies the sensor, and the `value` union stores the corresponding value.

The control task consumes events from the shared queue and forwards each received sensor event to the state machine. The state machine stores the latest sensor values, tracks whether each sensor has already reported data, validates critical readings, evaluates the current operating state, performs state transitions, and applies actuator outputs through the pump and heater drivers.

The control flow is event-driven. Sensor tasks do not control GPIOs directly; they only publish readings. The control task owns the state machine, and the state machine is the only part of the control layer that decides when the fill pump, drain pump, or heater should be enabled.

## Next Steps

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
| Fill pump | `src/actuators/pumps` | Configured as a relay output, turned off during initialization, and enabled by the state machine while filling. |
| Drain pump | `src/actuators/pumps` | Configured as a relay output, turned off during initialization, and enabled by the state machine while draining. |
| Heater | `src/actuators/heater` | Configured as a relay output, turned off during initialization, and enabled by the state machine while heating. |

The actuator drivers do not decide system behavior. They only apply the relay state requested by the control state machine.

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

## Operating States

| State | Activation condition | Actuator behavior |
| --- | --- | --- |
| `SYSTEM_STATE_IDLE` | Initial state, target level reached, target temperature reached, recovered from error, or no automatic action is required. | Fill pump off, drain pump off, heater off. |
| `SYSTEM_STATE_FILLING` | Activated from idle when the level distance is greater than `LEVEL_EMPTY_DISTANCE_CM`, meaning the water level is too low. | Fill pump on, drain pump off, heater off. |
| `SYSTEM_STATE_DRAINING` | Activated from idle when the level distance is lower than `LEVEL_FULL_DISTANCE_CM`, meaning the water level is too high. | Fill pump off, drain pump on, heater off. |
| `SYSTEM_STATE_HEATING` | Activated from idle when the temperature is lower than `TEMPERATURE_LOW_C`. | Fill pump off, drain pump off, heater on. |
| `SYSTEM_STATE_MANUAL` | Reserved for future manual command handling. It is not activated by the current sensor-only event flow. | Currently keeps automatic actuator outputs off. |
| `SYSTEM_STATE_ERROR` | Activated when a critical sensor reports invalid data after it has already published at least one event. Critical sensors are temperature and level. | Fill pump off, drain pump off, heater off. |
