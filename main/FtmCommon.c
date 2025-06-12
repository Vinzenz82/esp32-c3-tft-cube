#include "FtmCommon.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

static const char *TAG = "FtmCommon";

static EventGroupHandle_t s_wifi_event_group;
static EventGroupHandle_t s_ftm_event_group;
static bool s_ap_started;
static uint8_t s_ftm_report_num_entries;
static uint32_t s_rtt_est, s_dist_est;

float FTMCOMMON_getDistance(void)
{
    return s_dist_est / 100;
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
	if (event_id == WIFI_EVENT_STA_CONNECTED) {

    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {

    } else if (event_id == WIFI_EVENT_FTM_REPORT) {
        wifi_event_ftm_report_t *event = (wifi_event_ftm_report_t *) event_data;

        s_rtt_est = event->rtt_est;
        s_dist_est = event->dist_est;
        s_ftm_report_num_entries = event->ftm_report_num_entries;

        ESP_LOGI(TAG, "Estimated RTT - %" PRId32 " nSec, Estimated Distance - %" PRId32 ".%02" PRId32 " meters",
                          s_rtt_est, s_dist_est / 100, s_dist_est % 100);

    } else if (event_id == WIFI_EVENT_AP_START) {
        s_ap_started = true;
    } else if (event_id == WIFI_EVENT_AP_STOP) {
        s_ap_started = false;
    }
}

esp_err_t ftm_wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init()); // Initialize TCP/IP stack (required for Wi-Fi)
    
    s_wifi_event_group = xEventGroupCreate();
    s_ftm_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // Create default event loop
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // Initialize Wi-Fi driver

    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM)); // Store Wi-Fi config in RAM
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP)); // AP mode is required for FTM Responder
    ESP_ERROR_CHECK(esp_wifi_start()); // Start Wi-Fi

    return ESP_OK;
} 