# Code Explanation

So far, the project initializes the firmware modules, starts the FreeRTOS tasks, and executes the first version of the control state machine. `main.c` configures the sensor drivers, configures the actuator drivers, creates the shared sensor queue through the control module, and then starts the periodic sensor acquisition tasks. It also calls `init_communication()` to start Wi-Fi and the web server.

Each sensor has its own FreeRTOS task. Each task periodically reads one sensor, creates one `sensor_event_t`, marks whether the reading is valid, and publishes the event to the shared sensor queue. A `sensor_event_t` represents one reading from one sensor at a time. The `type` field identifies the sensor, and the `value` union stores the corresponding value.

The control task consumes events from the shared queue and forwards each received sensor event to the state machine. The state machine stores the latest sensor values, tracks whether each sensor has already reported data, validates critical readings, evaluates the current operating state, performs state transitions, and applies actuator outputs through the pump and heater drivers.

The control flow is event-driven. Sensor tasks do not control GPIOs directly; they only publish readings. The control task owns the state machine, and the state machine is the only part of the control layer that decides when the fill pump, drain pump, or heater should be enabled.

## Next Steps

- Dashboard disconnection handling.
- Sensor filtering and validation.
- NVS persistence.
- Watchdog and health monitoring.
- MQTT communication.
- OTA update support.

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

## Web Server

The `communication` folder contains the Wi-Fi manager, HTTP server, manual-command adapter, and embedded dashboard. The ESP32 creates a SoftAP, and the HTTP server runs on core 0. The browser polls the control status every 2 seconds. Dashboard commands pass through a separate queue to the control task and state machine. MANUAL allows pump commands while keeping the heater off and preserving sensor checks and timeouts. Disconnection handling is still pending.

## Dashboard Access

Connect to Wi-Fi **TankController** with password **TankControl32** and open **http://192.168.4.1/**. Keep the Wi-Fi connection even without Internet. Enable **Manual Mode** before sending pump commands; disable it to return to automatic control.

## Unity Tests

Connect the ESP32 by USB, close the serial monitor, and run from the project root (adjust the port if needed):

```sh
pio test -e esp32dev_test --upload-port /dev/ttyACM0 --test-port /dev/ttyACM0
```

Tests use simulated actuators. After testing, restore the production firmware:

```sh
pio run -e esp32dev -t upload --upload-port /dev/ttyACM0
```

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
| HTTP server task | `src/communication/web_server/web_server.c` | 0 | 5 | 6144 bytes | HTTP requests |

## Operating States

| State | Activation condition | Actuator behavior |
| --- | --- | --- |
| `SYSTEM_STATE_IDLE` | Initial state, target level reached, target temperature reached, recovered from error, or no automatic action is required. | Fill pump off, drain pump off, heater off. |
| `SYSTEM_STATE_FILLING` | Activated from idle when the level distance is greater than `LEVEL_EMPTY_DISTANCE_CM`, meaning the water level is too low. | Fill pump on, drain pump off, heater off. |
| `SYSTEM_STATE_DRAINING` | Activated from idle when the level distance is lower than `LEVEL_FULL_DISTANCE_CM`, meaning the water level is too high. | Fill pump off, drain pump on, heater off. |
| `SYSTEM_STATE_HEATING` | Activated from idle when the temperature is lower than `TEMPERATURE_LOW_C`. | Fill pump off, drain pump off, heater on. |
| `SYSTEM_STATE_MANUAL` | Enabled through the dashboard with valid temperature and level readings and no blocking fault. | Selected pump follows manual commands; the other pump and heater stay off. Sensor checks and timeouts remain active. |
| `SYSTEM_STATE_ERROR` | Invalid critical sensor data, actuator failure, or operation timeout. Critical sensors are temperature and level. | Requests shutdown of all outputs and retries failed shutdowns. |
