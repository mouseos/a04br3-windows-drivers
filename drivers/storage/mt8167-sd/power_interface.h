/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SD_POWER_INTERFACE_H
#define A04_SD_POWER_INTERFACE_H
#include <ntddk.h>
/* Same GUID is a fixed provider device-interface class and query-interface.
 * No user IOCTL, arbitrary register, voltage, firmware or path broker. */
static const GUID A04SdPowerInterfaceGuid=
 {0xf318c6da,0xd307,0x41ae,{0x89,0x16,0xa0,0x44,0x13,0x67,0x00,0x01}};
#define A04_SD_POWER_INTERFACE_VERSION 1
struct a04_sd_power_interface {
 INTERFACE Header;
 /* A nonzero token on failure means quarantine: power must stay retained. */
 NTSTATUS (*Acquire)(PVOID,ULONG*);
 /* Acquire is a no-mutation claim. Consumer inspects its private MSDC DMA
  * before Enable supplies power; gated cold handoff is unsupported. */
 NTSTATUS (*Enable)(PVOID,ULONG);
 NTSTATUS (*Health)(PVOID,ULONG);
 /* Consumer must join CPU users and prove private PIO host reset completed.
  * Quiesce failure must not call ReleaseQuiescent; no partial release success. */
 NTSTATUS (*ReleaseQuiescent)(PVOID,ULONG);
};
#endif
