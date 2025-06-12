#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h" // For ESP_MAC_ADDR_LEN
#include "esp_timer.h" 

#include "gpio.h"

#include "common.h" 
#include "EspNowSender.h"

#define ESPNOW_SEND_DELAY_MS 1000 // Send every 1000ms

// !!! IMPORTANT: Replace this with the MAC address of the RECEIVER-ESP32 !!!
static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Example: Broadcast, replace!
// Example for a specific MAC:
// static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = {0xE4, 0xB0, 0x63, 0x15, 0xF6, 0x24};

static const char *TAG = "sender";

// forward declarations
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status);
static void SENDER_sender_task(void *pvParameter);

void SENDER_init(void) {
    // Start Sender Task
    xTaskCreate(SENDER_sender_task, "sender_task", 2048, NULL, 5, NULL);
}

// ESP-NOW Initialization
static esp_err_t sender_espnow_init(void) {
    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());

    // Register Send Callback
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    // Add peer
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer info fail");
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = 0; // 0 means current channel
    peer->ifidx = ESP_IF_WIFI_STA;
    peer->encrypt = false; // No encryption for this example
    memcpy(peer->peer_addr, s_peer_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);

    return ESP_OK;
}

// Send Callback
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "Send CB mac_addr is NULL");
        return;
    }
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Send success to " MACSTR, MAC2STR(mac_addr));
        
        GPIO_toggle_led();
        COMMON_callback_called();

    } else {
        ESP_LOGW(TAG, "Send fail to " MACSTR ", Status: %d", MAC2STR(mac_addr), status);
    }
}

// Main Sender Task
static void SENDER_sender_task(void *pvParameter) {
    uint32_t counter = 0;
    char send_data[32];

    if (sender_espnow_init() != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW initialization failed");
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "ESP-NOW Sender Initialized. Sending data to " MACSTR, MAC2STR(s_peer_mac));

    while (1) {
        snprintf(send_data, sizeof(send_data), "Ping %lu", counter++);
        esp_err_t result = esp_now_send(s_peer_mac, (uint8_t *)send_data, strlen(send_data));

        if (result == ESP_OK) {
            // ESP_LOGI(TAG, "Sent data: %s", send_data); // Logging can be handled by Send Callback
        } else {
            ESP_LOGE(TAG, "Error sending data: %s", esp_err_to_name(result));
        }
        vTaskDelay(pdMS_TO_TICKS(ESPNOW_SEND_DELAY_MS));
    }
}