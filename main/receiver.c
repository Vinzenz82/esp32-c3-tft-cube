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
#include "esp_mac.h" // Für ESP_MAC_ADDR_LEN
#include "esp_timer.h" 

#include "bsp/esp-bsp.h"
#include "gpio.h"

#include "receiver.h"
#include "common.h" 

// --- KALIBRIERUNGSKONSTANTEN (BITTE ANPASSEN!) ---
// Dies ist der erwartete RSSI-Wert bei einem Abstand von 1 Meter.
// Messen Sie dies in Ihrer Umgebung! Typische Werte liegen zwischen -40 und -60 dBm.
#define RSSI_AT_1_METER -75.0

// Pfadverlustexponent (Path Loss Exponent - n).
// Beschreibt, wie schnell das Signal mit der Entfernung abnimmt.
// - Freier Raum: ~2.0
// - Innenräume (Sichtlinie): 1.6 - 1.8
// - Innenräume (mit Hindernissen): 2.0 - 4.0 (kann stark variieren!)
// Beginnen Sie mit ~2.5 oder 3.0 und passen Sie an.
#define PATH_LOSS_EXPONENT 2.5
// ---------------------------------------------------

static const char *TAG = "receiver";
static int16_t s_arc_value = 100;

// forward declarations
static esp_err_t RECEIVER_espnow_init(void);
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);

// Funktion zur Schätzung der Entfernung basierend auf RSSI
// Verwendet das vereinfachte Log-Distanz-Pfadverlustmodell: RSSI = A - 10*n*log10(d)
// wobei A = RSSI_AT_1_METER, n = PATH_LOSS_EXPONENT, d = Distanz
// Umgestellt nach d: d = 10^((A - RSSI) / (10 * n))
static float estimate_distance(int rssi) {
    // Vermeide Division durch Null oder Logarithmus von Nicht-Positiven Zahlen (theoretisch)
    if (PATH_LOSS_EXPONENT == 0) return -1.0; // Ungültiger Exponent

    float exponent = (RSSI_AT_1_METER - (float)rssi) / (10.0f * PATH_LOSS_EXPONENT);
    return powf(10.0f, exponent);
}

void RECEIVER_init(void) {
    if (RECEIVER_espnow_init() != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW initialization failed");
        return;
    }
}

// ESP-NOW Initialisierung
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
    int rssi = recv_info->rx_ctrl->rssi; // RSSI aus den RX Control Infos holen

    if (mac_addr == NULL) {
         ESP_LOGE(TAG, "Receive CB mac_addr is NULL");
        return;
    }

    GPIO_toggle_led();
    COMMON_callback_called();

    // Daten empfangen und loggen
    char received_data[len + 1];
    memcpy(received_data, data, len);
    received_data[len] = '\0'; // Null-Terminierung für String-Ausgabe
    ESP_LOGI(TAG, "Received data from " MACSTR ": %s (RSSI: %d)", MAC2STR(mac_addr), received_data, rssi);

    // Distanz schätzen und ausgeben
    float distance = estimate_distance(rssi);
    if (distance >= 0) {
        ESP_LOGI(TAG, "Estimated distance: %.2f meters", distance);
        s_arc_value = (uint16_t)distance;
    } else {
        ESP_LOGW(TAG, "Could not estimate distance (invalid parameters or calculation). RSSI was %d", rssi);
    }
}

int16_t RECEIVER_getDistance(void) {
    return s_arc_value;
}