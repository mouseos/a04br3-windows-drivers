/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_ADAPTER_INFO_H
#define GEO_ADAPTER_INFO_H
#include <stddef.h>
#include "aperture.h"
#include "clear_protocol.h"
#define GEO_ADAPTER_INFO_MAGIC 0x31494f47u
#define GEO_COMPILER_BVNC UINT64_C(0x001600280036001e)
/* UMD-visible, pointer-free wire v8. This is not struct geo_owner_ops:
 * no native function pointers or native struct size cross this boundary.
 * owner_ops_version explicitly identifies the supported native owner ABI4;
 * changing that ABI requires a deliberate wire/consumer compatibility review. */
#define GEO_ADAPTER_WIRE_VERSION 8u
#define GEO_ADAPTER_WIRE_BYTES 96u
#define GEO_ADAPTER_WIRE_OWNER_OPS_VERSION 4u
#define GEO_ADAPTER_WIRE_CONTROL_VERSION 2u
#define GEO_ADAPTER_WIRE_DRAW_VERSION 1u
#define GEO_ADAPTER_WIRE_TEXTURE_DRAW_VERSION 3u
#define GEO_ADAPTER_WIRE_SHADER_DRAW_VERSION 4u
struct geo_adapter_memory_info {
 uint32_t size,version,magic,owner_ops_version,control_version,format,max_allocation_bytes,texture_draw_version;
 struct geo_aperture_config aperture;
 uint64_t compiler_bvnc;
 uint32_t draw_version,vertex_max_bytes,shader_draw_version;
};
/* Preserve the existing RT8.1 ARM wire layout, independently of native ops. */
typedef char geo_adapter_wire_size[(sizeof(struct geo_adapter_memory_info)==GEO_ADAPTER_WIRE_BYTES)?1:-1];
typedef char geo_adapter_wire_aperture_offset[(offsetof(struct geo_adapter_memory_info,aperture)==32)?1:-1];
typedef char geo_adapter_wire_bvnc_offset[(offsetof(struct geo_adapter_memory_info,compiler_bvnc)==72)?1:-1];
typedef char geo_adapter_wire_draw_offset[(offsetof(struct geo_adapter_memory_info,draw_version)==80)?1:-1];
/* Stable diagnostic bits. Dynamic aperture/capacity values are not guessed or
 * substituted by this fixed-tuple check. Zero is the only accepted mask. */
#define GEO_ADAPTER_WIRE_BAD_SIZE     (1u<<0)
#define GEO_ADAPTER_WIRE_BAD_VERSION  (1u<<1)
#define GEO_ADAPTER_WIRE_BAD_MAGIC    (1u<<2)
#define GEO_ADAPTER_WIRE_BAD_OWNER    (1u<<3)
#define GEO_ADAPTER_WIRE_BAD_CONTROL  (1u<<4)
#define GEO_ADAPTER_WIRE_BAD_TEXTURE  (1u<<5)
#define GEO_ADAPTER_WIRE_BAD_SHADER   (1u<<6)
#define GEO_ADAPTER_WIRE_BAD_BVNC     (1u<<7)
#define GEO_ADAPTER_WIRE_BAD_DRAW     (1u<<8)
#define GEO_ADAPTER_WIRE_BAD_FORMAT   (1u<<9)
#define GEO_ADAPTER_WIRE_BAD_VERTEX   (1u<<10)
static inline uint32_t geo_adapter_wire_mismatch(const struct geo_adapter_memory_info *p) {
 uint32_t mask=0;
 if(p->size!=GEO_ADAPTER_WIRE_BYTES)mask|=GEO_ADAPTER_WIRE_BAD_SIZE;
 if(p->version!=GEO_ADAPTER_WIRE_VERSION)mask|=GEO_ADAPTER_WIRE_BAD_VERSION;
 if(p->magic!=GEO_ADAPTER_INFO_MAGIC)mask|=GEO_ADAPTER_WIRE_BAD_MAGIC;
 if(p->owner_ops_version!=GEO_ADAPTER_WIRE_OWNER_OPS_VERSION)mask|=GEO_ADAPTER_WIRE_BAD_OWNER;
 if(p->control_version!=GEO_ADAPTER_WIRE_CONTROL_VERSION)mask|=GEO_ADAPTER_WIRE_BAD_CONTROL;
 if(p->texture_draw_version!=GEO_ADAPTER_WIRE_TEXTURE_DRAW_VERSION)mask|=GEO_ADAPTER_WIRE_BAD_TEXTURE;
 if(p->shader_draw_version!=GEO_ADAPTER_WIRE_SHADER_DRAW_VERSION)mask|=GEO_ADAPTER_WIRE_BAD_SHADER;
 if(p->compiler_bvnc!=GEO_COMPILER_BVNC)mask|=GEO_ADAPTER_WIRE_BAD_BVNC;
 if(p->draw_version!=GEO_ADAPTER_WIRE_DRAW_VERSION)mask|=GEO_ADAPTER_WIRE_BAD_DRAW;
 if(p->format!=GEO_FORMAT_A8R8G8B8)mask|=GEO_ADAPTER_WIRE_BAD_FORMAT;
 if(p->vertex_max_bytes!=GEO_VERTEX_MAX_BYTES)mask|=GEO_ADAPTER_WIRE_BAD_VERTEX;
 return mask;
}
#endif
