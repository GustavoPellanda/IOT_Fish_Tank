#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>

#include "esp_err.h"

typedef struct {
    const char *ssid;     // Access point name, from 1 to 32 bytes
    const char *password; // WPA2 password, from 8 to 63 bytes
    uint8_t channel;      // Wi-Fi channel, from 1 to 11
    uint8_t max_clients;  // Maximum connected stations, from 1 to 4
} wifi_manager_config_t;

/*
Initialize NVS, the network interface, and a password-protected SoftAP at
192.168.4.1. The call returns after starting Wi-Fi, without waiting for a client;
the HTTP server must be started separately.
*/
esp_err_t wifi_manager_start(
    const wifi_manager_config_t *config // Access point credentials and radio settings
);

#endif

/*
 * How the Wi-Fi manager works:
 * The ESP32 creates a local network so a phone or computer can reach the
 * dashboard directly. ESP-IDF manages Wi-Fi tasks and delivers connection
 * events to the registered callback. Those events currently provide logging;
 * they do not change the tank operating mode.
 */
