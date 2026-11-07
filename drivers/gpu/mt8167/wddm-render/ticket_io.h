/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_TICKET_IO_H
#define GEO_TICKET_IO_H
#include <ntddk.h>
#include "clear_protocol.h"
/* Kernel runtime DMA/private buffers: do not assume ARM Normal memory.
 * Exact aligned 32-bit accesses are valid for the semantic ticket even when
 * a runtime mapping is Device memory. No byte/SIMD/unaligned memcpy. */
static __inline NTSTATUS GeoReadTicket(const void *buffer,struct geo_dma_ticket *t) {
 volatile ULONG *p=(volatile ULONG *)buffer;
 if(!buffer||!t||((ULONG_PTR)buffer&3))return STATUS_INVALID_PARAMETER;
 t->magic=READ_REGISTER_ULONG(p);t->slot=READ_REGISTER_ULONG(p+1);
 t->generation=READ_REGISTER_ULONG(p+2);t->reserved=READ_REGISTER_ULONG(p+3);return STATUS_SUCCESS;
}
static __inline NTSTATUS GeoWriteTicket(void *buffer,const struct geo_dma_ticket *t) {
 volatile ULONG *p=buffer;
 if(!buffer||!t||((ULONG_PTR)buffer&3))return STATUS_INVALID_PARAMETER;
 WRITE_REGISTER_ULONG(p,t->magic);WRITE_REGISTER_ULONG(p+1,t->slot);
 WRITE_REGISTER_ULONG(p+2,t->generation);WRITE_REGISTER_ULONG(p+3,t->reserved);return STATUS_SUCCESS;
}
#endif
