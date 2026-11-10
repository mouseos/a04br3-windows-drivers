/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_CLEAR_PROTOCOL_H
#define GEO_CLEAR_PROTOCOL_H
#include <stdint.h>
#include "frame_geometry356.h"
#define GEO_ABI_VERSION 1u
#define GEO_COPY_MAGIC 0x31504f47u /* GOP1: real GPU RT -> staging transfer */
#define GEO_CLEAR_MAGIC 0x31434f47u
#define GEO_FORMAT_A8R8G8B8 21u /* D3DDDIFMT_A8R8G8B8, BGRA bytes on ARM LE. */
#define GEO_FORMAT_X8R8G8B8 22u /* Standard lockable staging only; never a GPU render target. */
#define GEO_USAGE_RENDER_TARGET 1u
#define GEO_USAGE_STAGING 2u
#define GEO_USAGE_VERTEX_BUFFER 3u
#define GEO_FORMAT_VERTEXDATA 100u
#define GEO_USAGE_INDEX_BUFFER 4u
#define GEO_TEXTURE_STORAGE_VERSION 2u
#define GEO_USAGE_TEXTURE2D 5u
/* One linear BGRA backing, full native extent, both PBE output and SMP input. */
#define GEO_USAGE_RT_TEXTURE2D 6u
static inline int geo_usage_renderable(uint32_t u) {return u==GEO_USAGE_RENDER_TARGET||u==GEO_USAGE_RT_TEXTURE2D;}
static inline int geo_usage_sampleable(uint32_t u) {return u==GEO_USAGE_TEXTURE2D||u==GEO_USAGE_RT_TEXTURE2D;}
#define GEO_FORMAT_INDEX16 101u
#define GEO_FORMAT_INDEX32 102u
#define GEO_VERTEX_MAX_BYTES 65536u
struct geo_surface_desc { uint32_t size,version,width,height,pitch,format,usage,reserved; };
/* Logical API extent is distinct from physical storage. The first native
 * profile retains a proven 800x600 BGRA backing, including driver padding. */
struct geo_allocation_data {
 uint32_t size,version,logical_width,logical_height;
 struct geo_surface_desc storage;
};

/* V2 allocation metadata adds explicit standard-primary eligibility. GPU
 * storage descriptors and command wire remain V1; no caller-supplied address. */
#define GEO_ALLOCATION_PRIMARY 1u
struct geo_allocation_data_v2 {struct geo_allocation_data base;uint32_t flags,source_id;};
/* Semantic command only. Allocation index is resolved through VidMm's list.
 * No user VA, PA, GPU VA, firmware command, shader or arbitrary payload. */
struct geo_clear_packet { uint32_t magic,size,version,allocation_index,color,flags,reserved[2]; };
struct geo_copy_packet { uint32_t magic,size,version,destination_index,source_index,width,height,reserved; };
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
