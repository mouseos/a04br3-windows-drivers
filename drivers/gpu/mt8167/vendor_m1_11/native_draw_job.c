/* SPDX-License-Identifier: MIT
 * PPP/VDM/PDS encoders reuse source-proven capture06 field layouts.
 * Background loader is generated from Mesa pvr_uscgen_loadop source, not a
 * captured USC blob. Texture stride/address layout: rogue/texstate.xml and
 * pvr_arch_tex_state.c. H lowers TL to the proven native Y-down/GL-Z profile.
 */
#include "native_draw_job.h"
#include "native_triangle_pds.h"
#include "native_clear_pds.h"
#include "native_load_usc_generated.h"
static void put(uint8_t *p,unsigned at,uint64_t v,unsigned n)
{for(unsigned i=0;i<n;i++)p[at+i]=(uint8_t)(v>>(i*8));}
static void copy(void *to,const void *from,size_t n)
{for(size_t i=0;i<n;i++)((uint8_t *)to)[i]=((const uint8_t *)from)[i];}
static int same(const void *a,const void *b,size_t n)
{for(size_t i=0;i<n;i++)if(((const uint8_t *)a)[i]!=((const uint8_t *)b)[i])return 0;return 1;}
static int overlap(uintptr_t a,size_t n,uintptr_t b,size_t m)
{return n>UINTPTR_MAX-a || m>UINTPTR_MAX-b || (a<b+m && b<a+n);}
int32_t a04_m111_build_draw_job(struct a04_m111_draw_job *out,
 const struct a04_m111_clear_job_input *in,uint64_t vertex,const struct geo_draw_request *r,
 const void *blob,uint32_t bytes,void *scratch,size_t scratch_bytes)
{
 /* Exact source-generated semantic programs from H63bbe0b official SM4->PCO
  * output. This finite allowlist admits no arbitrary memory/texture ISA. */
 static const uint8_t vs[24]={0x58,0xa0,0x06,0x08,0x00,0x80,0x04,0x00,0x00,0x30,0xf3,0xff,0xff,0xff,0xff,0xff,0x44,0xa0,0x80,0x05,0x00,0x00,0x00,0xff};
 static const uint8_t ps[64]={0x35,0x82,0x00,0x87,0x82,0x08,0x00,0x00,0x00,0x40,0x35,0x82,0x00,0x87,0x81,0x08,0x00,0x00,0x00,0x41,0x35,0x82,0x00,0x87,0x80,0x08,0x00,0x00,0x00,0x42,0x35,0x82,0x00,0x87,0x83,0x08,0x00,0x00,0x00,0x43,0x58,0x92,0x06,0x9c,0x20,0x80,0x40,0xa0,0x40,0x10,0x00,0x2c,0x44,0xff,0xf1,0xff,0x34,0x8a,0x80,0x87,0x44,0x00,0x00,0x20};
 static const uint32_t viewport[6]={0,0,0x44480000u,0x44160000u,0,0x3f800000u};
 struct a04_m111_draw_job *m=scratch;struct a04_m111_triangle_suite *base;
 struct a04_m111_triangle_vertex_pds vp;uint32_t po;uint64_t bg[3];const uint8_t *b=blob;
 if(!out || !in || !scratch || ((uintptr_t)scratch&7) || scratch_bytes<A04_M111_DRAW_SCRATCH ||
    overlap((uintptr_t)scratch,scratch_bytes,(uintptr_t)out,sizeof(*out)) ||
    overlap((uintptr_t)scratch,scratch_bytes,(uintptr_t)in,sizeof(*in)) ||
    overlap((uintptr_t)scratch,scratch_bytes,(uintptr_t)r,sizeof(*r)) ||
    overlap((uintptr_t)scratch,scratch_bytes,(uintptr_t)blob,bytes))return GEO_INVALID;
 int32_t status=geo_draw_validate_capture(r,blob,bytes);if(status)return status;
 if(r->target_desc.width!=800 || r->target_desc.height!=600 || r->target_desc.pitch!=3200 ||
    r->state.vertex_count!=3 || !same(r->state.viewport,viewport,sizeof(viewport)) ||
    r->vs.temps || r->vs.shareds || r->vs.vtxins!=4 || r->vs.vtxouts!=4 || r->vs.input_mask!=1 || r->vs.constant_mask ||
    r->ps.temps!=5 || r->ps.shareds!=4 || r->ps.vtxins || r->ps.vtxouts || r->ps.input_mask || r->ps.constant_mask!=1 ||
    r->vs.usc.bytes!=sizeof(vs) || r->ps.usc.bytes!=sizeof(ps) ||
    !same(b+r->vs.usc.offset,vs,sizeof(vs)) || !same(b+r->ps.usc.offset,ps,sizeof(ps)))return GEO_UNSUPPORTED;
 if(vertex<4096 || vertex>=(UINT64_C(1)<<40)-48 || (vertex&3) ||
    (vertex<in->output_gpu+1920000u && in->output_gpu<vertex+48u) ||
    (vertex<in->data_page_gpu+4096 && in->data_page_gpu<vertex+48u) ||
    (vertex<in->usc_page_gpu+4096 && in->usc_page_gpu<vertex+48u) ||
    (vertex<in->pds_page_gpu+4096 && in->pds_page_gpu<vertex+48u))return GEO_INVALID;
 base=(void *)((uint8_t *)scratch+((sizeof(*m)+7u)&~7u));
 if(a04_m111_build_triangle_suite(base,in,(void *)(((uintptr_t)(base+1)+7u)&~(uintptr_t)7u),sizeof(*base)))return GEO_INVALID;
 for(unsigned i=0;i<sizeof(*m);i++)((uint8_t *)m)[i]=0;
 copy(m->ta,base->record[0].ta,sizeof(m->ta));copy(m->three,base->record[0].three,sizeof(m->three));
 copy(m->data,base->data,sizeof(base->data));copy(m->usc,base->usc,sizeof(base->usc));copy(m->pds,base->pds,sizeof(base->pds));
 copy(m->usc+0x80,vs,sizeof(vs));copy(m->usc+0xc0,ps,sizeof(ps));
 copy(m->usc+0x200,a04_native_load_usc,sizeof(a04_native_load_usc));
 if(a04_m111_build_triangle_vertex_pds(&vp,in->usc_page_gpu+0x80,vertex,4,16))return GEO_INVALID;
 copy(m->pds+0x140,vp.data,32);copy(m->pds+0x160,vp.code,12);
 if(a04_m111_patch_background_pds(m->pds+0x180,36,in->usc_page_gpu+0xc0,5) ||
    a04_m111_patch_background_pds(m->pds+0x300,36,in->usc_page_gpu+0x200,A04_NATIVE_LOAD_TEMPS))return GEO_INVALID;
 copy(m->data+0x4a0,b+r->ps.constants.offset,16);put(m->pds,0x208,0x90000004u,4);
 /* Four output registers; no interpolated user varyings/texture/depth state. */
 /* Keep proven +400/+300/.5 viewport scales/offsets. H's TL lowering
  * supplies native clip coordinates including D3D9 half-pixel adjustment. */
 put(m->data,0x500,(UINT64_C(1)<<49)|(UINT64_C(2)<<44)|(UINT64_C(2)<<41)|(UINT64_C(1)<<23)|0xfffu,8);
 put(m->data,0x510,(UINT64_C(599)<<48)|(UINT64_C(799)<<34)|(UINT64_C(12)<<27)|(UINT64_C(1)<<11)|(UINT64_C(2)<<8)|(UINT64_C(3)<<5)|4u,8);
 put(m->data,0x518,(UINT64_C(1)<<60)|(UINT64_C(1)<<59)|((in->output_gpu>>2)<<16)|799u,8);
 put(m->pds,0x340,in->data_page_gpu+0x500,8);put(m->pds,0x348,0x90000008u,4);
 put(m->pds,0x350,(0xfu<<28)|(1u<<26)|(2u<<16),4);
 po=(uint32_t)(in->pds_page_gpu-UINT64_C(0xa800000000));
 bg[0]=((uint64_t)(po+0x350)<<32)|(po+0x300);bg[1]=(uint64_t)(po+0x340)<<32;
 bg[2]=(UINT64_C(1)<<55)|(UINT64_C(1)<<16)|2u;
 if(a04_m111_patch_3d_background(m->three,sizeof(m->three),bg,bg))return GEO_INVALID;
 copy(out,m,sizeof(*out));return GEO_OK;
}
