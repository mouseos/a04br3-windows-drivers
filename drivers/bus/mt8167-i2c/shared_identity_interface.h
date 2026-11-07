/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_I2C1_SHARED_IDENTITY_INTERFACE_H
#define A04_I2C1_SHARED_IDENTITY_INTERFACE_H
#include <ntddk.h>
/* Private versioned kernel query-interface, not an SPB controller or IOCTL.
 * Only the MMA provider maps I2C1. No caller receives register pointers. */
static const GUID A04I2c1IdentityInterfaceGuid={0x6dfc0f04,0xbd49,0x4be9,{0x8b,0x8b,0x04,0x01,0x55,0x60,0x00,0x01}};
static const GUID A04I2c1OwnerDeviceGuid={0xb21b6c27,0x753c,0x4931,{0x91,0x04,0x55,0x60,0x00,0x01,0x00,0x01}};
#define A04_I2C1_IDENTITY_VERSION 1
/* A real shared rail/clock provider must implement this contract. Acquire
 * fails without side effects, or returns a live nonzero token. Release
 * preserves all other rail/clock consumers, including MMA/touch/audio.
 * No implementation is inferred from a caller boolean or hard-coded timing.
 * Called with the I2C1 controller lock held: never re-enter this interface. */
typedef struct {
 INTERFACE Header;
 NTSTATUS (*Acquire)(PVOID,ULONG *token);
 BOOLEAN (*Live)(PVOID,ULONG token);
 NTSTATUS (*Release)(PVOID,ULONG token);
} A04_HI556_POWER_INTERFACE;
typedef struct {
 ULONG stage,bytes_read,id_observed,power_observed,transport_observed,release_observed,quarantined;
 USHORT chip_id;
 NTSTATUS transport_status,power_status,power_release_status;
} A04_HI556_IDENTITY_RESULT;
typedef struct {
 INTERFACE Header;
 NTSTATUS (*Identify)(PVOID,const A04_HI556_POWER_INTERFACE *,A04_HI556_IDENTITY_RESULT *);
} A04_I2C1_IDENTITY_INTERFACE;
/* PASSIVE, referenced query-interface power object retained by caller for
 * this whole synchronous call. No arbitrary address/write/scan operations. */
#endif
