/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_CLOCK_SNAPSHOT_IOCTL_H
#define MT8167_CLOCK_SNAPSHOT_IOCTL_H
/* Existing resource-less thermal device interface, new READ_DATA operation.
 * Windows/WDK CTL_CODE definitions must be included by the caller. */
#include "../../bus/mt8167-shared/clock_snapshot.h"
#define MT8167_CLOCK_GET_SNAPSHOT CTL_CODE(FILE_DEVICE_UNKNOWN,0xa06,METHOD_BUFFERED,FILE_READ_DATA)
#endif
