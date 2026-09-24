# State machine tests

The Unity suite runs on an ESP32 connected over USB. Sensors, relays, pumps,
and a heater are not required. The tests inject sensor events directly into
the production state machine and record actuator commands in memory.

## Layout

```text
test/
├── README.md
└── test_state_machine/
    ├── test_main.c
    ├── test_state_machine.c
    ├── test_state_machine.h
    ├── fake_actuators.c
    └── fake_actuators.h
```

- `test_main.c` provides the test application's `app_main()` and waits two
  seconds for the serial runner before starting Unity.
- `test_state_machine.c` contains the cases, per-test setup, and assertions.
- `fake_actuators.c` replaces the pump and heater setters without accessing GPIO.
  It records commands, detects overlapping outputs, and supports one-shot errors.

## Build separation

The `esp32dev_test` environment uses `test_framework = unity` and
`test_build_src = yes`. PlatformIO supplies the Unity test dependency and
compiles the selected files under `test/test_state_machine/`.

The CMake option `TANK_STATE_MACHINE_TEST` selects only
`src/core/state_machine.c` from the application. The normal `src/main.c`,
sensor/control tasks, and real actuator drivers are excluded from this suite.
The production state machine is reused directly; its logic is not copied into
any test file.

The test environment shares `sdkconfig.esp32dev` with the application so both
use the same ESP-IDF configuration. Their build artifacts remain in separate
PlatformIO environment directories. The default environment stays `esp32dev`,
so a normal application upload does not run these tests.

## Run the suite

With the ESP32 connected and its serial port available:

```sh
pio test -e esp32dev_test
```

PlatformIO builds and uploads the test firmware, then reads the Unity results
from the serial port at 115200 baud. To select a port explicitly:

```sh
pio test -e esp32dev_test --upload-port /dev/ttyACM0 --test-port /dev/ttyACM0
```

This replaces the application currently stored on the board. Restore the
normal application after testing:

```sh
pio run -e esp32dev -t upload
```

Run these commands explicitly when hardware execution is intended. Tests are
not automatically invoked by a normal build or upload.

## Covered behavior

The suite includes 25 test cases covering:

- Initialization, missing initial readings, and null arguments.
- Filling, draining, heating, threshold boundaries, and hysteresis.
- Level control taking priority over starting heating from idle.
- Immediate heater shutdown when a valid distance exceeds 25 cm.
- Refilling and resuming heating after a low-water interruption.
- Critical sensor failures, retained last-valid values, and recovery.
- Luminosity failures remaining noncritical.
- Driver error propagation and shutdown/enable command ordering.
- Detection of conflicting outputs between individual successful driver calls.

Distance increases as the water level falls. A distance of exactly 25 cm does
not interrupt heating; a distance above 25 cm does. The low-water event moves
the state to `IDLE` and requests all outputs off. A subsequent event can start
`FILLING` through the existing idle rules. Filling continues until the distance
reaches 14 cm or less, then the normal temperature rules can start heating on
a later event.

Heater shutdown is attempted before pump commands. Heating is enabled only
after both pump-off commands succeed. If a driver reports an error, it is
returned to the caller; this does not confirm a physical actuator changed state.

The 25 cm cutoff reuses the current empty threshold and must be reviewed
against the physical heater placement before operating a real tank.

These tests cover state-machine behavior and requested outputs. They do not
validate physical sensors, relay polarity, FreeRTOS queue/task integration,
or electrical timing. Those require separate integration and hardware tests.

## References

- [PlatformIO Unity integration](https://docs.platformio.org/en/latest/advanced/unit-testing/frameworks/unity.html)
- [PlatformIO test source selection](https://docs.platformio.org/en/latest/projectconf/sections/env/options/test/test_build_src.html)
