/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_COMBO_CONTROLLER_H
#define A04_MT6630_COMBO_CONTROLLER_H
#include <ntddk.h>
#include <wdf.h>
#include "physical_owner.h"
#include "../../bus/mt8167-shared/hfp_interface.h"
#include "controller_interface.h"
struct a04_combo_controller {
 WDFDEVICE root,radio;WDFIOTARGET target;WDFWAITLOCK serial;
 struct a04_bt_physical_owner physical;UNICODE_STRING ready_link;
 EX_RUNDOWN_REF live_calls;volatile LONG running,removed,uncertain,holds;
 BOOLEAN live_closed,ready;
};
NTSTATUS a04_combo_controller_init(struct a04_combo_controller *,WDFDEVICE,WDFDEVICE,WDFIOTARGET,WDFWAITLOCK);
/* Start/Stop are called with the existing client's transport wait-lock held. */
NTSTATUS a04_combo_controller_start_locked(struct a04_combo_controller *);
NTSTATUS a04_combo_controller_stop_locked(struct a04_combo_controller *);
void a04_combo_controller_destroy(struct a04_combo_controller *);
#endif
