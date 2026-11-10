/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_USB_HOST_USB_H
#define MT8167_USB_HOST_USB_H
#include <stdint.h>
typedef uint32_t USBD_STATUS;
#define USBD_STATUS_SUCCESS 0u
#define USBD_STATUS_STALL_PID 0xc0000004u
#define USBD_STATUS_XACT_ERROR 0xc0000011u
#define USBD_STATUS_CANCELED 0xc0010000u
#endif
