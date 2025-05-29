/* ESP32-C3-TFT-CUBE

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "esp_lvgl_port.h"
#include "nvs_flash.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h" // Für ESP_MAC_ADDR_LEN
#include "esp_timer.h" 

#include "bsp/esp-bsp.h"

//#define CUBE_SENDER 
#ifdef CUBE_SENDER 
static const char *TAG = "cube_send";
#else
static const char *TAG = "cube_rec";
#endif

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO
#define GPIO_BUTTON_1 8
#define GPIO_BUTTON_2 10

#ifdef CUBE_SENDER 
#define ESPNOW_SEND_DELAY_MS 1000 // Sende alle 1000ms
#else
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
#endif

static uint8_t s_led_state = 0;
static int8_t s_arc_value = 100;
static uint32_t s_arc_count = 0;
static int64_t s_last_time_recv_cb_us = 0;

// !!! WICHTIG: Ersetzen Sie dies mit der MAC-Adresse des EMPFÄNGER-ESP32 !!!
// static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Beispiel: Broadcast, ersetzen!
// Beispiel für eine spezifische MAC:
static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = {0xE4, 0xB0, 0x63, 0x15, 0xF6, 0x24};
// static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = {0xE4, 0xB0, 0x63, 0x15, 0xF7, 0xB0};

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

#ifdef CUBE_SENDER 
// Send Callback
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "Send CB mac_addr is NULL");
        return;
    }
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Send success to " MACSTR, MAC2STR(mac_addr));
        
        blink_led();
        /* Toggle the LED state */
        s_led_state = !s_led_state;
    } else {
        ESP_LOGW(TAG, "Send fail to " MACSTR ", Status: %d", MAC2STR(mac_addr), status);
    }
}
#else
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

    blink_led();
    /* Toggle the LED state */
    s_led_state = !s_led_state;

    s_last_time_recv_cb_us = esp_timer_get_time();

    // Daten empfangen und loggen
    char received_data[len + 1];
    memcpy(received_data, data, len);
    received_data[len] = '\0'; // Null-Terminierung für String-Ausgabe
    ESP_LOGI(TAG, "Received data from " MACSTR ": %s (RSSI: %d)", MAC2STR(mac_addr), received_data, rssi);

    // Distanz schätzen und ausgeben
    float distance = estimate_distance(rssi);
    if (distance >= 0) {
        ESP_LOGI(TAG, "Estimated distance: %.2f meters", distance);
        s_arc_value = (uint8_t)distance;
    } else {
        ESP_LOGW(TAG, "Could not estimate distance (invalid parameters or calculation). RSSI was %d", rssi);
    }
}
#endif

void lv_screen_timer_arc(lv_timer_t* timer)
{
  /*Use the user_data*/
  lv_obj_t* user = lv_timer_get_user_data(timer);

  lv_arc_set_value(user, s_arc_value);

  #ifdef CUBE_SENDER
  s_arc_value = 50.0 * cos((2.0*3.184*s_arc_count)/200.0);
  s_arc_value += 50;
  s_arc_count++;
  #endif
}

void lv_screen_timer_led(lv_timer_t* timer)
{
  /*Use the user_data*/
  lv_obj_t* user = lv_timer_get_user_data(timer);

  #ifdef CUBE_SENDER
  lv_led_toggle(user);
  #else
  int64_t time_ms = (esp_timer_get_time() - s_last_time_recv_cb_us) / 1000;

  if( time_ms < 2000 ) {
    lv_led_set_color(user, lv_palette_main(LV_PALETTE_GREEN));
  } else if ( time_ms < 4000 ) {
    lv_led_set_color(user, lv_palette_main(LV_PALETTE_YELLOW));
  } else if ( time_ms < 8000 ) {
    lv_led_set_color(user, lv_palette_main(LV_PALETTE_ORANGE));
    lv_led_toggle(user);
  } else {
    lv_led_set_color(user, lv_palette_main(LV_PALETTE_RED));
    lv_led_toggle(user);
    s_arc_value = 100;
  }

  #endif
}

