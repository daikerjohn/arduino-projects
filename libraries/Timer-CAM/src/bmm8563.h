#pragma once
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _rtc_data_t {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_date_t;

// Clear irq, Init
void bmm8563_init();

void bmm8563_setTime(rtc_date_t* data);

void bmm8563_getTime(rtc_date_t* data);

// -1: disable
void bmm8563_setDateIRQ(int8_t minute, int8_t hour, int8_t day, int8_t week);

// sec, max time is 255 * 60
int16_t bmm8563_setTimerIRQ(int16_t value);

// sec, get timer reg value
int16_t bmm8563_getTimerTime();

// get irq status
uint8_t bmm8563_getIRQ();

// clear irq status
void bmm8563_clearIRQ();

#ifdef __cplusplus
}
#endif