/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "texture_draw_abi.h"
int geo_texture_sampler_profile_valid(uint32_t p) {
 uint32_t f=(p>>8)&255u,a=p&255u;
 if(p==GEO_TEXTURE_POINT_CLAMP)return 1;
 if((p&0xffff0000u)!=GEO_TEXTURE_FILTER_PROFILE4||a<1||a>16)return 0;
 if(f==0x55u)return 1;
 return !(f&~0x15u)&&a==1;
}
int32_t geo_draw_validate_capture_v3(const struct geo_draw_request_v3 *r,const void *blob,uint32_t bytes)
{
 struct geo_draw_request flat;
 if(!r||r->base.size!=sizeof(*r)||r->base.version!=3||r->reserved||!geo_texture_sampler_profile_valid(r->sampler_profile)||
    !r->texture||r->texture==r->base.target||r->texture==r->base.vertex||r->texture_desc.version!=GEO_TEXTURE_STORAGE_VERSION||
    r->texture_desc.usage!=GEO_USAGE_TEXTURE2D||!geo_desc_valid(&r->texture_desc))return GEO_INVALID;
 flat=r->base;flat.size=sizeof(flat);flat.version=1;
 return geo_draw_validate_capture(&flat,blob,bytes);
}
