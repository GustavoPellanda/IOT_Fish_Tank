# Project Instructions

This is an ESP32-WROOM-32 firmware modernization project.

## Development principles

- The user remains responsible for the architecture and design decisions.
- Do not make large-scale changes unless explicitly requested.
- Prefer small, incremental changes.
- Do not modify unrelated code.
- Preserve existing behavior unless the task explicitly requires changing it.
- Do not introduce new dependencies without explaining why.
- Do not silently change architectural decisions defined in the specification.
- When requirements are ambiguous, ask instead of guessing.

## Project documentation

Before implementing a feature or making a behavioral change:

1. Read `docs/firmware-specification.md`.
2. Read `docs/coding-style.md` when modifying source code.
3. Inspect the existing implementation before proposing changes.
4. Preserve the architecture defined by the specification unless the user explicitly requests an architectural change.

## Validation

After modifying code:

1. Build the project.
2. Report compilation errors and warnings.
3. Show the relevant changes.
4. Do not create commits unless explicitly requested.