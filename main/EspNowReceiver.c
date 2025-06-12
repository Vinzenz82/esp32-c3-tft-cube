#include <string.h>
#include <math.h>
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

#include "bsp/esp-bsp.h"
#include "gpio.h"

#include "common.h" 

#include "EspNowReceiver.h"

// --- CALIBRATION CONSTANTS (PLEASE ADJUST!) ---
// This is the expected RSSI value at a distance of 1 meter.
// Measure this in your environment! Typical values range between -40 and -60 dBm.
static float s_rssi_at_1_meter = -51.0f;

// Path Loss Exponent (n).
// Describes how quickly the signal decreases with distance.
// - Free space: ~2.0
// - Indoor (line of sight): 1.6 - 1.8
// - Indoor (with obstacles): 2.0 - 4.0 (can vary significantly!)
// Start with ~2.5 or 3.0 and adjust as needed.
#define PATH_LOSS_EXPONENT (1.64f)
// ---------------------------------------------------

static const char *TAG = "receiver";
static float s_arc_value = 100.0f;
static int16_t s_rssi_value = 0;

// forward declarations
static esp_err_t RECEIVER_espnow_init(void);
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);

// Function to estimate distance based on RSSI
// Uses the simplified log-distance path loss model: RSSI = A - 10*n*log10(d)
// where A = RSSI_AT_1_METER, n = PATH_LOSS_EXPONENT, d = distance
// Rearranged for d: d = 10^((A - RSSI) / (10 * n))
static float estimate_distance(int16_t rssi) {
    // Avoid division by zero or logarithm of non-positive numbers (theoretical)
    if (PATH_LOSS_EXPONENT == 0) return -1.0; // Invalid exponent

    float exponent = (s_rssi_at_1_meter - (float)rssi) / (10.0f * PATH_LOSS_EXPONENT);
    return powf(10.0f, exponent);
}

float RECEIVER_getRssiAt1Meter(void) {
    return s_rssi_at_1_meter;
}

void RECEIVER_setRssiAt1Meter(void) {
    s_rssi_at_1_meter = s_rssi_value;
}

void RECEIVER_init(void) {
    if (RECEIVER_espnow_init() != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW initialization failed");
        return;
    }
}

// ESP-NOW initialization
static esp_err_t RECEIVER_espnow_init(void) {
    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());

    // Register Receive Callback
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    return ESP_OK;
}

// Receive Callback
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (recv_info == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive CB error");
        return;
    }

    uint8_t *mac_addr = recv_info->src_addr;
    int16_t rssi = recv_info->rx_ctrl->rssi; // Get RSSI from RX control info
    s_rssi_value = rssi;

    if (mac_addr == NULL) {
         ESP_LOGE(TAG, "Receive CB mac_addr is NULL");
        return;
    }

    GPIO_toggle_led();
    COMMON_callback_called();

    // Receive and log data
    char received_data[len + 1];
    memcpy(received_data, data, len);
    received_data[len] = '\0'; // Null termination for string output
    ESP_LOGI(TAG, "Received data from " MACSTR ": %s (RSSI: %d)", MAC2STR(mac_addr), received_data, rssi);

    // Estimate and output distance
    float distance = estimate_distance(rssi);
    if (distance >= 0) {
        ESP_LOGI(TAG, "Estimated distance: %.2f meters", distance);
        s_arc_value = distance;
    } else {
        ESP_LOGW(TAG, "Could not estimate distance (invalid parameters or calculation). RSSI was %d", rssi);
        s_arc_value = 0.0f;
    }
}

float RECEIVER_getDistance(void) {
    return s_arc_value;
}

int16_t RECEIVER_getRSSI(void) {
    return s_rssi_value;
}
