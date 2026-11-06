/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_WIFI_INTERFACE_H
#define A04_SHARED_WIFI_INTERFACE_H
#include <ntddk.h>
/* Offline prototype. A real provider must guarantee the exclusive host,
 * shared GPIO/clock/reset, combo/firmware and IRQ lease until Release, and
 * serialize power transitions and PnP teardown. A referenced interface is
 * not itself a powered device. The explicit runtime parent grants a whole-combo cold-start lease.
 * Bluetooth/FM coexistence and live firmware handoff are not supported. */
static const GUID A04SharedWifiInterfaceGuid =
 {0x6210787e,0x11db,0x4a59,{0xb2,0x81,0x27,0x3b,0x59,0xf1,0x2e,0x33}};
#define A04_SHARED_WIFI_INTERFACE_VERSION 2
typedef struct _A04_SHARED_WIFI_INTERFACE {
 INTERFACE Header;
 /* Failure must be side-effect-free: no retained lease, power changes or
  * outstanding work. Partial acquisition must never be reported as failure;
  * a provider unable to ensure this contract must refuse before touching HW. */
 NTSTATUS (*Acquire)(PVOID context, ULONG *generation);
 NTSTATUS (*ReleaseQuiescent)(PVOID context, ULONG generation);
 /* Banks 0=TOPCKGEN, 1=GPIO. Each operation is serialized with touch. */
 int (*Live)(PVOID context, ULONG generation);
 int (*UpdateShared)(PVOID context, ULONG generation, unsigned bank, unsigned offset, ULONG clear, ULONG set);
 int (*WriteShared)(PVOID context, ULONG generation, unsigned bank, unsigned offset, ULONG value);
} A04_SHARED_WIFI_INTERFACE;
#endif
