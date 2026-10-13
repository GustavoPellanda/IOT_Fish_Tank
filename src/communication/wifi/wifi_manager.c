#include "wifi_manager.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_manager";
static esp_netif_t *ap_netif;
static esp_event_handler_instance_t wifi_event_instance;

// Report SoftAP lifecycle events without changing the tank operating mode:
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data) {
    (void)arg;
    (void)base;
    if(event_id == WIFI_EVENT_AP_START) ESP_LOGI(TAG, "SoftAP ready at 192.168.4.1");
    else if(event_id == WIFI_EVENT_AP_STOP) ESP_LOGI(TAG, "SoftAP stopped");
    else if(event_id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event = event_data;
        ESP_LOGI(TAG, "Dashboard client connected | aid=%u", (unsigned int)event->aid);
    }
    else if(event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event = event_data;
        ESP_LOGI(TAG, "Dashboard client disconnected | aid=%u reason=%u", (unsigned int)event->aid, (unsigned int)event->reason);
    }
}

// Initialize the local access point while leaving control independent of network errors:
esp_err_t wifi_manager_start(const wifi_manager_config_t *config) {
    if(config == NULL || config->ssid == NULL || config->password == NULL) {
        ESP_LOGW(TAG, "SoftAP start rejected | missing configuration");
        return ESP_ERR_INVALID_ARG;
    }
    if(ap_netif != NULL) {
        ESP_LOGW(TAG, "SoftAP start rejected | already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    size_t ssid_length = strlen(config->ssid);
    size_t password_length = strlen(config->password);
    if(ssid_length == 0 || ssid_length > 32 || password_length < 8 || password_length > 63) {
        ESP_LOGW(TAG, "SoftAP start rejected | invalid credential length");
        return ESP_ERR_INVALID_ARG;
    }
    if(config->channel < 1 || config->channel > 11 || config->max_clients < 1 || config->max_clients > 4) {
        ESP_LOGW(TAG, "SoftAP start rejected | channel=%u max_clients=%u", (unsigned int)config->channel, (unsigned int)config->max_clients);
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize storage and shared network services before creating the access point:
    esp_err_t result = nvs_flash_init();
    if(result != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(result));
        return result;
    }
    result = esp_netif_init();
    if(result != ESP_OK) {
        ESP_LOGE(TAG, "Network stack initialization failed: %s", esp_err_to_name(result));
        return result;
    }
    result = esp_event_loop_create_default();
    if(result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Default event loop initialization failed: %s", esp_err_to_name(result));
        return result;
    }

    // Assign the local address and prepare DHCP for connected clients:
    ap_netif = esp_netif_create_default_wifi_ap();
    if(ap_netif == NULL) {
        ESP_LOGE(TAG, "SoftAP interface allocation failed");
        return ESP_ERR_NO_MEM;
    }
    const char *initialization_stage = "interface address and DHCP";
    esp_netif_ip_info_t ip_info = {0};
    ip_info.ip.addr = ESP_IP4TOADDR(192, 168, 4, 1);
    ip_info.gw.addr = ESP_IP4TOADDR(192, 168, 4, 1);
    ip_info.netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0);
    result = esp_netif_dhcps_stop(ap_netif);
    if(result == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) result = ESP_OK;
    if(result == ESP_OK) result = esp_netif_set_ip_info(ap_netif, &ip_info);
    if(result == ESP_OK) result = esp_netif_dhcps_start(ap_netif);
    if(result != ESP_OK) goto netif_error;

    // Initialize the radio and register lifecycle reporting before Wi-Fi starts:
    initialization_stage = "Wi-Fi driver";
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&init_config);
    if(result != ESP_OK) goto netif_error;
    initialization_stage = "Wi-Fi event handler";
    result = esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        wifi_event_handler,
        NULL,
        &wifi_event_instance
    );
    if(result != ESP_OK) goto wifi_error;

    // Configure credentials, client limits, and WPA2 protection:
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = ssid_length,
            .channel = config->channel,
            .max_connection = config->max_clients,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            }
        }
    };
    memcpy(wifi_config.ap.ssid, config->ssid, ssid_length);
    memcpy(wifi_config.ap.password, config->password, password_length);
    // Apply the access point settings and start broadcasting the network:
    initialization_stage = "access point settings and radio startup";
    result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if(result == ESP_OK) result = esp_wifi_set_mode(WIFI_MODE_AP);
    if(result == ESP_OK) result = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if(result == ESP_OK) result = esp_wifi_start();
    if(result == ESP_OK) {
        ESP_LOGI(
            TAG,
            "SoftAP started | SSID=%s channel=%u max_clients=%u",
            config->ssid,
            (unsigned int)config->channel,
            (unsigned int)config->max_clients
        );
        return ESP_OK;
    }

    // Release only the resources acquired before the initialization failure:
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_instance);
wifi_error:
    esp_wifi_stop();
    esp_wifi_deinit();
netif_error:
    ESP_LOGE(TAG, "SoftAP initialization failed | stage=%s error=%s", initialization_stage, esp_err_to_name(result));
    esp_netif_destroy_default_wifi(ap_netif);
    ap_netif = NULL;
    return result;
}
/*
NVS errors are returned without erasing the partition, preserving any existing
configuration. ESP_ERR_INVALID_STATE from event-loop creation means the shared
default loop already exists and can be reused.

DHCP is stopped before changing the interface address. ESP_IP4TOADDR converts
each address into the byte order expected by the network stack. Starting DHCP
before the interface is up prepares it to serve clients when SoftAP starts.
WIFI_STORAGE_RAM keeps the Wi-Fi settings from being persisted to NVS.

Each error jump begins cleanup at the resources already initialized. Failure
after callback registration first unregisters it, then falls through to radio
and interface cleanup. The original error is returned to main, which can keep
the tank control tasks running even when dashboard initialization fails.
*/
