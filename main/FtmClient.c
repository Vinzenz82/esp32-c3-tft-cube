#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include "nvs_flash.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_console.h"
#include "esp_mac.h"
#include "FtmClient.h"

#define DEFAULT_WAIT_TIME_MS        (10 * 1000)
#define MAX_FTM_BURSTS              (8)

static const char *TAG = "FTM_CLIENT";

wifi_ftm_initiator_cfg_t ftmi_cfg = {
    .frm_count = 32,
    .burst_period = 2,
    .use_get_report_api = true,
};

const char *ftm_responder_ssid = "FTM";
uint16_t g_scan_ap_num;
wifi_ap_record_t *g_ap_list_buffer;

//forward declaration
wifi_ap_record_t *find_ftm_responder_ap(const char *ssid);
static bool wifi_perform_scan(const char *ssid, bool internal);
esp_err_t wifi_add_mode(wifi_mode_t mode);

void FTMCLIENT_init(void)
{
    wifi_ap_record_t *ap_record;

    ESP_LOGI(TAG, "Initializing FTM Client...");
    
    ESP_LOGI(TAG, "Get MAC of SSID...");

    ap_record = find_ftm_responder_ap(ftm_responder_ssid);
    if (ap_record) {
        memcpy(ftmi_cfg.resp_mac, ap_record->bssid, 6);
        ftmi_cfg.channel = ap_record->primary;
        ESP_LOGI(TAG, "FTM Client initialized successfully");
    } else {
        ESP_LOGE(TAG, "No AP found with SSID: %s", ftm_responder_ssid);
        return;
    }
} 

int FTMCLIENT_measure(void)
{
    ESP_LOGI(TAG, "Requesting FTM session with Frm Count - %d, Burst Period - %dmSec (0: No Preference)",
             ftmi_cfg.frm_count, ftmi_cfg.burst_period*100);

    if (ESP_OK != esp_wifi_ftm_initiate_session(&ftmi_cfg)) {
        ESP_LOGE(TAG, "Failed to start FTM session");
        return 0;
    }

    return 0;
}

wifi_ap_record_t *find_ftm_responder_ap(const char *ssid)
{
    bool retry_scan = false;
    uint8_t i;

    if (!ssid)
        return NULL;

retry:
    if (!g_ap_list_buffer || (g_scan_ap_num == 0)) {
        ESP_LOGI(TAG, "Scanning for %s", ssid);
        if (false == wifi_perform_scan(ssid, true)) {
            return NULL;
        }
    }

    for (i = 0; i < g_scan_ap_num; i++) {
        if (strcmp((const char *)g_ap_list_buffer[i].ssid, ssid) == 0)
            return &g_ap_list_buffer[i];
    }

    if (!retry_scan) {
        retry_scan = true;
        if (g_ap_list_buffer) {
            free(g_ap_list_buffer);
            g_ap_list_buffer = NULL;
        }
        goto retry;
    }

    ESP_LOGI(TAG, "No matching AP found");

    return NULL;
}

static bool wifi_perform_scan(const char *ssid, bool internal)
{
    wifi_scan_config_t scan_config = { 0 };
    scan_config.ssid = (uint8_t *) ssid;
    wifi_mode_t mode;
    uint8_t i;

    ESP_ERROR_CHECK( esp_wifi_get_mode(&mode) );
    if ((mode != WIFI_MODE_STA) && (mode != WIFI_MODE_APSTA)) {
        if (ESP_OK != wifi_add_mode(WIFI_MODE_STA)) {
            return false;
        }
    }
    if (ESP_OK != esp_wifi_scan_start(&scan_config, true)) {
        ESP_LOGI(TAG, "Failed to perform scan");
        return false;
    }

    esp_wifi_scan_get_ap_num(&g_scan_ap_num);
    if (g_scan_ap_num == 0) {
        ESP_LOGI(TAG, "No matching AP found");
        return false;
    }

    if (g_ap_list_buffer) {
        free(g_ap_list_buffer);
    }
    g_ap_list_buffer = malloc(g_scan_ap_num * sizeof(wifi_ap_record_t));
    if (g_ap_list_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to malloc buffer to print scan results");
        esp_wifi_clear_ap_list();
        return false;
    }

    if (esp_wifi_scan_get_ap_records(&g_scan_ap_num, (wifi_ap_record_t *)g_ap_list_buffer) == ESP_OK) {
        if (!internal) {
            for (i = 0; i < g_scan_ap_num; i++) {
                ESP_LOGI(TAG, "[%s][rssi=%d]""%s", g_ap_list_buffer[i].ssid, g_ap_list_buffer[i].rssi,
                         g_ap_list_buffer[i].ftm_responder ? "[FTM Responder]" : "");
            }
        }
    }

    ESP_LOGI(TAG, "sta scan done");

    return true;
}

esp_err_t wifi_add_mode(wifi_mode_t mode)
{
    wifi_mode_t cur_mode, new_mode = mode;

    if (esp_wifi_get_mode(&cur_mode)) {
        ESP_LOGE(TAG, "Failed to get mode!");
        return ESP_FAIL;
    }

    if (mode == WIFI_MODE_STA) {
        if (cur_mode == WIFI_MODE_STA || cur_mode == WIFI_MODE_APSTA) {
             return ESP_OK;
        } else if (cur_mode == WIFI_MODE_AP) {
            new_mode = WIFI_MODE_APSTA;
        } else {
            new_mode = WIFI_MODE_STA;
        }
    } else if (mode == WIFI_MODE_AP) {
        if (cur_mode == WIFI_MODE_AP || cur_mode == WIFI_MODE_APSTA) {
            /* Do nothing */
            return ESP_OK;
        } else if (cur_mode == WIFI_MODE_STA) {
            new_mode = WIFI_MODE_APSTA;
        } else {
            new_mode = WIFI_MODE_AP;
        }
    }

    ESP_ERROR_CHECK( esp_wifi_set_mode(new_mode) );
    return ESP_OK;
}