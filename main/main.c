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
#include "EspNowSender.h"
#include "EspNowReceiver.h"
#include "FtmResponder.h"
#include "EspNowCommon.h"
#include "FtmCommon.h"
#include "FtmClient.h"

static const char *TAG = "main";

typedef enum {
    EspNowSender,
    EspNowReceiver,
    FtmClient,
    FtmResponder
} DeviceMode_t;

/* Global structure to hold all LVGL objects */
typedef struct {
    /* Screen 0 objects */
    lv_obj_t *label_selection;
    lv_obj_t *btn_set;
    lv_obj_t *label_set;
    lv_obj_t *btn_enter;
    lv_obj_t *label_enter;
    
    /* Screen 1 objects */
    lv_obj_t *label_value;
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
    lv_timer_t *screen_1_calib;
} lvgl_timers_t;

static lvgl_objects_t g_lvgl_objects = {0};
static lvgl_timers_t g_lvgl_timers = {0};

/* Mutex for thread safety */
static SemaphoreHandle_t g_lvgl_mutex = NULL;

static bool s_globSelectionDone = false;
static DeviceMode_t s_globDeviceMode = EspNowReceiver;
static uint8_t s_globCalibStep = 0u;

void button_pressed_set(void) {
    if (xSemaphoreTake(g_lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        if( s_globSelectionDone == false ) {
            // Cycle through modes
            switch(s_globDeviceMode) {
                case EspNowReceiver:
                    s_globDeviceMode = EspNowSender;
                    break;
                case EspNowSender:
                    s_globDeviceMode = FtmClient;
                    break;
                case FtmClient:
                    s_globDeviceMode = FtmResponder;
                    break;
                case FtmResponder:
                    s_globDeviceMode = EspNowReceiver;
                    break;
            }
        }
        else {
            if( s_globCalibStep == 0 ) {
                if(lv_timer_get_paused(g_lvgl_timers.screen_1_calib)) {
                    lv_timer_resume(g_lvgl_timers.screen_1_calib);
                }
            }
            s_globCalibStep ^= 1;
        }
        xSemaphoreGive(g_lvgl_mutex);
    }
}

void button_pressed_enter(void) {
    if (xSemaphoreTake(g_lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        s_globSelectionDone = true;

        if( s_globCalibStep == 1 ) {
            RECEIVER_setRssiAt1Meter();
            s_globCalibStep = 0;
        }

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
        if( s_globDeviceMode == EspNowReceiver ) {
            if( s_globCalibStep == 0 ) {
                lv_obj_remove_flag(user, LV_OBJ_FLAG_HIDDEN);
                lv_arc_set_value(user, (int32_t)RECEIVER_getDistance());
            }
            else {
                lv_obj_add_flag(user, LV_OBJ_FLAG_HIDDEN);
            }
        }
        else if( s_globDeviceMode == FtmClient ) {
            lv_obj_remove_flag(user, LV_OBJ_FLAG_HIDDEN);
            lv_arc_set_value(user, (int32_t)FTMCOMMON_getDistance());
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

    if( s_globCalibStep == 0 ) {
        lv_obj_remove_flag(user, LV_OBJ_FLAG_HIDDEN);

        lv_led_toggle(user);

        int64_t time_ms = (esp_timer_get_time() - COMMON_get_time_of_last_callback()) / 1000;

        if( time_ms < 2000 ) {
            lv_led_set_color(user, lv_color_hex(0x0000FF));
        } else if ( time_ms < 4000 ) {
            lv_led_set_color(user, lv_palette_main(LV_PALETTE_YELLOW));
        } else if ( time_ms < 8000 ) {
            lv_led_set_color(user, lv_palette_main(LV_PALETTE_ORANGE));
        } else {
            lv_led_set_color(user, lv_palette_main(LV_PALETTE_RED));
        }
    }
    else {
        lv_obj_add_flag(user, LV_OBJ_FLAG_HIDDEN);
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
                if( s_globDeviceMode == EspNowReceiver ) {
                    const float distance = RECEIVER_getDistance();
                    const int16_t rssi = RECEIVER_getRSSI();

                    if( s_globCalibStep == 0 ) {
                        char distance_str[32];
                        snprintf(distance_str, sizeof(distance_str), "< %.0fm (RSSI: %d)", ceilf(distance), rssi);
                        lv_obj_set_style_text_font(user, &lv_font_montserrat_12, 0);
                        lv_obj_align(user, LV_ALIGN_BOTTOM_MID, 0, 0);
                        lv_label_set_text(user, distance_str);
                    }
                    else {
                        char distance_str[64];
                        snprintf(distance_str, sizeof(distance_str), "Press Apply at exactly 1m. (RSSI: %d)", rssi);
                        lv_obj_set_style_text_font(user, &lv_font_montserrat_14, 0);
                        lv_obj_align(user, LV_ALIGN_CENTER, 0, 0);
                        lv_label_set_text(user, distance_str);
                    }
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
        else if( s_globDeviceMode == FtmClient ) {
            const float distance = FTMCOMMON_getDistance();

            char distance_str[32];
            snprintf(distance_str, sizeof(distance_str), "%.0fm", ceilf(distance));
            lv_obj_set_style_text_font(user, &lv_font_montserrat_12, 0);
            lv_obj_align(user, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_label_set_text(user, distance_str);                    
        }
        xSemaphoreGive(g_lvgl_mutex);
    }
}

void lv_screen_timer_calib(lv_timer_t* timer)
{
        /*Use the user_data*/
    lvgl_objects_t* user = lv_timer_get_user_data(timer);
    
    if (user == NULL) {
        return;
    }

    if(s_globCalibStep == 0) {
        lv_timer_pause(g_lvgl_timers.screen_1_calib);

        if (user->btn_set != NULL) {
            lv_obj_set_size(user->btn_set, 5, 20);
        }
        if (user->label_set != NULL) {
            lv_label_set_text(user->label_set, "");
        }
        if (user->label_enter != NULL) {
            lv_obj_add_flag(user->btn_enter, LV_OBJ_FLAG_HIDDEN);
        }

    }
    else {
        if (user->btn_set != NULL) {
            lv_obj_set_size(user->btn_set, 45, 20);
        }
        if (user->label_set != NULL) {
            lv_label_set_text(user->label_set, "Abort");
        }
        if (user->label_enter != NULL) {
            lv_obj_remove_flag(user->btn_enter, LV_OBJ_FLAG_HIDDEN);
        }
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
    
    switch(s_globDeviceMode) {
        case EspNowReceiver:
            lv_label_set_text(user, "ESP-NOW Receiver");
            break;
        case EspNowSender:
            lv_label_set_text(user, "ESP-NOW Sender");
            break;
        case FtmClient:
            lv_label_set_text(user, "FTM Client");
            break;
        case FtmResponder:
            lv_label_set_text(user, "FTM Responder");
            break;
    }
}

void lv_screen_0(void)
{
    char label_string[32] = {0};

    switch(s_globDeviceMode) {
        case EspNowReceiver:
            snprintf(label_string, sizeof(label_string), "ESP-NOW Receiver");
            break;
        case EspNowSender:
            snprintf(label_string, sizeof(label_string), "ESP-NOW Sender");
            break;
        case FtmClient:
            snprintf(label_string, sizeof(label_string), "FTM Client");
            break;
        case FtmResponder:
            snprintf(label_string, sizeof(label_string), "FTM Responder");
            break;
    }

    g_lvgl_objects.label_selection = lv_label_create(lv_screen_active());
    lv_label_set_text(g_lvgl_objects.label_selection, (const char *)&label_string);
    lv_obj_set_style_text_font(g_lvgl_objects.label_selection, &lv_font_montserrat_14, 0);
    lv_obj_align(g_lvgl_objects.label_selection, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(g_lvgl_objects.label_selection, LV_LABEL_LONG_WRAP);     /*Break the long lines*/
    lv_obj_set_style_text_align(g_lvgl_objects.label_selection, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(g_lvgl_objects.label_selection, 120);  /*Set smaller width to make the lines wrap*/

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
    /*Change the active screen's background color to white*/
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    /*Create a white label, set its text and align it to the center*/
    uint8_t mac[8] = {0};
    char mac_string[32] = {0};

    esp_read_mac(&mac[0], ESP_IF_WIFI_STA);
    snprintf(&mac_string[0], 31, "%02X %02X %02X %02X %02X %02X ", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    g_lvgl_objects.label_value = lv_label_create(lv_screen_active());
    lv_label_set_text(g_lvgl_objects.label_value, (const char *)&mac_string);
    lv_obj_set_style_text_color(g_lvgl_objects.label_value, lv_color_hex(0x000000), LV_PART_MAIN);  /* Black text for contrast */
    lv_obj_set_style_text_font(g_lvgl_objects.label_value, &lv_font_montserrat_12, 0);
    lv_obj_align(g_lvgl_objects.label_value, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_long_mode(g_lvgl_objects.label_value, LV_LABEL_LONG_WRAP);     /*Break the long lines*/
    lv_obj_set_style_text_align(g_lvgl_objects.label_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(g_lvgl_objects.label_value, 120);  /*Set smaller width to make the lines wrap*/

    /*Create an Arc*/
    g_lvgl_objects.arc = lv_arc_create(lv_screen_active());

    if(     ( s_globDeviceMode == EspNowReceiver ) 
        ||  ( s_globDeviceMode == FtmClient ) ) {
        lv_arc_set_mode(g_lvgl_objects.arc, LV_ARC_MODE_NORMAL);
        lv_arc_set_range(g_lvgl_objects.arc, 0, 50);
    }
    else {
        lv_arc_set_mode(g_lvgl_objects.arc, LV_ARC_MODE_SYMMETRICAL);
    }

    lv_obj_set_size(g_lvgl_objects.arc, 110, 110);
    lv_arc_set_rotation(g_lvgl_objects.arc, 135);
    lv_arc_set_bg_angles(g_lvgl_objects.arc, 0, 270);
    lv_obj_set_style_arc_color(g_lvgl_objects.arc, lv_color_hex(0xE0E0E0), LV_PART_MAIN);  /* Light gray background */
    lv_obj_set_style_arc_color(g_lvgl_objects.arc, lv_color_hex(0x0000FF), LV_PART_INDICATOR);  /* Blue indicator for contrast */
    lv_arc_set_value(g_lvgl_objects.arc, 100);
    lv_obj_center(g_lvgl_objects.arc);

    /*Create a LED*/
    g_lvgl_objects.led = lv_led_create(lv_screen_active());
    lv_obj_align(g_lvgl_objects.led, LV_ALIGN_CENTER, 0, 0);
    lv_led_set_brightness(g_lvgl_objects.led, 200);  /* Increased brightness */
    lv_led_set_color(g_lvgl_objects.led, lv_color_hex(0x0000FF));  /* Blue LED for contrast */
    lv_led_on(g_lvgl_objects.led);

    /*Create Set button*/
    g_lvgl_objects.btn_set = lv_btn_create(lv_screen_active());
    lv_obj_set_size(g_lvgl_objects.btn_set, 5, 20);
    lv_obj_align(g_lvgl_objects.btn_set, LV_ALIGN_TOP_LEFT, 2, 10);
    lv_obj_set_style_bg_color(g_lvgl_objects.btn_set, lv_color_hex(0x0000FF), LV_PART_MAIN);  /* Blue button */
    g_lvgl_objects.label_set = lv_label_create(g_lvgl_objects.btn_set);
    lv_obj_set_style_text_font(g_lvgl_objects.label_set, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_lvgl_objects.label_set, lv_color_hex(0xFFFFFF), LV_PART_MAIN);  /* White text */
    lv_label_set_text(g_lvgl_objects.label_set, "");
    lv_obj_center(g_lvgl_objects.label_set);

    /*Create Enter button*/
    g_lvgl_objects.btn_enter = lv_btn_create(lv_screen_active());
    lv_obj_set_size(g_lvgl_objects.btn_enter, 45, 20);
    lv_obj_align(g_lvgl_objects.btn_enter, LV_ALIGN_BOTTOM_LEFT, 2, -10);
    lv_obj_set_style_bg_color(g_lvgl_objects.btn_enter, lv_color_hex(0x0000FF), LV_PART_MAIN);  /* Blue button */
    g_lvgl_objects.label_enter = lv_label_create(g_lvgl_objects.btn_enter);
    lv_obj_set_style_text_font(g_lvgl_objects.label_enter, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_lvgl_objects.label_enter, lv_color_hex(0xFFFFFF), LV_PART_MAIN);  /* White text */
    lv_label_set_text(g_lvgl_objects.label_enter, "Apply");
    lv_obj_center(g_lvgl_objects.label_enter);
    lv_obj_add_flag(g_lvgl_objects.btn_enter, LV_OBJ_FLAG_HIDDEN);

    g_lvgl_timers.screen_1_label = lv_timer_create(lv_screen_timer_label, 500, g_lvgl_objects.label_value);
    g_lvgl_timers.screen_1_led = lv_timer_create(lv_screen_timer_led, 1000, g_lvgl_objects.led);
    g_lvgl_timers.screen_1_arc = lv_timer_create(lv_screen_timer_arc, 100, g_lvgl_objects.arc);

    g_lvgl_timers.screen_1_calib = lv_timer_create(lv_screen_timer_calib, 100, &g_lvgl_objects);
    lv_timer_pause(g_lvgl_timers.screen_1_calib);
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
    if (g_lvgl_objects.label_value != NULL) {
        lv_obj_delete_async(g_lvgl_objects.label_value);
        g_lvgl_objects.label_value = NULL;
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

    // Set button callback
    GPIO_register_callback_button_set(&button_pressed_set);
    GPIO_register_callback_button_enter(&button_pressed_enter);

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

    if( s_globDeviceMode == EspNowReceiver ) {
        // Initialize WiFi
        ESP_ERROR_CHECK(esp_now_wifi_init());

        RECEIVER_init();
        
        ESP_LOGI(TAG, "ESP-NOW Receiver Initialized. Waiting for data...");
        // Main task can simply wait here or perform other tasks
        // The actual work happens in the ESP-NOW Receive Callback
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(5000)); // Prevent app_main from ending
        }
    }
    else if( s_globDeviceMode == EspNowSender ) {
        // Initialize WiFi
        ESP_ERROR_CHECK(esp_now_wifi_init());

        SENDER_init();
    }
    else if( s_globDeviceMode == FtmResponder) {
        // Initialize WiFi
        ESP_ERROR_CHECK(ftm_wifi_init());

        FTMRESPONDER_init();

        // Main task can simply wait here or perform other tasks
        // The actual work happens in the ESP-NOW Receive Callback
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(5000)); // Prevent app_main from ending
        }
    }
    else if( s_globDeviceMode == FtmClient) {
        // Initialize WiFi
        ESP_ERROR_CHECK(ftm_wifi_init());

        FTMCLIENT_init();

        // Main task can simply wait here or perform other tasks
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(3000)); // Prevent app_main from ending
            FTMCLIENT_measure();
        }
    }
    else {
        ESP_LOGI(TAG, "Mode not implemented yet");
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(5000)); // Prevent app_main from ending
        }
    }
}
