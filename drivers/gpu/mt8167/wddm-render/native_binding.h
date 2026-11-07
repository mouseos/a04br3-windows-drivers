/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_NATIVE_BINDING_H
#define GEO_NATIVE_BINDING_H
#include "lifecycle.h"
/* Native core factory is SOURCE-LINKED inside the same SYS. No KMDF provider,
 * PnP sibling query, device interface or arbitrary binary/payload transport. */
struct geo_native_binding {
 UINT size,version;
 struct geo_owner_control_v2 control;
 void *native_context;
};
/* PASSIVE. Uses DxgkCbGetDeviceInformation resources and Dxgk callback memory
 * contract. No IoConnectInterrupt/WdfInterruptCreate: Dxgk owns the sole IRQ.
 * Factory failure must set native_context when partial resources need pinning.
 * After success caller acquires a referenced lease through control; factory's
 * base reference is retained until delete after successful session stop. */
NTSTATUS GeoNativeOwnerCreate(PDEVICE_OBJECT,const DXGKRNL_INTERFACE *,const DXGK_DEVICE_INFO *,struct geo_native_binding *);
NTSTATUS GeoNativeOwnerDelete(struct geo_native_binding *);
/* Remaining render DDIs come from the reviewed rendering frontend, NOT the
 * native register/firmware core. Missing implementations are final link errors.
 * DriverEntry checks mandatory render entries and display entries before
 * registration, so a Clear-only table cannot masquerade as a WDDM adapter. */
extern const DRIVER_INITIALIZATION_DATA geo_ge8300_render_contract;
#endif
