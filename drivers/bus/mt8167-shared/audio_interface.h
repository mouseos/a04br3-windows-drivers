/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_AUDIO_INTERFACE_H
#define A04_SHARED_AUDIO_INTERFACE_H
#include <ntddk.h>
/* Prototype QUERY_INTERFACE contract; parent retains Context until the last
 * InterfaceDereference. All operations are PASSIVE_LEVEL and must reject once
 * the parent closes admission. Success must mean exclusive audio clock and a
 * verified muted codec, not merely an available register mapping. */
static const GUID A04SharedAudioInterfaceGuid =
    {0x5b8d8b10,0x347b,0x4f76,{0x9c,0x7e,0x92,0x06,0x61,0xb3,0xd0,0x45}};
typedef struct _A04_SHARED_AUDIO_INTERFACE {
    INTERFACE Header;
    int (*AcquireMuted)(PVOID, ULONG sampleRate);
    int (*ReleaseQuiescent)(PVOID);
    /* PASSIVE_LEVEL, with an acquired audio lease. On success the parent
     * has exclusive AFE/clock control, has masked and cleared every source
     * on this IRQ. The parent keeps all other sources masked throughout the
     * lease; the child may enable DL1 only after ISR connection, and must
     * mask DL1 and synchronize/disconnect the ISR before releasing. The mapping
     * is the child's PnP-assigned AFE page and remains valid for this call.
     * On failure no interrupt may be connected; parent retains responsibility
     * for any partial operation until ReleaseQuiescent reports success. */
    int (*PrepareMaskedIrq)(PVOID, PVOID afe, ULONG length);
} A04_SHARED_AUDIO_INTERFACE;
#define A04_SHARED_AUDIO_INTERFACE_VERSION 3
#endif
