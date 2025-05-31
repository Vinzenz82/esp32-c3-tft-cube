#pragma once 

#include "esp_timer.h" 

extern void COMMON_callback_called(void);
extern int64_t COMMON_get_time_of_last_callback(void);