void lv_screen_timer_label(lv_timer_t* timer)
{
  /*Use the user_data*/
  lv_obj_t* user = lv_timer_get_user_data(timer);
  int64_t time_ms = (esp_timer_get_time() - s_last_time_recv_cb_us) / 1000;

  if( s_last_time_recv_cb_us > 0 ) {
        if( time_ms < 2000 ) {
            lv_label_set_text(user, "Verbunden");
        } else if ( time_ms < 10000 ) {
            lv_label_set_text(user, "Warte...");
        } else {
            lv_label_set_text(user, "Keine Verbindung!");
        }
    }
}

void lv_screen_1(void)
{
    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);

    /*Create a white label, set its text and align it to the center*/
    uint8_t mac[8] = {0};
    char mac_string[32] = {0};

    esp_read_mac(&mac, ESP_IF_WIFI_STA);
    snprintf(&mac_string, 31, "%02X %02X %02X %02X %02X %02X ", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, &mac_string);
    lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);

    /*Create an Arc*/
    lv_obj_t * arc = lv_arc_create(lv_screen_active());
    #ifdef CUBE_SENDER
    lv_arc_set_mode(arc, LV_ARC_MODE_SYMMETRICAL);
    #else
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    #endif
    lv_obj_set_size(arc, 110, 110);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_obj_set_style_arc_color(arc,lv_palette_main(LV_PALETTE_INDIGO), LV_PART_INDICATOR);
    lv_arc_set_value(arc, s_arc_value);
    lv_obj_center(arc);

    /*Create a LED*/
    lv_obj_t * led = lv_led_create(lv_screen_active());
    lv_obj_align(led, LV_ALIGN_CENTER, 0, 0);
    lv_led_set_brightness(led, 150);
    lv_led_set_color(led, lv_palette_main(LV_PALETTE_INDIGO));
    lv_led_on(led);

    lv_timer_t * timer_label = lv_timer_create(lv_screen_timer_label, 500, label);
    lv_timer_t * timer_led = lv_timer_create(lv_screen_timer_led, 1000, led);
    lv_timer_t * timer_arc = lv_timer_create(lv_screen_timer_arc, 100, arc);
}

static void configure_io(void)
{
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

void app_lvgl_display(void)
{
    bsp_display_lock(0);
    lv_screen_1();
    bsp_display_unlock();
}

// WiFi Initialisierung
static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init()); // Initialisiert TCP/IP-Stack (notwendig für Wi-Fi)
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // Erstellt Default Event Loop
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // Initialisiert Wi-Fi Treiber
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM)); // Speicher für Wi-Fi Config im RAM
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // Station Mode ist für ESP-NOW erforderlich
    ESP_ERROR_CHECK(esp_wifi_start()); // Startet Wi-Fi

    // Optional: Long Range Mode (kann helfen, erfordert aber auch am Empfänger)
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));

    // Eigene MAC-Adresse ausgeben (hilfreich für den Sender)
    // uint8_t self_mac[ESP_MAC_ADDR_LEN];
    // esp_wifi_get_mac(ESP_IF_WIFI_STA, self_mac);
    // ESP_LOGI(TAG, "Receiver MAC Address: " MACSTR, MAC2STR(self_mac));
}

#ifdef CUBE_SENDER 
// ESP-NOW Initialisierung
static esp_err_t espnow_init(void) {
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
    peer->channel = 0; // 0 bedeutet aktueller Kanal
    peer->ifidx = ESP_IF_WIFI_STA;
    peer->encrypt = false; // Keine Verschlüsselung für dieses Beispiel
    memcpy(peer->peer_addr, s_peer_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);

    return ESP_OK;
}
#else
// ESP-NOW Initialisierung
static esp_err_t espnow_init(void) {
    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());

    // Register Receive Callback
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    return ESP_OK;
}
#endif

