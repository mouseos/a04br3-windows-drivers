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
} A04_SHARED_AUDIO_INTERFACE;
#define A04_SHARED_AUDIO_INTERFACE_VERSION 1
#endif
