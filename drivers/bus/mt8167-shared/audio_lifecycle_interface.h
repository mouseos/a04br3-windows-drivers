/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_AUDIO_LIFECYCLE_INTERFACE_H
#define A04_AUDIO_LIFECYCLE_INTERFACE_H
#include <ntddk.h>
/* New interface: never reinterpret the legacy v11 VerifyDmaDrained result.
 * The contract is CT3 STOP -> hw_free -> shutdown, not a bus-idle monitor.
 * All calls PASSIVE_LEVEL, serialized by the single child owner. */
static const GUID A04AudioLifecycleGuid=
 {0xb514e251,0x6283,0x41eb,{0xb6,0x13,0x86,0x77,0x30,0xf1,0x12,0x22}};
#define A04_AUDIO_LIFECYCLE_VERSION 1
#define A04_AUDIO_STOP_SOFTWARE_QUIESCENT 1u
struct A04_AUDIO_LIFECYCLE_INTERFACE {
 INTERFACE Header;
 /* Acquires the same v11 exclusive muted lease plus a fresh generation. */
 NTSTATUS (*AcquireMuted)(PVOID,ULONG rate,PULONG generation);
 /* Child has disabled DL1, masked/acked IRQ1, drained position/ISR/DPC,
  * disconnected IRQ and excluded new RUN before calling. Parent re-reads
  * all memif/IRQ enables and pending status under its own powered lease.
  * Success permits returning this generation's HAL SG/locked PCM pages.
  * No timeout is a successful stop. Failure retains ownership/resources. */
 NTSTATUS (*CertifyStopped)(PVOID,ULONG generation,ULONG software_state);
 /* Only after SG is returned (or while unchanged pages remain parked).
  * Ends voice/digital/codec/clock ownership, balances StopIdle, preserves
  * board always-on rails. Stale/un-certified generation is rejected. */
 NTSTATUS (*ReleaseStopped)(PVOID,ULONG generation);
};
#endif
