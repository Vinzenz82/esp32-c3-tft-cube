#ifndef ESP_NOW_COMMON_H
#define ESP_NOW_COMMON_H

#include "esp_err.h"

/**
 * @brief Initialize WiFi for ESP-NOW functionality
 * 
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t esp_now_wifi_init(void);

#endif /* ESP_NOW_COMMON_H */ 