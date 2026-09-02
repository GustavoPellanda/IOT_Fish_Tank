# Project Instructions

This project is an embedded IoT controller for automated monitoring and control of tank systems, based on the ESP32-WROOM-32.

The firmware is built with ESP-IDF and FreeRTOS and is designed as a modular, event-driven embedded system. It continuously acquires and validates sensor data, executes automatic control logic through a state machine, and manages actuators such as pumps, valves, and heaters.

The architecture prioritizes separation of concerns, safe state transitions, fault detection, maintainability, scalability, and reliable autonomous operation.

## Development principles

* The user remains responsible for the architecture and design decisions.
* Do not make large-scale changes unless explicitly requested.
* Prefer small, incremental changes.
* Do not modify unrelated code.
* Preserve existing behavior unless the task explicitly requires changing it.
* Do not introduce new dependencies without explaining why.
* Do not silently change architectural decisions defined in the specification.
* When requirements are ambiguous, ask instead of guessing.

## Project documentation

Before implementing changes:

1. Read `docs/firmware-specification.md`.
2. Read `docs/coding-style.md` when modifying source code.
3. Inspect the existing implementation before proposing changes.
4. Preserve the architecture defined by the specification unless the user explicitly requests an architectural change.

## Validation

* Do not execute build, test, flash, or other shell commands automatically.
* Only modify the files necessary to fulfill the user's request.
* Report the relevant changes after modifying the code.
* Do not create commits unless explicitly requested.