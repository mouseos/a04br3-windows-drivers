/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_MUSB_PARENT_STATE_H
#define MT8167_MUSB_PARENT_STATE_H
#include "musb_owner_interface.h"
#include "musb_board_proof.h"
struct mt_musb_parent {
 PHYSICAL_ADDRESS mac_pa,phy_pa;PVOID mac,phy;ULONGLONG generation;
 ULONG bound,accessible,held,poisoned,stop_ok,powered,stage;
};
#endif
