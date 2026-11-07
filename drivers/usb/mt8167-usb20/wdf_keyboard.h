#ifndef A04_USB_WDF_KEYBOARD_H
#define A04_USB_WDF_KEYBOARD_H
#include <ntddk.h>
#include <wdf.h>
#include "board_host.h"
/* The caller must retain an exclusive, serialized USB clock/PHY/regulator and
 * GPIO22 lease across Activate..Deactivate, and provide callbacks with that
 * exact lifetime. These are internal same-driver entry points, not a public
 * PnP interface or evidence that a provider exists. */
NTSTATUS KbHidActivate(WDFDEVICE,const A04_USB_BOARD *);
void KbHidDeactivate(WDFDEVICE);
#endif
