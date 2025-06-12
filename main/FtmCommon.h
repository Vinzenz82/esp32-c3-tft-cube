#ifndef FTM_COMMON_H
#define FTM_COMMON_H

#include "esp_err.h"

/**
 * @brief Initialize WiFi for FTM functionality
 * 
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
extern esp_err_t ftm_wifi_init(void);
extern float FTMCOMMON_getDistance(void);

#endif /* FTM_COMMON_H */ 