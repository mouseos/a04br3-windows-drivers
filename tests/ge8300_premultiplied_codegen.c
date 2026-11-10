/* SPDX-License-Identifier: MIT. Production compiler variants; no GPU access. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-render/compiler/pair_compiler.h"
#include "../drivers/gpu/mt8167/wddm-render/compiler/texture2d_sm4.h"
#include "../drivers/gpu/mt8167/wddm-render/frontend/shader_witness.h"
#include "../drivers/gpu/mt8167/vendor_m1_11/render_owner/native_color_usc_generated.h"
#include "../drivers/gpu/mt8167/vendor_m1_11/render_owner/native_draw_job.h"
static void capture_shader(unsigned char *blob,uint32_t *at,struct geo_draw_shader *s,struct geo_draw_span *proof,
 const struct geo_compiled_shader *c,const uint32_t *tokens,uint32_t bytes,const void *w,uint32_t wb){
 s->format=GEO_SHADER_D3D10_TOKENS;s->stage=c->stage;s->temps=c->temps;s->shareds=c->shareds;s->vtxins=c->vtxins;s->vtxouts=c->vtxouts;s->input_mask=c->input_mask;s->constant_mask=c->constant_mask;
 s->tokens=(struct geo_draw_span){*at,bytes};memcpy(blob+*at,tokens,bytes);*at+=bytes;
 s->usc=(struct geo_draw_span){*at,c->code_bytes};memcpy(blob+*at,c->code,c->code_bytes);*at+=c->code_bytes;
 s->constants=(struct geo_draw_span){*at,512};memset(blob+*at,0,512);*at+=512;
 if(proof){*proof=(struct geo_draw_span){*at,wb};memcpy(blob+*at,w,wb);*at+=wb;}
}
static void native_contract(struct geo_shader_compiler *c,const struct geo_compiled_shader *ps,const uint32_t *pt,uint32_t pb,const void *pw,uint32_t pwb,const struct geo_compiled_pair *pair){
 const uint32_t vt[]={0x10040,15,0x300005f,0x1010f2,0,0x4000067,0x1020f2,0,1,0x5000036,0x1020f2,0,0x101e46,0,0x100003e};
 struct geo_sm4_signatures sig={0};sig.input_count=sig.output_count=1;sig.inputs[0].mask=sig.outputs[0].mask=15;sig.outputs[0].system_value=1;
 struct geo_compiled_shader vs={0};struct geo_sm2_error e;void*vw=NULL;uint32_t vwb=0;
 if(!pair)assert(!geo_pco_compile_tokenized_witness(c->context,0,vt,sizeof(vt),&sig,&vs,&e,&vw,&vwb));
 unsigned char *blob=calloc(1,65536);void *scratch=malloc(A04_M111_DRAW_SCRATCH);struct a04_m111_draw_job *job=malloc(sizeof(*job)),*old=malloc(sizeof(*old));assert(blob&&scratch&&job&&old);uint32_t at=0;
 struct geo_draw_request_v4 r4={0};struct geo_draw_request_v5 r5={0};struct geo_draw_request_v2 r2={0};
 struct geo_draw_request *r=pair?&r2.base:pw?&r4.base:&r5.sampled.base;
 struct geo_draw_span *vp=pair?NULL:pw?&r4.witness[0]:&r5.vertex_witness,*pp=pw?&r4.witness[1]:NULL;
 capture_shader(blob,&at,&r->vs,vp,pair?&pair->vs:&vs,pair?a04_native_color_vs_tokens:vt,pair?sizeof(a04_native_color_vs_tokens):sizeof(vt),vw,vwb);
 capture_shader(blob,&at,&r->ps,pp,ps,pt,pb,pw,pwb);
 r->version=pair?2:pw?4:5;r->size=pair?sizeof(r2):pw?sizeof(r4):sizeof(r5);r->blob_bytes=at;r->target=1;r->vertex=2;
 r->target_desc=(struct geo_surface_desc){sizeof(r->target_desc),1,1280,800,5120,GEO_FORMAT_A8R8G8B8,GEO_USAGE_RT_TEXTURE2D,0};
 r->state.topology=GEO_DRAW_TRIANGLE_LIST;r->state.vertex_count=3;r->state.vertex_stride=pair?32:16;r->state.color_write_mask=15;r->state.blend_enable=1;
 r->state.viewport[2]=0x44a00000;r->state.viewport[3]=0x44480000;r->state.viewport[5]=0x3f800000;
 r->state.scissor_enable=1;r->state.scissor[0]=13;r->state.scissor[1]=17;r->state.scissor[2]=317;r->state.scissor[3]=199;
 if(pair)r2.link=pair->link;
 else if(pw)r4.isa=GEO_SHADER_WITNESS_ISA;
 else{r5.isa=GEO_SHADER_WITNESS_ISA;r5.sampled.texture=3;r5.sampled.sampler_profile=1;r5.sampled.texture_desc=(struct geo_surface_desc){sizeof(struct geo_surface_desc),GEO_TEXTURE_STORAGE_VERSION,2,2,64,GEO_FORMAT_A8R8G8B8,GEO_USAGE_TEXTURE2D,0};}
 struct a04_m111_clear_job_input in={0};in.output_gpu=0x7000000000ULL;in.output_pitch_pixels=1280;in.data_page_gpu=0x65c0010000ULL;in.usc_page_gpu=0xba00000000ULL;in.pds_page_gpu=0xa800000000ULL;in.dummy_region_gpu=0x65c0020000ULL;in.dummy_stencil_gpu=0x65c0021000ULL;in.hwrt_fw=0xc0088000;in.pr_fence_fw=0xc0084c00;
 int32_t status=pair?a04_m111_build_draw_job_v2(job,&in,0x7000800000ULL,&r2,blob,at,scratch,A04_M111_DRAW_SCRATCH):pw?a04_m111_build_draw_job_v4(job,&in,0x7000800000ULL,&r4,blob,at,scratch,A04_M111_DRAW_SCRATCH):a04_m111_build_draw_job_v5(job,&in,0x7000800000ULL,0x7000900000ULL,&r5,blob,at,scratch,A04_M111_DRAW_SCRATCH);
 printf("native blend kind%u status%08x\n",r->version,(unsigned)status);fflush(stdout);assert(!status);
 uint32_t ispa=0;memcpy(&ispa,job->data+0x248,4);assert(((ispa>>24)&7)==1);*old=*job;
 r->state.blend_enable=0;
 status=pair?a04_m111_build_draw_job_v2(job,&in,0x7000800000ULL,&r2,blob,at,scratch,A04_M111_DRAW_SCRATCH):pw?a04_m111_build_draw_job_v4(job,&in,0x7000800000ULL,&r4,blob,at,scratch,A04_M111_DRAW_SCRATCH):a04_m111_build_draw_job_v5(job,&in,0x7000800000ULL,0x7000900000ULL,&r5,blob,at,scratch,A04_M111_DRAW_SCRATCH);
 assert(status&&!memcmp(old,job,sizeof(*job)));
 if(!pair)c->release(c->context,&vs);free(blob);free(scratch);free(job);free(old);
}
static void save(const char *base,const char *name,const struct geo_compiled_shader *s){
 char path[4096];assert(snprintf(path,sizeof(path),"%s/%s.usc",base,name)>0);FILE*f=fopen(path,"wb");assert(f);assert(fwrite(s->code,1,s->code_bytes,f)==s->code_bytes);assert(!fclose(f));
 printf("%s bytes%u temps%u shared%u inputs%u constants%u\n",name,s->code_bytes,s->temps,s->shareds,s->input_mask,s->constant_mask);
}
int main(int argc,char **argv){
 assert(argc==2);struct geo_shader_compiler c;void*owned=NULL;assert(!geo_pco_device_create(0x001600280036001eULL,&c,&owned));
 struct geo_sm4_signatures sig={0};sig.output_count=1;sig.outputs[0].mask=15;
 const uint32_t constant[]={0x40,0x10,0x4000059,0x208e46,0,1,0x3000065,0x1020f2,0,0x6000036,0x1020f2,0,0x208e46,0,0,0x100003e};
 struct geo_compiled_shader ps={0};struct geo_sm2_error error;void*w=NULL;uint32_t wb=0;
 int32_t status=geo_pco_compile_tokenized_blend_witness(c.context,1,constant,sizeof(constant),&sig,&ps,&error,&w,&wb,1);
 printf("general blend status%08x\n",(unsigned)status);fflush(stdout);assert(!status);
 struct geo_shader_metadata m={1,ps.temps,ps.shareds,0,0,ps.input_mask,ps.constant_mask,1};
 assert(!geo_shader_witness_validate(w,wb,ps.code,ps.code_bytes,&m));m.framebuffer_fetch=0;assert(geo_shader_witness_validate(w,wb,ps.code,ps.code_bytes,&m));save(argv[1],"constant-blend",&ps);native_contract(&c,&ps,constant,sizeof(constant),w,wb,NULL);c.release(c.context,&ps);
 const uint32_t sample[]={0x40,0x1b,0x4000059,0x208e46,0,1,0x300005a,0x106000,0,0x4001858,0x107000,0,0x5555,0x3000065,0x1020f2,0,0xa000045,0x1020f2,0,0x208046,0,0,0x107e46,0,0x106000,0,0x100003e};
 struct geo_texture2d_program binding;assert(!geo_pco_compile_texture2d_blend(c.context,sample,sizeof(sample),&sig,&ps,&binding,1));save(argv[1],"texture-blend",&ps);native_contract(&c,&ps,sample,sizeof(sample),NULL,0,NULL);c.release(c.context,&ps);
 struct geo_sm4_signatures vsig={0},fsig={0};vsig.input_count=vsig.output_count=2;
 for(unsigned i=0;i<2;i++){vsig.inputs[i].reg=vsig.outputs[i].reg=i;vsig.inputs[i].mask=vsig.outputs[i].mask=15;}
 vsig.outputs[0].system_value=1;fsig.input_count=fsig.output_count=1;fsig.inputs[0].mask=fsig.outputs[0].mask=15;
 struct geo_compiled_pair pair={0};assert(!geo_pco_compile_pair_blend(c.context,a04_native_color_vs_tokens,sizeof(a04_native_color_vs_tokens),&vsig,a04_native_color_fs_tokens,sizeof(a04_native_color_fs_tokens),&fsig,&pair,1));
 save(argv[1],"pair-blend",&pair.ps);native_contract(&c,&pair.ps,a04_native_color_fs_tokens,sizeof(a04_native_color_fs_tokens),NULL,0,&pair);c.release(c.context,&pair.vs);c.release(c.context,&pair.ps);
 geo_pco_device_destroy(&c,owned);puts("premultiplied compiler variants and framebuffer-fetch witness PASS; hardware unverified");return 0;
}
