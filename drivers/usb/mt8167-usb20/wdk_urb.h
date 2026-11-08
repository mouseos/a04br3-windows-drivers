/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WDK_URB_H
#define A04_WDK_URB_H
#include <ntddk.h>
#include <usb.h>
#include "host_control.h"
#include "root_hub.h"
#if A04_MUSB_HOST_CORE_EXPERIMENT
/* Published WDK 8.1 URB types only. No private USBPORT/UCX ABI is declared.
 * Owner resolves device/pipe handles, serializes EP0, maps MDLs with sufficient
 * byte count, holds requests until hardware quiescence, and enforces deadlines.
 * No IRP completion, PDO publication or USB bus interface is fabricated here. */
typedef void *(*A04_URB_MAP_MDL)(void *context,PMDL mdl,ULONG required_bytes);
struct a04_wdk_control {
 struct a04_control_request request;
 PURB urb;
 unsigned capacity, short_ok, timeout_ms, prepared;
 struct a04_hub_action hub_action;
};
NTSTATUS A04UrbPrepare(PURB,USBD_PIPE_HANDLE expected_pipe,unsigned address,
 unsigned speed,unsigned max_packet,A04_URB_MAP_MDL,void *,struct a04_wdk_control *);
NTSTATUS A04UrbSubmit(struct a04_control_host *,struct a04_wdk_control *);
NTSTATUS A04UrbComplete(struct a04_wdk_control *);
/* Root-hub class controls use the same validated decoder, but no MMIO.
 * Pending actions belong to caller until board completion is verified. */
NTSTATUS A04UrbRootHub(struct a04_hub *,struct a04_wdk_control *,struct a04_hub_action *);
NTSTATUS A04UrbRootHubDone(struct a04_hub *,struct a04_wdk_control *,
 const struct a04_hub_action *,int,const struct a04_hub_observation *);
#endif
#endif
