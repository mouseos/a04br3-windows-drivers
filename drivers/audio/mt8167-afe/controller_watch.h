/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_CONTROLLER_WATCH_H
#define MT8167_CONTROLLER_WATCH_H
#include "../../bluetooth/mt6630/controller_interface.h"
struct Mt6630ControllerWatch;
/* Serial PASSIVE worker. Descriptor remains referenced until a successful
 * NULL/remove callback; failure pins the watcher and controller, no retry. */
typedef NTSTATUS (*Mt6630ControllerChanged)(PVOID,const A04_HFP_CONTROLLER *);
NTSTATUS Mt6630ControllerWatchStart(PDEVICE_OBJECT,Mt6630ControllerChanged,PVOID,Mt6630ControllerWatch **);
NTSTATUS Mt6630ControllerWatchStop(Mt6630ControllerWatch *);
#endif
