/* SPDX-License-Identifier: MIT
 * pvr_arch_cmd_buffer.c:pvr_setup_viewport (height/2, x/y+extent/2,
 * maxDepth-minDepth, minDepth). D3D uses a negative height viewport
 * at native Y=top+height. No CPU vertex-coordinate rewrite. */
#include "native_d3d_viewport.h"
#include "../../wddm-render/clear_protocol.h"
static int integral(uint32_t f,uint32_t max,uint32_t *out)
{
 uint32_t exponent=(f>>23)&255u,significand=(f&0x7fffffu)|0x800000u;
 if(!(f&0x7fffffffu)){*out=0;return 1;}
 if((f>>31) || exponent<127 || exponent>136)return 0;
 uint32_t shift=150-exponent;
 if(significand&((1u<<shift)-1u))return 0;
 uint32_t v=significand>>shift;if(v>max)return 0;
 *out=v;return 1;
}
/* Exactly encode n/2, n<=1600: every result is representable in binary32. */
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
 if(!integral(f[0],799,&x) || !integral(f[1],599,&y) ||
    !integral(f[2],800,&w) || !integral(f[3],600,&h) || !w || !h ||
    x>800-w || y>600-h || f[4] || f[5]!=0x3f800000u)return GEO_UNSUPPORTED;
 made[0]=half(w);made[1]=half(2*x+w);
 made[2]=half(h)|0x80000000u;made[3]=half(2*y+h);
 made[4]=0x3f800000u;made[5]=0;
 for(unsigned i=0;i<6;i++)out[i]=made[i];
 return GEO_OK;
}
