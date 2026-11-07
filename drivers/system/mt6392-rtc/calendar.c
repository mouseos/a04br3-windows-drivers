/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "calendar.h"
static unsigned leap(unsigned y) { return y%4==0 && (y%100!=0 || y%400==0); }
int mt6392_rtc_decode(const struct mt6392_rtc_raw *r, struct mt6392_rtc_calendar *out)
{
    static const unsigned days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    struct mt6392_rtc_calendar v; unsigned y,m,n=0,limit;
    if(!r || !out || r->year>127 || r->month<1 || r->month>12 ||
       r->hour>23 || r->min>59 || r->sec>59)return MT6392_RTC_INVALID;
    y=1968u+r->year;limit=days[r->month-1]+(r->month==2?leap(y):0);
    if(r->day<1 || r->day>limit)return MT6392_RTC_INVALID;
    /* 1968-01-01 was Monday. Hardware DOW is retained raw, never trusted. */
    for(m=1968;m<y;m++)n+=365+leap(m);
    for(m=1;m<r->month;m++)n+=days[m-1]+(m==2?leap(y):0);
    v.year=(uint16_t)y;v.month=r->month;v.day=r->day;v.hour=r->hour;
    v.min=r->min;v.sec=r->sec;v.weekday=(uint16_t)((n+r->day)%7);
    *out=v;return 0;
}
