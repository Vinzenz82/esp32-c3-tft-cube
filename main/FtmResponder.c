#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"

#include "FtmResponder.h"

typedef struct {
    char *ssid;
    char *password;
} wifi_args_t;

static bool s_reconnect = true;
static const char *TAG_AP = "ftm_ap";
static wifi_args_t ap_args = {
    .ssid = "FTM",
    .password = "12345678"
};

wifi_config_t g_ap_config = {
    .ap.max_connection = 4,
    .ap.authmode = WIFI_AUTH_WPA2_PSK,
    .ap.ftm_responder = true
};

static bool wifi_cmd_ap_set(const char* ssid, const char* pass)
{
    s_reconnect = false;
    strlcpy((char*) g_ap_config.ap.ssid, ssid, MAX_SSID_LEN);
    if (pass) {
        if (strlen(pass) != 0 && strlen(pass) < 8) {
            s_reconnect = true;
            ESP_LOGE(TAG_AP, "password cannot be less than 8 characters long");
            return false;
        }
        strlcpy((char*) g_ap_config.ap.password, pass, MAX_PASSPHRASE_LEN);
    }

    if (strlen(pass) == 0) {
        g_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &g_ap_config));
    return true;
}

void FTMRESPONDER_init(void) 
{
    if (true == wifi_cmd_ap_set(ap_args.ssid, ap_args.password)) {
        ESP_LOGI(TAG_AP, "Starting SoftAP with FTM Responder support, SSID - %s, Password - %s", ap_args.ssid, ap_args.password);
    }
    else {
        ESP_LOGE(TAG_AP, "Failed to start SoftAP!");
    }
}