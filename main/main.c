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
#include "gpio.h"
#include "common.h"
#include "sender.h"
#include "receiver.h"

static const char *TAG = "main";

static bool s_globSelectionDone = false;
static bool s_globIsReceiver = true;

void lv_screen_timer_arc(lv_timer_t* timer)
{
  /*Use the user_data*/
  lv_obj_t* user = lv_timer_get_user_data(timer);

    if( s_globIsReceiver ) {
        lv_arc_set_value(user, RECEIVER_getDistance());
    }
    else {
        int32_t value = 50.0 * cos((2.0*3.184*xTaskGetTickCount())/400.0);
        value += 50;
        lv_arc_set_value(user, value);
    }
}

void lv_screen_timer_led(lv_timer_t* timer)
{
  /*Use the user_data*/
  lv_obj_t* user = lv_timer_get_user_data(timer);

  lv_led_toggle(user);

  int64_t time_ms = (esp_timer_get_time() - COMMON_get_time_of_last_callback()) / 1000;

  if( time_ms < 2000 ) {
    lv_led_set_color(user, lv_palette_main(LV_PALETTE_GREEN));
  } else if ( time_ms < 4000 ) {
    lv_led_set_color(user, lv_palette_main(LV_PALETTE_YELLOW));
  } else if ( time_ms < 8000 ) {
    lv_led_set_color(user, lv_palette_main(LV_PALETTE_ORANGE));
  } else {
    lv_led_set_color(user, lv_palette_main(LV_PALETTE_RED));
  }

}

void lv_screen_timer_label(lv_timer_t* timer)
{
  /*Use the user_data*/
  lv_obj_t* user = lv_timer_get_user_data(timer);
  int64_t time_ms = (esp_timer_get_time() - COMMON_get_time_of_last_callback()) / 1000;

  if( COMMON_get_time_of_last_callback() > 0 ) {
        if( time_ms < 2000 ) {
            lv_label_set_text(user, "Verbunden");
        } else if ( time_ms < 10000 ) {
            lv_label_set_text(user, "Warte...");
        } else {
            lv_label_set_text(user, "Keine Verbindung!");
        }
    }
}

void lv_screen_0(void)
{
    char label_string[32] = {0};

    if( s_globIsReceiver ) {
        snprintf(&label_string, 31, "Receiver");
    }
    else {
        snprintf(&label_string, 31, "Sender");
    }

    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, &label_string);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
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

    if( s_globIsReceiver ) {
        lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    }
    else {
        lv_arc_set_mode(arc, LV_ARC_MODE_SYMMETRICAL);
    }
    lv_arc_set_mode(arc, LV_ARC_MODE_SYMMETRICAL);

    lv_obj_set_size(arc, 110, 110);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_obj_set_style_arc_color(arc,lv_palette_main(LV_PALETTE_INDIGO), LV_PART_INDICATOR);
    lv_arc_set_value(arc, 100);
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

void app_lvgl_display(void)
{
    bsp_display_lock(0);
    if( s_globSelectionDone ) {
        lv_screen_1();    
    }
    else {
        lv_screen_0();
    }
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

void app_main(void)
{
    ESP_LOGI(TAG, "Initialize I2C bus");

    /* Configure LED  */
    GPIO_configure_io();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    wifi_init();

    // Starte die Task zur Button-Überwachung
    xTaskCreate(GPIO_button_monitoring_task, "button_task", 2048, NULL, 10, NULL);

    /* Configure Display  */
    if (bsp_display_start() == NULL) {
        ESP_LOGE(TAG, "display start failed!");
        abort();
    }
    app_lvgl_display();

    if( s_globIsReceiver ) {
        RECEIVER_init();
        
        ESP_LOGI(TAG, "ESP-NOW Receiver Initialized. Waiting for data...");
        // Der Haupttask kann hier einfach warten oder andere Dinge tun.
        // Die Arbeit geschieht im ESP-NOW Receive Callback.
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(5000)); // Nur um zu verhindern, dass app_main endet
        }
    }
    else {
        SENDER_init();
    }
}
