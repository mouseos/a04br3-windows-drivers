/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Include after Windows/WDK headers. The shared parent owns the GPIO mapping. */
#ifndef MT8167_ACCDET_IOCTL_H
#define MT8167_ACCDET_IOCTL_H
#include "socket.h"
#include "status.h"
static const GUID GUID_MT8167_ACCDET_SOCKET =
 {0x42d17690,0xb0a3,0x4e17,{0xa9,0x21,0x8d,0x06,0x9c,0xc8,0x77,0x31}};
#define IOCTL_MT8167_ACCDET_READ_SOCKET CTL_CODE(FILE_DEVICE_UNKNOWN,0x955,METHOD_BUFFERED,FILE_READ_DATA)
#define IOCTL_MT8167_ACCDET_QUERY_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN,0x956,METHOD_BUFFERED,FILE_READ_DATA)
#endif
