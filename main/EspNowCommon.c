#include "EspNowCommon.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

static const char *TAG = "EspNowCommon";

esp_err_t esp_now_wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init()); // Initialize TCP/IP stack (required for Wi-Fi)
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // Create default event loop
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // Initialize Wi-Fi driver
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM)); // Store Wi-Fi config in RAM
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // Station mode is required for ESP-NOW
    ESP_ERROR_CHECK(esp_wifi_start()); // Start Wi-Fi

    // Optional: Long Range Mode (can help, but also requires receiver support)
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));

    return ESP_OK;
} 