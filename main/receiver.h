#pragma once 

#include <stdio.h>

extern int64_t s_last_time_recv_cb_us;

extern void RECEIVER_init(void);
extern float RECEIVER_getDistance(void);
extern int16_t RECEIVER_getRSSI(void);
extern float RECEIVER_getRssiAt1Meter(void);
extern void RECEIVER_setRssiAt1Meter(void);