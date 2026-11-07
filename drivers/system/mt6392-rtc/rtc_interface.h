/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT6392_RTC_INTERFACE_H
#define MT6392_RTC_INTERFACE_H
#include <ntddk.h>
#include "calendar.h"
static const GUID Mt6392RtcInterfaceGuid =
 {0xebd273c9,0x03bf,0x49e4,{0x99,0x6b,0x26,0x8c,0x81,0x5a,0x87,0x14}};
#define MT6392_RTC_INTERFACE_VERSION 1
/* PASSIVE_LEVEL only. References retain parent memory, not power. Provider
 * owns admission/D0/removal serialization and never writes RTC registers. */
typedef struct {
 INTERFACE Header;
 NTSTATUS (*ReadCalendar)(PVOID,struct mt6392_rtc_raw *);
} MT6392_RTC_INTERFACE;
#endif
