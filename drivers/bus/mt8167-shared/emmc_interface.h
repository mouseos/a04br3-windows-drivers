/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_EMMC_INTERFACE_H
#define A04_SHARED_EMMC_INTERFACE_H
#include <ntddk.h>
#include "../../input/a04br3-buttons/pmic_read.h"
/* Offline diagnostic contract only. A reference retains the parent object,
 * not power, bus presence, clocks, pinmux or the eMMC controller. ReadRails
 * returns fixed raw PMIC observations only after parent admission and fails
 * without changing the output otherwise. It cannot authorize I/O or D0.
 * Storport integration and a real child PDO are not provided. */
static const GUID A04SharedEmmcInterfaceGuid =
    {0xa34b1311,0x5d86,0x41c4,{0x9b,0x11,0x7a,0x0b,0xc4,0xa0,0xe1,0x61}};
typedef struct _A04_SHARED_EMMC_INTERFACE {
    INTERFACE Header;
    int (*ReadRails)(PVOID, struct a04_emmc_rails_raw *);
} A04_SHARED_EMMC_INTERFACE;
#define A04_SHARED_EMMC_INTERFACE_VERSION 1
#endif
