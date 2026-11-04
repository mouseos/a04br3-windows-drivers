/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_WIFI_INTERFACE_H
#define A04_SHARED_WIFI_INTERFACE_H
#include <ntddk.h>
/* Offline prototype. A real provider must guarantee the exclusive host,
 * shared GPIO/clock/reset, combo/firmware and IRQ lease until Release, and
 * serialize power transitions and PnP teardown. A referenced interface is
 * not itself a powered device. No provider currently grants this lease. */
static const GUID A04SharedWifiInterfaceGuid =
 {0x6210787e,0x11db,0x4a59,{0xb2,0x81,0x27,0x3b,0x59,0xf1,0x2e,0x33}};
#define A04_SHARED_WIFI_INTERFACE_VERSION 1
typedef struct _A04_SHARED_WIFI_INTERFACE {
 INTERFACE Header;
 NTSTATUS (*Acquire)(PVOID context, ULONG *generation);
 NTSTATUS (*ReleaseQuiescent)(PVOID context, ULONG generation);
} A04_SHARED_WIFI_INTERFACE;
#endif
