/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "draw_link_abi.h"
#include "draw_validation_private.h"
int32_t geo_draw_validate_capture_v2(const struct geo_draw_request_v2 *r,const void *blob,uint32_t bytes)
{
 if(!r)return GEO_INVALID;
 int32_t s=capture(&r->base,blob,bytes,sizeof(*r),2,32);if(s)return s;
 const struct geo_draw_pair_link *l=&r->link;
 if(l->size!=sizeof(*l) || l->version!=2 || l->reserved || l->varying_count!=1 ||
    l->fs_uses_z>1 || l->fs_uses_w>1 || l->vs_vtxouts!=r->base.vs.vtxouts ||
    (l->vs_entry_offset&3) || (l->fs_entry_offset&3) ||
    l->vs_entry_offset>=r->base.vs.usc.bytes || l->fs_entry_offset>=r->base.ps.usc.bytes)
    return GEO_INVALID;
 return GEO_OK;
}
