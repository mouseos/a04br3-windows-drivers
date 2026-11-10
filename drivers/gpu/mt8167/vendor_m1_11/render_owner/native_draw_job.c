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
#include "native_color_usc_generated.h"
#include "native_varying_pds.h"
#include "native_d3d_viewport.h"
#include "../../wddm-render/render_layout.h"
#include "../../wddm-render/viewport_profile.h"
#include "native_texture2d_state.h"
#include "native_texture2d_program.h"
static void put(uint8_t *p,unsigned at,uint64_t v,unsigned n)
{for(unsigned i=0;i<n;i++)p[at+i]=(uint8_t)(v>>(i*8));}
static void copy(void *to,const void *from,size_t n)
{for(size_t i=0;i<n;i++)((uint8_t *)to)[i]=((const uint8_t *)from)[i];}
static int same(const void *a,const void *b,size_t n)
{for(size_t i=0;i<n;i++)if(((const uint8_t *)a)[i]!=((const uint8_t *)b)[i])return 0;return 1;}
/* Official D3DCompile SM4 token streams, H63bbe0b verified inputs.
 * Admit this finite semantic program only; ABI2 uses a separately linked pair.
 * Words are read as LE bytes, independent of host alignment. */
static int sm4_profile(const struct geo_draw_shader *shader,const uint8_t *blob,int linked)
{
 static const uint32_t vs[]={0x10040u,0xfu,0x300005fu,0x1010f2u,0x0u,0x4000067u,0x1020f2u,0x0u,0x1u,0x5000036u,0x1020f2u,0x0u,0x101e46u,0x0u,0x100003eu};
 static const uint32_t ps[]={0x40u,0x10u,0x4000059u,0x208e46u,0x0u,0x1u,0x3000065u,0x1020f2u,0x0u,0x6000036u,0x1020f2u,0x0u,0x208e46u,0x0u,0x0u,0x100003eu};
 const uint32_t *words=linked?(shader->stage?a04_native_color_fs_tokens:a04_native_color_vs_tokens):(shader->stage?ps:vs);
 uint32_t count=linked?(shader->stage?(uint32_t)(sizeof(a04_native_color_fs_tokens)/4):(uint32_t)(sizeof(a04_native_color_vs_tokens)/4)):
 (shader->stage?(uint32_t)(sizeof(ps)/4):(uint32_t)(sizeof(vs)/4));
 if(shader->format!=GEO_SHADER_D3D10_TOKENS)return 1;
 if(shader->tokens.bytes!=count*4)return 0;
 const uint8_t *b=blob+shader->tokens.offset;
 for(uint32_t i=0;i<count;i++) {
  uint32_t w=(uint32_t)b[4*i]|((uint32_t)b[4*i+1]<<8)|((uint32_t)b[4*i+2]<<16)|((uint32_t)b[4*i+3]<<24);
  if(w!=words[i])return 0;
 }
 return 1;
}
static int overlap(uintptr_t a,size_t n,uintptr_t b,size_t m)
{return n>UINTPTR_MAX-a || m>UINTPTR_MAX-b || (a<b+m && b<a+n);}
static int32_t build(struct a04_m111_draw_job *out,
 const struct a04_m111_clear_job_input *in,uint64_t vertex,const struct geo_draw_request *r,
 const void *blob,uint32_t bytes,void *scratch,size_t scratch_bytes,const struct geo_draw_request_v2 *linked,const struct geo_draw_request_v3 *sampled,uint64_t texture,const struct geo_draw_request_v4 *general,const struct geo_draw_request_v5 *combined)
{
 /* Exact source-generated semantic programs from H63bbe0b official SM4->PCO
  * output. This finite allowlist admits no arbitrary memory/texture ISA. */
 static const uint8_t vs[24]={0x58,0xa0,0x06,0x08,0x00,0x80,0x04,0x00,0x00,0x30,0xf3,0xff,0xff,0xff,0xff,0xff,0x44,0xa0,0x80,0x05,0x00,0x00,0x00,0xff};
 static const uint8_t ps[64]={0x35,0x82,0x00,0x87,0x82,0x08,0x00,0x00,0x00,0x40,0x35,0x82,0x00,0x87,0x81,0x08,0x00,0x00,0x00,0x41,0x35,0x82,0x00,0x87,0x80,0x08,0x00,0x00,0x00,0x42,0x35,0x82,0x00,0x87,0x83,0x08,0x00,0x00,0x00,0x43,0x58,0x92,0x06,0x9c,0x20,0x80,0x40,0xa0,0x40,0x10,0x00,0x2c,0x44,0xff,0xf1,0xff,0x34,0x8a,0x80,0x87,0x44,0x00,0x00,0x20};
 static const uint32_t viewport[6]={0,0,0x44480000u,0x44160000u,0,0x3f800000u};
 const uint8_t *vc=linked?a04_native_color_vs:vs,*pc=linked?a04_native_color_fs:ps;
 uint32_t vn=linked?sizeof(a04_native_color_vs):sizeof(vs),pn=linked?sizeof(a04_native_color_fs):sizeof(ps);
 uint32_t vs_at=(general||combined)?0x400u:0x80u,ps_at=(general||combined)?0x800u:linked?0x100u:0xc0u,vertex_bytes=linked?96u:48u,vp_words[6];
 struct a04_m111_draw_job *m=scratch;struct a04_m111_triangle_suite *base;
 struct a04_m111_triangle_vertex_pds vp;uint32_t po;uint64_t bg[3];const uint8_t *b=blob;
 struct a04_texture2d_state texstate;
 if(!out || !in || !scratch || ((uintptr_t)scratch&7) || scratch_bytes<A04_M111_DRAW_SCRATCH ||
    overlap((uintptr_t)scratch,scratch_bytes,(uintptr_t)out,sizeof(*out)) ||
    overlap((uintptr_t)scratch,scratch_bytes,(uintptr_t)in,sizeof(*in)) ||
    overlap((uintptr_t)scratch,scratch_bytes,(uintptr_t)r,combined?sizeof(*combined):general?sizeof(*general):sampled?sizeof(*sampled):linked?sizeof(*linked):sizeof(*r)) ||
    overlap((uintptr_t)scratch,scratch_bytes,(uintptr_t)blob,bytes))return GEO_INVALID;
 int32_t status=combined?geo_draw_validate_capture_v5(combined,blob,bytes):general?geo_draw_validate_capture_v4(general,blob,bytes):sampled?geo_draw_validate_capture_v3(sampled,blob,bytes):linked?geo_draw_validate_capture_v2(linked,blob,bytes):geo_draw_validate_capture(r,blob,bytes);if(status)return status;
 if(sampled){
  struct a04_texture2d_storage t={sizeof(t),1,sampled->texture_desc.width,sampled->texture_desc.height,sampled->texture_desc.pitch,sampled->texture_desc.pitch*sampled->texture_desc.height,(sampled->texture_desc.format==GEO_FORMAT_A8B8G8R8?A04_TEXTURE2D_RGBA8:A04_TEXTURE2D_BGRA8),1,1,1,0,texture};
  status=a04_m111_texture2d_program(&r->ps,blob,bytes);if(status)return status;
  status=a04_m111_texture2d_state_filtered(&texstate,&t,sampled->sampler_profile==1?0:(sampled->sampler_profile>>8)&255u,sampled->sampler_profile==1?1:sampled->sampler_profile&255u);if(status)return status;
  if(texture<vertex+vertex_bytes&&vertex<texture+t.bytes)return GEO_INVALID;
  if((texture<in->output_gpu+(uint64_t)r->target_desc.pitch*r->target_desc.height&&in->output_gpu<texture+t.bytes)||
     (texture<in->data_page_gpu+4096&&in->data_page_gpu<texture+t.bytes)||
     (texture<in->usc_page_gpu+4096&&in->usc_page_gpu<texture+t.bytes)||
     (texture<in->pds_page_gpu+4096&&in->pds_page_gpu<texture+t.bytes))return GEO_INVALID;
  pc=b+r->ps.usc.offset;pn=r->ps.usc.bytes;
 }
 if(general){vc=b+r->vs.usc.offset;pc=b+r->ps.usc.offset;vn=r->vs.usc.bytes;pn=r->ps.usc.bytes;}
 else if((!combined&&!sm4_profile(&r->vs,b,linked!=NULL)) || (!sampled&&!sm4_profile(&r->ps,b,linked!=NULL)))return GEO_UNSUPPORTED;
 if(combined){vc=b+r->vs.usc.offset;vn=r->vs.usc.bytes;}
 if(linked) {
  struct a04_m111_varying_pds cp;
  status=a04_m111_build_varying_pds(&cp,&linked->link);if(status)return status;
  if(linked->link.fs_uses_z || linked->link.fs_uses_w!=1 || linked->link.fs_coeffs!=20 ||
     linked->link.vs_f32_smooth!=4 || linked->link.vs_f32_npc || linked->link.vs_entry_offset || linked->link.fs_entry_offset ||
     r->vs.format!=GEO_SHADER_D3D10_TOKENS || r->ps.format!=GEO_SHADER_D3D10_TOKENS)return GEO_UNSUPPORTED;
 }
 if(r->vs.format==GEO_SHADER_D3D10_TOKENS) {
  status=a04_m111_build_d3d_viewport(vp_words,r->state.viewport);if(status)return status;
 } else if(!same(r->state.viewport,viewport,sizeof(viewport)))return GEO_UNSUPPORTED;
 if(!geo_render_layout_valid(r->target_desc.width,r->target_desc.height,r->target_desc.pitch) ||
    (in->output_pitch_pixels?in->output_pitch_pixels:r->target_desc.width)!=r->target_desc.pitch/4u ||
    (r->vs.format==GEO_SHADER_D3D10_TOKENS&&!geo_viewport_target_valid(r->state.viewport,r->target_desc.width,r->target_desc.height)) ||
    r->state.vertex_count!=3 ||
    (!general&&!combined&&(r->vs.temps || r->vs.shareds || r->vs.vtxins!=(linked?8u:4u) || r->vs.vtxouts!=(linked?8u:4u) || r->vs.input_mask!=(linked?3u:1u) || r->vs.constant_mask ||
    r->ps.temps!=(sampled?A04_TEXTURE2D_SAMPLE_TEMPS:5u) || r->ps.shareds!=(sampled?12u:linked?0u:4u) || r->ps.vtxins || r->ps.vtxouts || r->ps.input_mask!=(linked?1u:0u) || r->ps.constant_mask!=(linked?0u:1u) ||
    r->vs.usc.bytes!=vn || r->ps.usc.bytes!=pn ||
    !same(b+r->vs.usc.offset,vc,vn) || !same(b+r->ps.usc.offset,pc,pn))))return GEO_UNSUPPORTED;
 if(combined&&(r->ps.temps!=A04_TEXTURE2D_SAMPLE_TEMPS||r->ps.shareds!=12u||r->ps.vtxins||r->ps.vtxouts||r->ps.input_mask||r->ps.constant_mask!=1u))return GEO_UNSUPPORTED;
 if(vertex<4096 || vertex>=(UINT64_C(1)<<40)-vertex_bytes || (vertex&3) ||
    (vertex<in->output_gpu+(uint64_t)r->target_desc.pitch*r->target_desc.height && in->output_gpu<vertex+vertex_bytes) ||
    (vertex<in->data_page_gpu+4096 && in->data_page_gpu<vertex+vertex_bytes) ||
    (vertex<in->usc_page_gpu+4096 && in->usc_page_gpu<vertex+vertex_bytes) ||
    (vertex<in->pds_page_gpu+4096 && in->pds_page_gpu<vertex+vertex_bytes))return GEO_INVALID;
 base=(void *)((uint8_t *)scratch+((sizeof(*m)+7u)&~7u));
 if(a04_m111_build_triangle_suite_dims(base,in,(void *)(((uintptr_t)(base+1)+7u)&~(uintptr_t)7u),sizeof(*base),r->target_desc.width,r->target_desc.height))return GEO_INVALID;
 for(unsigned i=0;i<sizeof(*m);i++)((uint8_t *)m)[i]=0;
 copy(m->ta,base->record[0].ta,sizeof(m->ta));copy(m->three,base->record[0].three,sizeof(m->three));
 copy(m->data,base->data,sizeof(base->data));copy(m->usc,base->usc,sizeof(base->usc));copy(m->pds,base->pds,sizeof(base->pds));
 {uint32_t clipped[4];
  if(geo_draw_scissor_bounds(&r->state,r->target_desc.width,r->target_desc.height,clipped)!=GEO_OK)return GEO_INVALID;
  /* Mesa rogue/ipf.xml: min in bits31:16, exclusive max in bits15:0.
   * The ISP scissor enable and PPP index already refer to this captured table. */
  for(unsigned i=0;i<2;i++){
   put(m->data,0x380+i*8,((uint64_t)clipped[0]<<16)|clipped[2],4);
   put(m->data,0x384+i*8,((uint64_t)clipped[1]<<16)|clipped[3],4);
  }
 }
 copy(m->usc+vs_at,vc,vn);copy(m->usc+ps_at,pc,pn);
 copy(m->usc+0x200,a04_native_load_usc,sizeof(a04_native_load_usc));
 if((general||combined)?a04_m111_build_program_vertex_pds(&vp,in->usc_page_gpu+vs_at,vertex,r->vs.temps):linked?a04_m111_build_linked_vertex_pds(&vp,in->usc_page_gpu+0x80,vertex,r->vs.temps):
    a04_m111_build_triangle_vertex_pds(&vp,in->usc_page_gpu+0x80,vertex,4,16))return GEO_INVALID;
 copy(m->pds+0x140,vp.data,32);copy(m->pds+0x160,vp.code,12);
 if((general||combined)&&r->vs.shareds){
  struct a04_m111_uniform_vertex_pds uvp;
  if(a04_m111_build_uniform_vertex_pds(&uvp,in->usc_page_gpu+vs_at,vertex,r->vs.temps,in->data_page_gpu+0x540,r->vs.shareds))return GEO_INVALID;
  /* Independent immutable VS constant copy, after all RT/EOT descriptors. */
  copy(m->data+0x540,b+r->vs.constants.offset,64);
  copy(m->pds+0x280,uvp.data,sizeof(uvp.data));copy(m->pds+0x2c0,uvp.code,sizeof(uvp.code));
  po=(uint32_t)(in->pds_page_gpu-UINT64_C(0xa800000000));
  put(m->data,10*4,po+0x280,4);put(m->data,11*4,po+0x2c0,4);
  /* vdm.xml: common64B, unified16B, temp16B, data48B, maxInstances16. */
  put(m->data,13*4,(16u<<25)|(1u<<16)|(1u<<10)|(1u<<6)|3u,4);
 }

 if(a04_m111_patch_background_pds(m->pds+0x180,36,in->usc_page_gpu+ps_at,r->ps.temps) ||
    a04_m111_patch_background_pds(m->pds+0x300,36,in->usc_page_gpu+0x200,A04_NATIVE_LOAD_TEMPS))return GEO_INVALID;
 if(general){copy(m->data+0x4a0,b+r->ps.constants.offset,64);put(m->pds,0x208,0x90000000u|(r->ps.shareds?r->ps.shareds:4u),4);}
 else if(sampled){copy(m->data+0x4a0,&texstate,sizeof(texstate));copy(m->data+0x4c0,b+r->ps.constants.offset,16);put(m->pds,0x208,0x9000000cu,4);}
 else{copy(m->data+0x4a0,b+r->ps.constants.offset,16);put(m->pds,0x208,0x90000004u,4);}
 if(linked) {
  struct a04_m111_varying_pds cp;
  struct a04_m111_triangle_ppp_input pp={0};
  if(a04_m111_build_varying_pds(&cp,&linked->link))return GEO_INVALID;
  for(uint32_t i=0;i<cp.data_dwords;i++)put(m->pds,0x380+4*i,cp.data[i],4);
  for(uint32_t i=0;i<cp.code_dwords;i++)put(m->pds,0x380+4*(cp.data_dwords+i),cp.code[i],4);
  pp.fragment_pds=(uint32_t)(in->pds_page_gpu-UINT64_C(0xa800000000))+0x180;
  pp.varying_pds=(uint32_t)(in->pds_page_gpu-UINT64_C(0xa800000000))+0x380;
  if(a04_m111_build_linked_triangle_ppp(m->data+0x240,104,&pp,&linked->link,0,0,r->ps.shareds))return GEO_INVALID;
  put(m->data,12*4,8,4); /* VDM_STATE4 main VS output:8 DWORD, source vdm.xml. */
  put(m->data,13*4,(16u<<25)|(2u<<10)|(1u<<6)|2u,4); /* input8 / unit4 */
 }
 /* PPP REGION_CLIP fields use native 16px tile coordinates (ppp.xml).
  * Match HWRT/terminate dimensions, including the general/linked PS path. */
 put(m->data,0x26c,(1u<<31)|((r->target_desc.width+15u)/16u-1u),4);
 put(m->data,0x270,(r->target_desc.height+15u)/16u-1u,4);
 /* Typed and old TL use distinct viewport conventions. */
 /* Typed SM4 consumes native D3D clip coordinates, unlike old SM2 TL
  * preconversion. D3D viewport: x=400*x/w+400,y=-300*y/w+300,z=z/w.
  * pvr_arch_cmd_buffer.c:7825 uses height/2 and maxDepth-minDepth;
  * D3D Y is the negative-height viewport equivalent. Keep format1 unchanged.
  * No CPU input-coordinate rewrite can preserve arbitrary VS semantics. */
 if(r->vs.format==GEO_SHADER_D3D10_TOKENS) {
  for(unsigned i=0;i<6;i++)put(m->data,0x274+4*i,vp_words[i],4);
 }
 put(m->data,0x500,(UINT64_C(1)<<49)|(UINT64_C(2)<<44)|(UINT64_C(2)<<41)|(UINT64_C(1)<<23)|0xfffu,8);
 put(m->data,0x510,((uint64_t)(r->target_desc.height-1u)<<48)|((uint64_t)(r->target_desc.width-1u)<<34)|(UINT64_C(12)<<27)|(UINT64_C(1)<<11)|(UINT64_C(2)<<8)|(UINT64_C(3)<<5)|4u,8);
 put(m->data,0x518,(UINT64_C(1)<<60)|(UINT64_C(1)<<59)|((in->output_gpu>>2)<<16)|(r->target_desc.pitch/4u-1u),8);
 put(m->pds,0x340,in->data_page_gpu+0x500,8);put(m->pds,0x348,0x90000008u,4);
 put(m->pds,0x350,(0xfu<<28)|(1u<<26)|(2u<<16),4);
 po=(uint32_t)(in->pds_page_gpu-UINT64_C(0xa800000000));
 bg[0]=((uint64_t)(po+0x350)<<32)|(po+0x300);bg[1]=(uint64_t)(po+0x340)<<32;
 bg[2]=(UINT64_C(1)<<55)|(UINT64_C(1)<<16)|2u;
 if(a04_m111_patch_3d_background(m->three,sizeof(m->three),bg,bg))return GEO_INVALID;
 copy(out,m,sizeof(*out));return GEO_OK;
}

