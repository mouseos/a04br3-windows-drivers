/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_MSDC2_H
#define A04_WINDOWS_MSDC2_H
#include <ntddk.h>
#include "msdc2_pio.h"
/* Prototype: caller must hold a non-reentrant PASSIVE_LEVEL lock across map,
 * all PIO and unmap, plus a live exclusive broker lease. Surprise removal can
 * still fault an in-flight MMIO access. No current driver calls this API. */
struct a04_windows_msdc2 { volatile UCHAR *base; struct a04_msdc2_pio pio; };
NTSTATUS a04_windows_msdc2_map(struct a04_windows_msdc2 *,
 const CM_PARTIAL_RESOURCE_DESCRIPTOR *raw,const CM_PARTIAL_RESOURCE_DESCRIPTOR *translated,
 struct a04_wifi_owner *,unsigned generation,int irq_quiesced);
void a04_windows_msdc2_unmap(struct a04_windows_msdc2 *);
#endif
