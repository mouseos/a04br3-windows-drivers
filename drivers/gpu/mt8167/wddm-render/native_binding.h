/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_NATIVE_BINDING_H
#define GEO_NATIVE_BINDING_H
#include "lifecycle.h"
/* Native core factory is SOURCE-LINKED inside the same SYS. No KMDF provider,
 * PnP sibling query, device interface or arbitrary binary/payload transport. */
struct geo_native_binding {
 UINT size,version;
 struct geo_owner_control_v2 control;
 struct geo_aperture_config aperture;
 void *native_context;
};
/* PASSIVE. Uses DxgkCbGetDeviceInformation resources and Dxgk callback memory
 * contract. No IoConnectInterrupt/WdfInterruptCreate: Dxgk owns the sole IRQ.
 * Factory failure must set native_context when partial resources need pinning.
 * After success caller acquires a referenced lease through control; factory's
 * base reference is retained until delete after successful session stop. */
/* V3 symbol prevents accidentally calling a legacy four-argument factory.
 * Output binding/control/ops versions remain 2; memory input is version 1. */
NTSTATUS GeoNativeOwnerCreateV3(PDEVICE_OBJECT,const DXGKRNL_INTERFACE *,const DXGK_DEVICE_INFO *,const struct geo_frontend_memory *,struct geo_native_binding *);
NTSTATUS GeoNativeOwnerDrainForDisplay(struct geo_native_binding *);
NTSTATUS GeoNativeOwnerDelete(struct geo_native_binding *);
/* Kernel render contract/caps/preemption are implemented by this sidecar.
 * Native factory must implement actual memory/command/IRQ/control operations,
 * not Windows feature-level claims. No fallback factory definition exists. */
extern const DRIVER_INITIALIZATION_DATA geo_ge8300_render_contract;
#endif