int32_t a04_m111_build_draw_job(struct a04_m111_draw_job *out,
 const struct a04_m111_clear_job_input *in,uint64_t vertex,const struct geo_draw_request *r,
 const void *blob,uint32_t bytes,void *scratch,size_t scratch_bytes)
{ return build(out,in,vertex,r,blob,bytes,scratch,scratch_bytes,NULL,NULL,0,NULL,NULL); }
int32_t a04_m111_build_draw_job_v2(struct a04_m111_draw_job *out,
 const struct a04_m111_clear_job_input *in,uint64_t vertex,const struct geo_draw_request_v2 *r,
 const void *blob,uint32_t bytes,void *scratch,size_t scratch_bytes)
{
 if(!r)return GEO_INVALID;
 return build(out,in,vertex,&r->base,blob,bytes,scratch,scratch_bytes,r,NULL,0,NULL,NULL);
}

int32_t a04_m111_build_draw_job_v3(struct a04_m111_draw_job *out,const struct a04_m111_clear_job_input *in,uint64_t vertex,uint64_t texture,const struct geo_draw_request_v3 *r,const void *blob,uint32_t bytes,void *scratch,size_t capacity) {
 if(!r)return GEO_INVALID;
 return build(out,in,vertex,&r->base,blob,bytes,scratch,capacity,NULL,r,texture,NULL,NULL);
}

int32_t a04_m111_build_draw_job_v4(struct a04_m111_draw_job *out,const struct a04_m111_clear_job_input *in,uint64_t vertex,const struct geo_draw_request_v4 *r,const void *blob,uint32_t bytes,void *scratch,size_t capacity)
{if(!r)return GEO_INVALID;return build(out,in,vertex,&r->base,blob,bytes,scratch,capacity,NULL,NULL,0,r,NULL);}

int32_t a04_m111_build_draw_job_v5(struct a04_m111_draw_job *out,const struct a04_m111_clear_job_input *in,uint64_t vertex,uint64_t texture,const struct geo_draw_request_v5 *r,const void *blob,uint32_t bytes,void *scratch,size_t capacity)
{if(!r)return GEO_INVALID;return build(out,in,vertex,&r->sampled.base,blob,bytes,scratch,capacity,NULL,&r->sampled,texture,NULL,r);}
