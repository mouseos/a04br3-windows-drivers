/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_CLEAR_PROTOCOL_H
#define GEO_CLEAR_PROTOCOL_H
#include <stdint.h>
#define GEO_ABI_VERSION 1u
#define GEO_CLEAR_MAGIC 0x31434f47u
#define GEO_FORMAT_A8R8G8B8 21u /* D3DDDIFMT_A8R8G8B8, BGRA bytes on ARM LE. */
#define GEO_USAGE_RENDER_TARGET 1u
#define GEO_USAGE_STAGING 2u
struct geo_surface_desc { uint32_t size,version,width,height,pitch,format,usage,reserved; };
/* Semantic command only. Allocation index is resolved through VidMm's list.
 * No user VA, PA, GPU VA, firmware command, shader or arbitrary payload. */
struct geo_clear_packet { uint32_t magic,size,version,allocation_index,color,flags,reserved[2]; };
struct geo_dma_ticket { uint32_t magic,slot,generation,reserved; };
#define GEO_TICKET_MAGIC 0x31544f47u
#define GEO_OK ((int32_t)0)
#define GEO_INVALID ((int32_t)0xc000000d)
#define GEO_UNSUPPORTED ((int32_t)0xc00000bb)
#define GEO_BUSY ((int32_t)0x80000011)
#define GEO_NOT_READY ((int32_t)0xc00000a3)
#define GEO_NOT_FOUND ((int32_t)0xc0000225)
#define GEO_POISONED ((int32_t)0xc0000184)
#ifdef __cplusplus
extern "C" {
#endif
int geo_desc_valid(const struct geo_surface_desc *);
int geo_packet_valid(const struct geo_clear_packet *,uint32_t allocation_count);
#ifdef __cplusplus
}
#endif
#endif
