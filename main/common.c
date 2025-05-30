#include "common.h" 

static int64_t s_last_time_cb_us = 0; 

void COMMON_callback_called(void) {
    s_last_time_cb_us = esp_timer_get_time();
}

int64_t COMMON_get_time_of_last_callback(void) {
    return s_last_time_cb_us;
}