/* SPDX-License-Identifier: MIT */
#ifndef GEO_COPY_REGION_H
#define GEO_COPY_REGION_H
#include "clear_protocol.h"
/* Unsigned, non-scaling, single mip/sample 2D texel region. Logical bounds
 * are checked by the API producer; the kernel independently checks them
 * against each referenced allocation. Subtractions avoid overflow. */
struct geo_copy_region {uint32_t source_x,source_y,destination_x,destination_y,width,height;};
static inline int geo_copy_region_valid(const struct geo_copy_region *r,
 uint32_t sw,uint32_t sh,uint32_t dw,uint32_t dh) {
 return r&&r->width&&r->height&&r->source_x<=sw&&r->source_y<=sh&&
 r->destination_x<=dw&&r->destination_y<=dh&&r->width<=sw-r->source_x&&
 r->height<=sh-r->source_y&&r->width<=dw-r->destination_x&&r->height<=dh-r->destination_y;
}
/* Same allocation-list identities as GOP1; independently sized V2 command.
 * No GPU VA, pointers, bytecode, scaling or completion hints on the wire. */
struct geo_copy_region_packet {uint32_t magic,size,version,destination_index,source_index,reserved;struct geo_copy_region region;};
#endif
