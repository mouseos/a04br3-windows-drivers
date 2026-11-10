/* SPDX-License-Identifier: MIT
 * pvr_arch_cmd_buffer.c:pvr_setup_viewport (height/2, x/y+extent/2,
 * maxDepth-minDepth, minDepth). D3D uses a negative height viewport
 * at native Y=top+height. No CPU vertex-coordinate rewrite. */
#include "native_d3d_viewport.h"
#include "../../wddm-render/clear_protocol.h"
#include "../../wddm-render/viewport_profile.h"
/* Exactly encode n/2, n<=8192: every result is representable in binary32. */
static uint32_t half(uint32_t n)
{
 uint32_t top=0,v=n;if(!n)return 0;
 while(v>>1){v>>=1;top++;}
 return ((126u+top)<<23)|((n<<(23-top))&0x7fffffu);
}
int32_t a04_m111_build_d3d_viewport(uint32_t out[6],const uint32_t f[6])
{
 uint32_t x,y,w,h,made[6];
 if(!out || !f)return GEO_INVALID;
 if(!geo_viewport_profile_valid(f))return GEO_UNSUPPORTED;
 geo_viewport_integral(f[0],GEO_RENDER_MAX_EDGE,&x);geo_viewport_integral(f[1],GEO_RENDER_MAX_EDGE,&y);
 geo_viewport_integral(f[2],GEO_RENDER_MAX_EDGE,&w);geo_viewport_integral(f[3],GEO_RENDER_MAX_EDGE,&h);
 made[0]=half(2*x+w);made[1]=half(w);
 made[2]=half(2*y+h);made[3]=half(h)|0x80000000u;
 made[4]=0;made[5]=0x3f800000u;
 for(unsigned i=0;i<6;i++)out[i]=made[i];
 return GEO_OK;
}
