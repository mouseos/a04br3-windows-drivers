/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <ntddk.h>
#include <usbspec.h>
#include "../../drivers/usb/mt8167-usb20/host_core.h"
#if !A04_MUSB_HOST_CORE_EXPERIMENT
#error host core contract requires the experimental flag
#endif
#if A04_PORT_CONNECTION != USB_PORT_STATUS_CONNECT || A04_PORT_ENABLE != USB_PORT_STATUS_ENABLE || A04_PORT_OVERCURRENT != USB_PORT_STATUS_OVER_CURRENT || A04_PORT_RESET != USB_PORT_STATUS_RESET || A04_PORT_POWER != USB_PORT_STATUS_POWER || A04_PORT_LOW_SPEED != USB_PORT_STATUS_LOW_SPEED || A04_PORT_HIGH_SPEED != USB_PORT_STATUS_HIGH_SPEED
#error root port status differs from WDK USB 2.0 definitions
#endif
C_ASSERT(sizeof(USB_PORT_STATUS_AND_CHANGE) == 4);
