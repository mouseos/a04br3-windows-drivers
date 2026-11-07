#ifndef A04_MT8167_SHARED_USB_INTERFACE_H
#define A04_MT8167_SHARED_USB_INTERFACE_H
#include <ntddk.h>
/* Only valid on a child stack forwarding queries to its real parent. The
 * acquired lease must outlive all PHY and controller accesses. PASSIVE_LEVEL.
 * Acquire: 0 grants a lease; the two positive usb_admission.h wait results
 * grant no lease and have no hardware side effects. Negative/unknown results
 * fail closed. This extends return semantics, not the v1 structure or GUID. */
static const GUID A04SharedUsbInterfaceGuid =
 {0x91a4a93e,0x635e,0x42a5,{0x88,0x21,0xa7,0x2c,0x83,0x10,0x21,0x54}};
typedef struct _A04_SHARED_USB_INTERFACE {
 INTERFACE Header;
 int (*Acquire)(PVOID); int (*SetVbus)(PVOID,int); int (*Health)(PVOID); int (*Release)(PVOID);
} A04_SHARED_USB_INTERFACE;
#define A04_SHARED_USB_INTERFACE_VERSION 1
#endif
