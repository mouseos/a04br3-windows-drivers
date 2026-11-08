/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_DOD_TQ_BRIDGE_H
#define GEO_DOD_TQ_BRIDGE_H
#include "../dod_tq_interface.h"
#ifndef DXGKDDI_INTERFACE_VERSION
#define DXGKDDI_INTERFACE_VERSION 0x4002
#endif
#include <dispmprt.h>
/* Embedded in the single render miniport. No new PDO/IRQ/MMIO/DMA owner.
 * Backend must enter the SAME native owner's D0/rundown/serialization, refuse
 * active render work and retain failed DMA. Publishing without it is forbidden.
 * Configure only after a real backend exists; NULL means unavailable, not CPU
 * copy emulation. Stop waits for synchronous calls; references block removal. */
struct geo_dod_tq_bridge {
 KMUTEX gate;
 volatile LONG references;
 BOOLEAN active;
 GEO_DOD_TQ_COPY backend;
 PVOID backend_context;
};
VOID GeoDodTqInitialize(struct geo_dod_tq_bridge *);
NTSTATUS GeoDodTqStart(struct geo_dod_tq_bridge *,GEO_DOD_TQ_COPY,PVOID);
VOID GeoDodTqStop(struct geo_dod_tq_bridge *);
BOOLEAN GeoDodTqCanRemove(struct geo_dod_tq_bridge *);
NTSTATUS GeoDodTqQuery(struct geo_dod_tq_bridge *,PQUERY_INTERFACE);
#endif
