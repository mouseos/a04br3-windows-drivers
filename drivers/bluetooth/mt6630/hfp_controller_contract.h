/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BT_HFP_CONTROLLER_CONTRACT_H
#define A04_BT_HFP_CONTROLLER_CONTRACT_H
#include <ntddk.h>
/* Proposed version 2 of the paired Raman interface, SAME v1 layout.
 * V2 registers real transport ownership before optional audio qualification.
 * MatchHfpTarget is PASSIVE (standard PnP I/O); Live/ref/deref <=DISPATCH.
 * Null ValidatePcm/CodecLatency means qualification is unavailable: consumers
 * MUST NOT publish factories or advertise HCIBypass. No success/refusal stubs.
 * V1 parents are never asked to accept a V2 descriptor. */
static const GUID A04BtSharedHfpGuid={0x84e4cb62,0xe9b8,0x47cc,{0x9f,0x62,0xa7,0xc3,0xe4,0xd5,0x82,0x40}};
#define A04_BT_CONTROLLER_VERSION 2u
struct a04_bt_hfp_controller {
 ULONG size,version;PVOID context;PDEVICE_OBJECT controller;
 void (*Reference)(PVOID);void (*Dereference)(PVOID);
 NTSTATUS (*Acquire)(PVOID,PULONG);BOOLEAN (*Live)(PVOID,ULONG);NTSTATUS (*Release)(PVOID,ULONG);
 NTSTATUS (*MatchHfpTarget)(PVOID,PDEVICE_OBJECT);
 NTSTATUS (*ValidatePcm)(PVOID,UCHAR,ULONG,ULONG,ULONG);
 NTSTATUS (*CodecLatency)(PVOID,PULONG);
};
struct a04_bt_hfp_parent {
 INTERFACE Header;
 NTSTATUS (*RegisterController)(PVOID,const struct a04_bt_hfp_controller *);
 NTSTATUS (*UnregisterController)(PVOID,PVOID);
 NTSTATUS (*GetController)(PVOID,struct a04_bt_hfp_controller *);
 NTSTATUS (*Acquire)(PVOID,PULONG);BOOLEAN (*Live)(PVOID,ULONG);
 NTSTATUS (*Pins)(PVOID,ULONG,BOOLEAN);NTSTATUS (*Release)(PVOID,ULONG);
 NTSTATUS (*VerifyDmaQuiescent)(PVOID,ULONG,ULONG,PMDL);
};
#endif
