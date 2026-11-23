#ifndef RTC_UNIT_H
#define RTC_UNIT_H

#include <stdint.h>

typedef struct RTC_UNIT {
    uint8_t year;
    uint8_t month;
    uint8_t date;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} RTC_UNIT;

#define RTC_UNIT_DEFINED

#endif // RTC_UNIT_H
