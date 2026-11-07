/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT6392_RTC_IOCTL_H
#define MT6392_RTC_IOCTL_H
#include "calendar.h"
/* Include Windows kernel/user IOCTL definitions before this header. */
#define IOCTL_MT6392_RTC_READ CTL_CODE(FILE_DEVICE_UNKNOWN,0x800,METHOD_BUFFERED,FILE_READ_ACCESS)
#define MT6392_RTC_SAMPLE_VERSION 1
struct mt6392_rtc_sample {
 uint32_t size, version;
 struct mt6392_rtc_raw raw;
 struct mt6392_rtc_calendar calendar;
};
#endif
