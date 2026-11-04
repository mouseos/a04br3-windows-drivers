/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Prototype kernel interface. The provider must keep Context alive until the
 * last InterfaceDereference and serialize each operation with D0 exit/removal.
 * References preserve memory, not power or access to unmapped registers.
 * Operations must fail after admission closes and never touch removed MMIO.
 */
#ifndef A04_SHARED_CLIENT_INTERFACE_H
#define A04_SHARED_CLIENT_INTERFACE_H
#include <ntddk.h>
static const GUID A04SharedInterfaceGuid =
    {0xefcb3ba1,0x8939,0x4cef,{0x82,0x1f,0x94,0x54,0x15,0xa0,0x4c,0x63}};
/* Touch bank values match enum a04_touch_register_bank, not the parent
 * internal a04_shared_bank values. The provider must explicitly translate. */
#define A04_INTERFACE_TOUCH_GPIO 0u
#define A04_INTERFACE_TOUCH_TOP 1u
#define A04_INTERFACE_TOUCH_INFRA 2u
typedef struct _A04_SHARED_INTERFACE {
    USHORT Size, Version;
    PVOID Context;
    PINTERFACE_REFERENCE InterfaceReference;
    PINTERFACE_DEREFERENCE InterfaceDereference;
    int (*TouchUpdate)(PVOID, unsigned, unsigned, ULONG, ULONG);
    int (*TouchWrite)(PVOID, unsigned, unsigned, ULONG);
    int (*TouchPen)(PVOID, int *);
    int (*ButtonPmicRead)(PVOID, USHORT *);
} A04_SHARED_INTERFACE;
#define A04_SHARED_INTERFACE_VERSION 1
#endif
