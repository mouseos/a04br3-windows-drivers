/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT6392_RTC_CALENDAR_H
#define MT6392_RTC_CALENDAR_H
#include <stdint.h>
#define MT6392_RTC_IO_ERROR (-1)
#define MT6392_RTC_INVALID (-2)
#define MT6392_RTC_UNSTABLE (-3)
struct mt6392_rtc_raw { uint16_t sec, min, hour, day, dow, month, year; };
/* Fixed-width result: no timezone or oscillator-validity claim. */
struct mt6392_rtc_calendar {
    uint16_t year, month, day, hour, min, sec, weekday;
};
int mt6392_rtc_decode(const struct mt6392_rtc_raw *, struct mt6392_rtc_calendar *);
#endif
