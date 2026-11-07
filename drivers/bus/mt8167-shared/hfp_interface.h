/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_HFP_INTERFACE_H
#define A04_SHARED_HFP_INTERFACE_H
#include <ntddk.h>
/* Separate from Audio v11 and WiFi v2. Kernel-only, paired BT profile.
 * BT owns controller provenance/FC72/GORM; Audio owns AFE/DMA/IRQ.
 * Reference/dereference/live at <=DISPATCH; MatchHfpTarget is PASSIVE;
 * other methods PASSIVE. Never expose a callback/path/command user broker. */
static const GUID A04SharedHfpInterfaceGuid={0x84e4cb62,0xe9b8,0x47cc,{0x9f,0x62,0xa7,0xc3,0xe4,0xd5,0x82,0x40}};
#define A04_SHARED_HFP_VERSION 3
#define A04_HFP_CODEC_LATENCY_VALID 1u
struct A04_HFP_CODEC_LATENCY {ULONG valid,codec_100ns;};
struct A04_HFP_CONTROLLER {
 ULONG size,version;PVOID context;PDEVICE_OBJECT controller;
 void (*Reference)(PVOID);void (*Dereference)(PVOID);
 NTSTATUS (*Acquire)(PVOID,PULONG);
 BOOLEAN (*Live)(PVOID,ULONG);
 NTSTATUS (*Release)(PVOID,ULONG);
 NTSTATUS (*MatchHfpTarget)(PVOID,PDEVICE_OBJECT);
 NTSTATUS (*ValidatePcm)(PVOID,UCHAR,ULONG,ULONG,ULONG);
 NTSTATUS (*CodecLatency)(PVOID,ULONG direction,struct A04_HFP_CODEC_LATENCY *); /* actual codec delay, 100ns */
};
/* KSRTAUDIO_HWLATENCY: FIFO bytes, chipset 100ns delay. Direction is
 * speaker=0/mic=1. Presence bits distinguish a real zero from no provider.
 * No implementation/constant is inferred from register names or HCI slots. */
#define A04_HFP_LATENCY_FIFO_VALID 1u
#define A04_HFP_LATENCY_CHIPSET_VALID 2u
struct A04_HFP_DMA_LATENCY {ULONG valid,fifo_bytes,chipset_100ns;};
struct A04_SHARED_HFP_INTERFACE {
 INTERFACE Header;
 /* BT registers its real combo owner after pad ACK/GORM/FC72 completion.
  * Registration alone does not establish PCM format or HCIBypass support.
  * V3 permits NULL ValidatePcm/CodecLatency: explicit absent qualification.
  * GetController returns a Reference owned by the caller. */
 NTSTATUS (*RegisterController)(PVOID,const struct A04_HFP_CONTROLLER *);
 NTSTATUS (*UnregisterController)(PVOID,PVOID controllerContext);
 NTSTATUS (*GetController)(PVOID,struct A04_HFP_CONTROLLER *);
 /* Original WiFi v2 Live is PASSIVE-only. This optional parent-owned epoch
  * reader is nonblocking at <=DISPATCH and never touches MMIO. */
 int (*ControllerEpochLive)(PVOID,ULONG);
 NTSTATUS (*Acquire)(PVOID,PULONG);BOOLEAN (*Live)(PVOID,ULONG);
 NTSTATUS (*Pins)(PVOID,ULONG,BOOLEAN);NTSTATUS (*Release)(PVOID,ULONG);
 /* Independent platform proof for exposed AFE memory transactions, not a
  * disable write, poll delay, IRQ flush, caller Boolean or BT ACK. Optional:
  * absent => no HFP endpoint factory/admission, never a success stub. */
 NTSTATUS (*VerifyDmaQuiescent)(PVOID,ULONG,ULONG direction,PMDL);
 /* PASSIVE, read-only qualification for exact format, before any stream is
  * exposed. Missing callback or incomplete valid mask denies publication. */
 NTSTATUS (*QueryDmaLatency)(PVOID,ULONG direction,ULONG rate,ULONG channels,ULONG bits,struct A04_HFP_DMA_LATENCY *);
};
#endif
