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

/* Global structure to hold all LVGL objects */
typedef struct {
    /* Screen 0 objects */
    lv_obj_t *label_selection;
    lv_obj_t *btn_set;
    lv_obj_t *label_set;
    lv_obj_t *btn_enter;
    lv_obj_t *label_enter;
    
    /* Screen 1 objects */
    lv_obj_t *label_mac;
    lv_obj_t *arc;
    lv_obj_t *led;
} lvgl_objects_t;

typedef struct {
    /* Screen 0 objects */
    lv_timer_t *screen_0_selection;
    
    /* Screen 1 objects */
    lv_timer_t *screen_1_label;
    lv_timer_t *screen_1_led;
    lv_timer_t *screen_1_arc;
} lvgl_timers_t;

static lvgl_objects_t g_lvgl_objects = {0};
static lvgl_timers_t g_lvgl_timers = {0};

/* Mutex for thread safety */
static SemaphoreHandle_t g_lvgl_mutex = NULL;

static bool s_globSelectionDone = false;
static bool s_globIsReceiver = true;

void toggle_is_receiver(void) {
    if (xSemaphoreTake(g_lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        if( s_globSelectionDone == false ) {
            s_globIsReceiver = !s_globIsReceiver;
        }
        xSemaphoreGive(g_lvgl_mutex);
    }
}

void set_selection_done(void) {
    if (xSemaphoreTake(g_lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        s_globSelectionDone = true;
        xSemaphoreGive(g_lvgl_mutex);
    }
}

void lv_screen_timer_arc(lv_timer_t* timer)
{
    /*Use the user_data*/
    lv_obj_t* user = lv_timer_get_user_data(timer);
    
    if (user == NULL) {
        return;
    }

    if (xSemaphoreTake(g_lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        if( s_globIsReceiver ) {
            lv_arc_set_value(user, RECEIVER_getDistance());
        }
        else {
            int32_t value = 50.0 * cos((2.0*3.184*xTaskGetTickCount())/400.0);
            value += 50;
            lv_arc_set_value(user, value);
        }
        xSemaphoreGive(g_lvgl_mutex);
    }
}

void lv_screen_timer_led(lv_timer_t* timer)
{
    /*Use the user_data*/
    lv_obj_t* user = lv_timer_get_user_data(timer);
    
    if (user == NULL) {
        return;
    }

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
    
    if (user == NULL) {
        return;
    }

    int64_t time_ms = (esp_timer_get_time() - COMMON_get_time_of_last_callback()) / 1000;

    if (xSemaphoreTake(g_lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        if( COMMON_get_time_of_last_callback() > 0 ) {
            if( time_ms < 2000 ) {
                if( s_globIsReceiver ) {
                    uint16_t distance = RECEIVER_getDistance();
                    int16_t rssi = RECEIVER_getRSSI();

                    char distance_str[32];
                    snprintf(distance_str, sizeof(distance_str), "< %um (%d)", distance, rssi);
                    lv_label_set_text(user, distance_str);
                }
                else {
                    lv_label_set_text(user, "Broadcasting...");
                }
            } else if ( time_ms < 10000 ) {
                lv_label_set_text(user, "Waiting...");
            } else {
                lv_label_set_text(user, "No connection!");
            }
        }
        xSemaphoreGive(g_lvgl_mutex);
    }
}

void lv_screen_timer_label_selection(lv_timer_t* timer)
{
      /*Use the user_data*/
    lv_obj_t* user = lv_timer_get_user_data(timer);

    if (user == NULL) {
        return;
    }

    if (g_lvgl_objects.label_selection == NULL) {
        return;
    }
    
    if( s_globIsReceiver ) {
        lv_label_set_text(user, "Receiver");
    }
    else {
        lv_label_set_text(user, "Sender");
    }

}

void lv_screen_0(void)
{
    char label_string[32] = {0};

    if( s_globIsReceiver ) {
        snprintf(label_string, sizeof(label_string), "Receiver");
    }
    else {
        snprintf(label_string, sizeof(label_string), "Sender");
    }

    g_lvgl_objects.label_selection = lv_label_create(lv_screen_active());
    lv_label_set_text(g_lvgl_objects.label_selection, (const char *)&label_string);
    lv_obj_set_style_text_font(g_lvgl_objects.label_selection, &lv_font_montserrat_14, 0);
    lv_obj_align(g_lvgl_objects.label_selection, LV_ALIGN_CENTER, 0, 0);

    /*Create Set and Enter buttons*/
    g_lvgl_objects.btn_set = lv_btn_create(lv_screen_active());
    lv_obj_set_size(g_lvgl_objects.btn_set, 35, 20);
    lv_obj_align(g_lvgl_objects.btn_set, LV_ALIGN_TOP_LEFT, 2, 10);
    g_lvgl_objects.label_set = lv_label_create(g_lvgl_objects.btn_set);
    lv_obj_set_style_text_font(g_lvgl_objects.label_set, &lv_font_montserrat_12, 0);
    lv_label_set_text(g_lvgl_objects.label_set, "Set");
    lv_obj_center(g_lvgl_objects.label_set);

    g_lvgl_objects.btn_enter = lv_btn_create(lv_screen_active());
    lv_obj_set_size(g_lvgl_objects.btn_enter, 45, 20);
    lv_obj_align(g_lvgl_objects.btn_enter, LV_ALIGN_BOTTOM_LEFT, 2, -10);
    g_lvgl_objects.label_enter = lv_label_create(g_lvgl_objects.btn_enter);
    lv_obj_set_style_text_font(g_lvgl_objects.label_enter, &lv_font_montserrat_12, 0);
    lv_label_set_text(g_lvgl_objects.label_enter, "Enter");
    lv_obj_center(g_lvgl_objects.label_enter);

    g_lvgl_timers.screen_0_selection = lv_timer_create(lv_screen_timer_label_selection, 20, g_lvgl_objects.label_selection);
}

void lv_screen_1(void)
{
    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);

    /*Create a white label, set its text and align it to the center*/
    uint8_t mac[8] = {0};
    char mac_string[32] = {0};

    esp_read_mac(&mac[0], ESP_IF_WIFI_STA);
    snprintf(&mac_string[0], 31, "%02X %02X %02X %02X %02X %02X ", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    g_lvgl_objects.label_mac = lv_label_create(lv_screen_active());
    lv_label_set_text(g_lvgl_objects.label_mac, (const char *)&mac_string);
    lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_lvgl_objects.label_mac, &lv_font_montserrat_12, 0);
    lv_obj_align(g_lvgl_objects.label_mac, LV_ALIGN_BOTTOM_MID, 0, 0);

    /*Create an Arc*/
    g_lvgl_objects.arc = lv_arc_create(lv_screen_active());

    if( s_globIsReceiver ) {
        lv_arc_set_mode(g_lvgl_objects.arc, LV_ARC_MODE_NORMAL);
        lv_arc_set_range(g_lvgl_objects.arc, 0, 50);
    }
    else {
        lv_arc_set_mode(g_lvgl_objects.arc, LV_ARC_MODE_SYMMETRICAL);
    }

    lv_obj_set_size(g_lvgl_objects.arc, 110, 110);
    lv_arc_set_rotation(g_lvgl_objects.arc, 135);
    lv_arc_set_bg_angles(g_lvgl_objects.arc, 0, 270);
    lv_obj_set_style_arc_color(g_lvgl_objects.arc,lv_palette_main(LV_PALETTE_INDIGO), LV_PART_INDICATOR);
    lv_arc_set_value(g_lvgl_objects.arc, 100);
    lv_obj_center(g_lvgl_objects.arc);

    /*Create a LED*/
    g_lvgl_objects.led = lv_led_create(lv_screen_active());
    lv_obj_align(g_lvgl_objects.led, LV_ALIGN_CENTER, 0, 0);
    lv_led_set_brightness(g_lvgl_objects.led, 150);
    lv_led_set_color(g_lvgl_objects.led, lv_palette_main(LV_PALETTE_INDIGO));
    lv_led_on(g_lvgl_objects.led);

    g_lvgl_timers.screen_1_label = lv_timer_create(lv_screen_timer_label, 500, g_lvgl_objects.label_mac);
    g_lvgl_timers.screen_1_led = lv_timer_create(lv_screen_timer_led, 1000, g_lvgl_objects.led);
    g_lvgl_timers.screen_1_arc = lv_timer_create(lv_screen_timer_arc, 100, g_lvgl_objects.arc);
}

void cleanup_lvgl_resources(void) {
    // Delete all timers
    if (g_lvgl_timers.screen_0_selection != NULL) {
        lv_timer_del(g_lvgl_timers.screen_0_selection);
        g_lvgl_timers.screen_0_selection = NULL;
    }
    if (g_lvgl_timers.screen_1_label != NULL) {
        lv_timer_del(g_lvgl_timers.screen_1_label);
        g_lvgl_timers.screen_1_label = NULL;
    }
    if (g_lvgl_timers.screen_1_led != NULL) {
        lv_timer_del(g_lvgl_timers.screen_1_led);
        g_lvgl_timers.screen_1_led = NULL;
    }
    if (g_lvgl_timers.screen_1_arc != NULL) {
        lv_timer_del(g_lvgl_timers.screen_1_arc);
        g_lvgl_timers.screen_1_arc = NULL;
    }

    // Delete all objects
    if (g_lvgl_objects.label_selection != NULL) {
        lv_obj_delete_async(g_lvgl_objects.label_selection);
        g_lvgl_objects.label_selection = NULL;
    }
    if (g_lvgl_objects.btn_set != NULL) {
        lv_obj_delete_async(g_lvgl_objects.btn_set);
        g_lvgl_objects.btn_set = NULL;
    }
    if (g_lvgl_objects.btn_enter != NULL) {
        lv_obj_delete_async(g_lvgl_objects.btn_enter);
        g_lvgl_objects.btn_enter = NULL;
    }
    if (g_lvgl_objects.label_mac != NULL) {
        lv_obj_delete_async(g_lvgl_objects.label_mac);
        g_lvgl_objects.label_mac = NULL;
    }
    if (g_lvgl_objects.arc != NULL) {
        lv_obj_delete_async(g_lvgl_objects.arc);
        g_lvgl_objects.arc = NULL;
    }
    if (g_lvgl_objects.led != NULL) {
        lv_obj_delete_async(g_lvgl_objects.led);
        g_lvgl_objects.led = NULL;
    }
}

void app_lvgl_display(void)
{
    bsp_display_lock(0);
    if( s_globSelectionDone ) {
        // delete screen0
        cleanup_lvgl_resources();
        lv_screen_1();    
    }
    else {
        lv_screen_0();
    }
    bsp_display_unlock();
}

// WiFi Initialization
static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init()); // Initialize TCP/IP stack (required for Wi-Fi)
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // Create default event loop
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // Initialize Wi-Fi driver
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM)); // Store Wi-Fi config in RAM
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // Station mode is required for ESP-NOW
    ESP_ERROR_CHECK(esp_wifi_start()); // Start Wi-Fi

    // Optional: Long Range Mode (can help, but also requires receiver support)
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));

    // Output own MAC address (helpful for the sender)
    // uint8_t self_mac[ESP_MAC_ADDR_LEN];
    // esp_wifi_get_mac(ESP_IF_WIFI_STA, self_mac);
    // ESP_LOGI(TAG, "Receiver MAC Address: " MACSTR, MAC2STR(self_mac));
}

void app_main(void)
{
    // Create mutex for thread safety
    g_lvgl_mutex = xSemaphoreCreateMutex();
    if (g_lvgl_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        abort();
    }

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

    // Set button callback
    GPIO_register_callback_button_set(&toggle_is_receiver);
    GPIO_register_callback_button_enter(&set_selection_done);

    // Start button monitoring task
    xTaskCreate(GPIO_button_monitoring_task, "button_task", 2048, NULL, 10, NULL);

    /* Configure Display  */
    if (bsp_display_start() == NULL) {
        ESP_LOGE(TAG, "display start failed!");
        abort();
    }
    app_lvgl_display();

    while(!s_globSelectionDone) {
        vTaskDelay(pdMS_TO_TICKS(500)); // Prevent app_main from ending
    }

    app_lvgl_display();

    if( s_globIsReceiver ) {
        RECEIVER_init();
        
        ESP_LOGI(TAG, "ESP-NOW Receiver Initialized. Waiting for data...");
        // Main task can simply wait here or perform other tasks
        // The actual work happens in the ESP-NOW Receive Callback
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(5000)); // Prevent app_main from ending
        }
    }
    else {
        SENDER_init();
    }
}
