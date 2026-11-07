/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_RADIO_BINDING_H
#define A04_MT6630_RADIO_BINDING_H
#include "hfp_endpoint.h"
/* Kernel PnP QUERY_INTERFACE only. No user callback/pointer IOCTL.
 * The Audio owner supplies real MRG/DMA/IRQ callbacks; this interface does
 * not create a second SDIO owner or turn absent qualifications into success. */
static const GUID A04Mt6630SidebandInterfaceGuid={0x292160a3,0xd794,0x4d76,{0xa4,0xc2,0x45,0xdb,0xa2,0xcb,0x16,0x09}};
#define A04_MT6630_SIDEBAND_VERSION 1u
struct a04_mt6630_sideband_interface {
 INTERFACE Header;
 NTSTATUS (*Bind)(PVOID,const struct a04_bt_hfp_provider *,PULONG);
 NTSTATUS (*Unbind)(PVOID,ULONG);
};
struct a04_mt6630_radio_binding {
 KMUTEX lock;struct a04_bt_hfp_provider provider;PDEVICE_OBJECT radio;
 ULONG generation;BOOLEAN bound,retired;
};
void a04_mt6630_binding_init(struct a04_mt6630_radio_binding *,PDEVICE_OBJECT);
NTSTATUS a04_mt6630_binding_bind(struct a04_mt6630_radio_binding *,const struct a04_bt_hfp_provider *,PULONG);
NTSTATUS a04_mt6630_binding_unbind(struct a04_mt6630_radio_binding *,ULONG);
/* PASSIVE. Calls the actual PCM/latency provider, never inferred from HCI ACK.
 * No active HFP peer/SLC/STREAM_OPEN prerequisite at stack-load time. */
BOOLEAN a04_mt6630_binding_capable(struct a04_mt6630_radio_binding *);
BOOLEAN a04_mt6630_binding_attached(struct a04_mt6630_radio_binding *);
#endif