#ifdef CUBE_SENDER 
// Haupt-Task des Senders
static void sender_task(void *pvParameter) {
    uint32_t counter = 0;
    char send_data[32];

    if (espnow_init() != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW initialization failed");
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "ESP-NOW Sender Initialized. Sending data to " MACSTR, MAC2STR(s_peer_mac));

    while (1) {
        snprintf(send_data, sizeof(send_data), "Ping %lu", counter++);
        esp_err_t result = esp_now_send(s_peer_mac, (uint8_t *)send_data, strlen(send_data));

        if (result == ESP_OK) {
            // ESP_LOGI(TAG, "Sent data: %s", send_data); // Loggen ggf. Send Callback überlassen
        } else {
            ESP_LOGE(TAG, "Error sending data: %s", esp_err_to_name(result));
        }
        vTaskDelay(pdMS_TO_TICKS(ESPNOW_SEND_DELAY_MS));
    }
}
#endif

/**
 * @brief Task zur Überwachung des Buttons und Auslösung der Label-Aktualisierung.
 */
static void button_monitoring_task(void *pvParameter) {
    ESP_LOGI(TAG, "Button Monitoring Task gestartet");

    gpio_config_t io_conf;
    // Interrupt für fallende Flanke deaktivieren (wir pollen)
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // Bitmaske des Pins
    io_conf.pin_bit_mask = (1ULL << GPIO_BUTTON_1) | (1ULL << GPIO_BUTTON_2);
    // Als Input setzen
    io_conf.mode = GPIO_MODE_INPUT;
    // Pull-up aktivieren
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    bool last_button_state[2] = {true}; 
    TickType_t last_press_time[2] = {0};
    const TickType_t debounce_delay = pdMS_TO_TICKS(50); // 50ms Entprellzeit

    while (1) {
        bool current_button_state[2] = {gpio_get_level(GPIO_BUTTON_1), gpio_get_level(GPIO_BUTTON_2)};

        // Prüfen auf fallende Flanke (Button gedrückt)
        if (last_button_state[0] == true && current_button_state[0] == false) {
            // Entprellen
            if ((xTaskGetTickCount() - last_press_time[0]) > debounce_delay) {
                last_press_time[0] = xTaskGetTickCount();
                ESP_LOGI(TAG, "Button an GPIO %d gedrückt!", GPIO_BUTTON_1);
            }
        }
        // Prüfen auf fallende Flanke (Button gedrückt)
        if (last_button_state[1] == true && current_button_state[1] == false) {
            // Entprellen
            if ((xTaskGetTickCount() - last_press_time[1]) > debounce_delay) {
                last_press_time[1] = xTaskGetTickCount();
                ESP_LOGI(TAG, "Button an GPIO %d gedrückt!", GPIO_BUTTON_2);
            }
        }
        last_button_state[0] = current_button_state[0];
        last_button_state[1] = current_button_state[1];
        vTaskDelay(pdMS_TO_TICKS(20)); // Alle 20ms prüfen
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initialize I2C bus");

    /* Configure LED  */
    configure_io();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    wifi_init();

    /* Configure Display  */
    if (bsp_display_start() == NULL) {
        ESP_LOGE(TAG, "display start failed!");
        abort();
    }
    app_lvgl_display();

    // Starte die Task zur Button-Überwachung
    xTaskCreate(button_monitoring_task, "button_task", 2048, NULL, 10, NULL);

    #ifdef CUBE_SENDER 
    // Start Sender Task
    xTaskCreate(sender_task, "sender_task", 4096, NULL, 5, NULL);
    #else
    // Initialize ESP-NOW
    if (espnow_init() != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW initialization failed");
        return;
    }

    ESP_LOGI(TAG, "ESP-NOW Receiver Initialized. Waiting for data...");
    // Der Haupttask kann hier einfach warten oder andere Dinge tun.
    // Die Arbeit geschieht im ESP-NOW Receive Callback.
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Nur um zu verhindern, dass app_main endet
    }
    #endif

}
