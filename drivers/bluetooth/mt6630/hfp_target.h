/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BT_HFP_TARGET_H
#define A04_BT_HFP_TARGET_H
#include <ntifs.h>
/* PASSIVE, caller retains radio D0/combo lease and target file until return.
 * Proves either same PDO, direct PnP parent, or parent in radio's actual
 * Device_Children property. Deeper/unpublished ancestry is unsupported.
 * No BusRelations IRP (sending it is forbidden), private BTH IOCTL, guessed
 * instance prefixes, MAC comparison or public path/identity broker.
 * Runtime devnode IDs may contain individual data: erase, never log them. */
NTSTATUS a04_bt_hfp_match_radio(PDEVICE_OBJECT radio_pdo,PDEVICE_OBJECT target);
#endif
