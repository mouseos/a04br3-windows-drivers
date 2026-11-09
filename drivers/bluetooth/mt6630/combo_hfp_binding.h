/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_COMBO_HFP_BINDING_H
#define A04_MT6630_COMBO_HFP_BINDING_H
#include "hfp_pnp.h"
struct mt6630_combo_hfp_binding;
/* Actual HCI PDO from the controller descriptor, not a user path/name. */
NTSTATUS Mt6630ComboHfpAttach(PDEVICE_OBJECT radio,PDEVICE_OBJECT audio,PRESOURCELIST,
 const struct a04_bt_hfp_provider *,struct mt6630_combo_hfp_binding **);
NTSTATUS Mt6630ComboHfpDetach(struct mt6630_combo_hfp_binding *);
#endif
