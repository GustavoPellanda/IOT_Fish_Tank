if(TANK_STATE_MACHINE_TEST)
    # PlatformIO adds the test runner and fake actuators from test/ separately.
    set(app_sources "core/state_machine.c")
else()
    file(GLOB_RECURSE app_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.*")
endif()

idf_component_register(SRCS ${app_sources} INCLUDE_DIRS ".")
