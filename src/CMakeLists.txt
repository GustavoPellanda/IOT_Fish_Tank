if(TANK_STATE_MACHINE_TEST)
    # PlatformIO adds the test runner and fake actuators from test/ separately.
    set(app_sources "core/state_machine.c")
    idf_component_register(SRCS ${app_sources} INCLUDE_DIRS ".")
else()
    file(GLOB_RECURSE app_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
    idf_component_register(
        SRCS ${app_sources}
        INCLUDE_DIRS "."
        PRIV_REQUIRES esp_adc driver esp_timer freertos esp_wifi esp_event esp_netif nvs_flash esp_http_server espressif__cjson
        EMBED_TXTFILES "communication/web_server/dashboard.html"
    )
endif()
