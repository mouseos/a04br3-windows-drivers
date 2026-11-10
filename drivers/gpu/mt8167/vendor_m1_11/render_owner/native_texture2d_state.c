/* SPDX-License-Identifier: MIT
 * Imagination Mesa251b89ed: rogue/texstate.xml, pvr_arch_tex_state.c,
 * pvr_arch_sampler.c. BVNC22.40.54.30/ge8300.h has no BRN51025.
 * No captured descriptor or guessed register field. */
#include "native_texture2d_state.h"
#include "../../wddm-render/clear_protocol.h"
static void words(uint32_t *out,uint64_t a,uint64_t b)
{out[0]=(uint32_t)a;out[1]=(uint32_t)(a>>32);out[2]=(uint32_t)b;out[3]=(uint32_t)(b>>32);}
int32_t a04_m111_texture2d_state_filtered(struct a04_texture2d_state *out,const struct a04_texture2d_storage *s,uint32_t filter,uint32_t max_anisotropy)
{
 uint32_t aniso=0;
 struct a04_texture2d_state made;uint64_t extent,word0,word1,sampler;
 /* DDI regular filter bits: min4/mag2/mip0. Mesa pvr_arch_sampler.c
  * floors anisotropy to supported2/4/8/16 ratios, encoded by texstate.xml.
  * Comparison/reduction/text1bit have distinct contracts and are rejected. */
 if(filter==0x55u){
  if(!max_anisotropy||max_anisotropy>16)return GEO_INVALID;
  aniso=max_anisotropy>=16?4:max_anisotropy>=8?3:max_anisotropy>=4?2:max_anisotropy>=2?1:0;
 }else if((filter&~0x15u)||max_anisotropy!=1)return GEO_UNSUPPORTED;
 if(!out||!s||s->size!=sizeof(*s)||s->version!=A04_TEXTURE2D_STATE_VERSION||s->reserved)return GEO_INVALID;
 if((s->format!=A04_TEXTURE2D_BGRA8&&s->format!=A04_TEXTURE2D_RGBA8)||s->mip_levels!=1||s->array_size!=1||s->sample_count!=1)return GEO_UNSUPPORTED;
 if(!s->width||!s->height||s->width>A04_TEXTURE2D_MAX_EDGE||s->height>A04_TEXTURE2D_MAX_EDGE)return GEO_UNSUPPORTED;
 if(!geo_texture_layout_valid(s->width,s->height,s->row_pitch))return GEO_INVALID;
 extent=(uint64_t)s->row_pitch*s->height;
 if(extent>A04_TEXTURE2D_MAX_BYTES||extent!=s->bytes||s->gpu<4096u||(s->gpu&3u)||s->gpu>=(UINT64_C(1)<<40)||extent>(UINT64_C(1)<<40)-s->gpu)return GEO_INVALID;
 /* IMAGE_WORD0: minus-one dimensions, U8U8U8U8, canonical RGBA sampling
  * from little-endian BGRA storage via (2,1,0,3), STRIDE layout (not twiddled).
  * STRIDE_IMAGE_WORD1 stride is pixels-1, no mip chain, one declared level. */
 word0=((uint64_t)(s->height-1u)<<48)|((uint64_t)(s->width-1u)<<34)|
       (UINT64_C(12)<<27)|(UINT64_C(1)<<11)|(UINT64_C(3)<<5)|4u;
 /* Existing BGRA8 uses2,1,0,3; RGBA8 uses0,1,2,3. Keep the memory
  * channel order in allocation metadata, not a CPU pixel swizzle. */
 if(s->format==A04_TEXTURE2D_BGRA8)word0|=UINT64_C(2)<<14;
 else word0|=UINT64_C(2)<<8;
 word1=(UINT64_C(1)<<60)|(UINT64_C(1)<<59)|((s->gpu>>2)<<16)|(s->row_pitch/4u-1u);
 /* A single mip always samples level0. Zero LOD bias = 4095, not raw0.
  * U/V/W CLAMP_TO_EDGE2, normalized coords, POINT min/mag/mip, no compare,
  * anisotropy/border/YUV planes absent. LOD bounds are both0. */
 sampler=(UINT64_C(2)<<56)|(UINT64_C(2)<<44)|(UINT64_C(2)<<41)|4095u;
 sampler|=(uint64_t)((filter>>4)&1u)<<38;sampler|=(uint64_t)((filter>>2)&1u)<<36;sampler|=(uint64_t)(filter&1u)<<40;sampler|=(uint64_t)aniso<<33;
 words(made.sampler,sampler,0);words(made.image,word0,word1);*out=made;return GEO_OK;
}

int32_t a04_m111_texture2d_state(struct a04_texture2d_state *out,const struct a04_texture2d_storage *s)
{return a04_m111_texture2d_state_filtered(out,s,0,1);}
