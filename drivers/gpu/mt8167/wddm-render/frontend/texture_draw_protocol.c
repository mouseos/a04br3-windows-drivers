/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "texture_draw_abi.h"
int32_t geo_draw_validate_capture_v3(const struct geo_draw_request_v3 *r,const void *blob,uint32_t bytes)
{
 struct geo_draw_request flat;
 if(!r||r->base.size!=sizeof(*r)||r->base.version!=3||r->reserved||r->sampler_profile!=GEO_TEXTURE_POINT_CLAMP||
    !r->texture||r->texture==r->base.target||r->texture==r->base.vertex||r->texture_desc.version!=GEO_TEXTURE_STORAGE_VERSION||
    r->texture_desc.usage!=GEO_USAGE_TEXTURE2D||!geo_desc_valid(&r->texture_desc))return GEO_INVALID;
 flat=r->base;flat.size=sizeof(flat);flat.version=1;
 return geo_draw_validate_capture(&flat,blob,bytes);
}
