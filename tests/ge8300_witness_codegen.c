/* SPDX-License-Identifier: MIT. Real source compiler -> native immutable capture.
 * No GPU, driver load, fixed USC fixture, or substituted shader is used. */
#include <assert.h>
#include "../drivers/gpu/mt8167/wddm-render/compiler/texture2d_sm4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-render/compiler/shader_compiler.h"
#include "../drivers/gpu/mt8167/vendor_m1_11/render_owner/native_draw_job.h"
static uint32_t vs[]={
 0x10040,46,0x4000059,0x208e46,0,4,
 0x300005f,0x1010f2,0,0x4000067,0x1020f2,0,1,
 0x8000011,0x102012,0,0x101e46,0,0x208e46,0,0,
 0x8000011,0x102022,0,0x101e46,0,0x208e46,0,1,
 0x8000011,0x102042,0,0x101e46,0,0x208e46,0,2,
 0x8000011,0x102082,0,0x101e46,0,0x208e46,0,3,
 0x100003e};

static uint32_t ps[]={0x40,21,0x4000059,0x208e46,0,1,0x3000065,0x1020f2,0,0xb000038,0x1020f2,0,0x208e46,0,0,0x4002,0x3ec00000,0x3f200000,0x3f400000,0x3f800000,0x100003e};
static void packet_shader(struct geo_draw_shader *s,const struct geo_compiled_shader *c,uint32_t *t,uint32_t bytes,const void *w,uint32_t wb,struct geo_draw_span *proof,unsigned char *blob,uint32_t *at){
 s->format=2;s->stage=c->stage;s->temps=c->temps;s->shareds=c->shareds;s->vtxins=c->vtxins;s->vtxouts=c->vtxouts;s->input_mask=c->input_mask;s->constant_mask=c->constant_mask;
 s->tokens=(struct geo_draw_span){*at,bytes};memcpy(blob+*at,t,bytes);*at+=bytes;
 s->usc=(struct geo_draw_span){*at,c->code_bytes};memcpy(blob+*at,c->code,c->code_bytes);*at+=c->code_bytes;
 s->constants=(struct geo_draw_span){*at,512};memset(blob+*at,0,512);*at+=512;
 *proof=(struct geo_draw_span){*at,wb};if(wb)memcpy(blob+*at,w,wb);*at+=wb;
}
int geo_witness_ffsll(long long);
int main(void){
 assert(!geo_witness_ffsll(0));for(unsigned bit=0;bit<64;bit++)assert(geo_witness_ffsll((long long)(UINT64_C(1)<<bit))==(int)bit+1);
 assert(geo_witness_ffsll(-1)==1);
 struct geo_shader_compiler compiler;void *owner=NULL,*vproof=NULL,*pproof=NULL;uint32_t vbytes=0,pbytes=0;struct geo_compiled_shader v,p;struct geo_sm2_error e;struct geo_sm4_signatures sig={0};int32_t status;
 assert(!geo_pco_device_create(UINT64_C(0x001600280036001e),&compiler,&owner));
 sig.input_count=sig.output_count=1;sig.inputs[0].mask=sig.outputs[0].mask=15;sig.outputs[0].system_value=1;
 status=geo_pco_compile_tokenized_witness(compiler.context,0,vs,sizeof(vs),&sig,&v,&e,&vproof,&vbytes);printf("VS%x bytes%u\n",status,vbytes);fflush(stdout);assert(!status);
 memset(&sig,0,sizeof(sig));sig.output_count=1;sig.outputs[0].mask=15;
 status=geo_pco_compile_tokenized_witness(compiler.context,1,ps,sizeof(ps),&sig,&p,&e,&pproof,&pbytes);printf("PS%x bytes%u\n",status,pbytes);fflush(stdout);assert(!status);
 unsigned char *blob=calloc(1,65536);assert(blob);uint32_t at=0;struct geo_draw_request_v4 request={0};
 packet_shader(&request.base.vs,&v,vs,sizeof(vs),vproof,vbytes,&request.witness[0],blob,&at);
 packet_shader(&request.base.ps,&p,ps,sizeof(ps),pproof,pbytes,&request.witness[1],blob,&at);
 /* Identity/nonuniform transform rows are actual captured CB bytes. */
 uint32_t matrix[16]={0x3f000000,0,0,0,0,0x3f400000,0,0,0,0,0x3f800000,0,0,0,0,0x3f800000};
 memcpy(blob+request.base.vs.constants.offset,matrix,sizeof(matrix));
 assert(v.constant_mask==15&&v.shareds==16);
 request.base.size=sizeof(request);request.base.version=4;request.base.blob_bytes=at;request.base.target=1;request.base.vertex=2;request.isa=GEO_SHADER_WITNESS_ISA;
 request.base.target_desc=(struct geo_surface_desc){sizeof(struct geo_surface_desc),1,800,600,3200,GEO_FORMAT_A8R8G8B8,GEO_USAGE_RENDER_TARGET,0};
 request.base.state.topology=GEO_DRAW_TRIANGLE_LIST;request.base.state.vertex_count=3;request.base.state.vertex_stride=16;request.base.state.color_write_mask=15;
 request.base.state.viewport[2]=0x44480000;request.base.state.viewport[3]=0x44160000;request.base.state.viewport[5]=0x3f800000;
 assert(!geo_draw_validate_capture_v4(&request,blob,at));
 struct a04_m111_clear_job_input input={0};input.output_gpu=0x7000000000ULL;input.data_page_gpu=0x65c0010000ULL;input.usc_page_gpu=0xba00000000ULL;input.pds_page_gpu=0xa800000000ULL;input.dummy_region_gpu=0x65c0020000ULL;input.dummy_stencil_gpu=0x65c0021000ULL;input.hwrt_fw=0xc0088000;input.pr_fence_fw=0xc0084c00;
 struct a04_m111_draw_job *job=malloc(sizeof(*job)),*prior=malloc(sizeof(*job));void *scratch=malloc(A04_M111_DRAW_SCRATCH);assert(job&&prior&&scratch);
 assert(!a04_m111_build_draw_job_v4(job,&input,0x7000200000ULL,&request,blob,at,scratch,A04_M111_DRAW_SCRATCH));
 assert(!memcmp(job->data+0x540,matrix,64));
 assert(job->pds[0x2c0]==0&&job->pds[0x2c1]==4&&job->pds[0x2c2]==10&&job->pds[0x2c3]==0xf0);
 assert(job->pds[0x2a8]==16&&job->pds[0x2ab]==0x10); /* no LAST on CB; vertex LAST retained */
 assert(job->pds[0x298]==4&&job->pds[0x29b]==0x80);
 assert(!memcmp(job->usc+0x400,v.code,v.code_bytes)&&!memcmp(job->usc+0x800,p.code,p.code_bytes));*prior=*job;
 /* These genuine MUL programs differ from every old passthrough profile. */
 assert(a04_m111_build_draw_job(job,&input,0x7000200000ULL,&request.base,blob,at,scratch,A04_M111_DRAW_SCRATCH));
 for(unsigned stage=0;stage<2;stage++){
  struct geo_draw_shader *s=stage?&request.base.ps:&request.base.vs;
  for(unsigned i=0;i<s->usc.bytes;i++){blob[s->usc.offset+i]^=1;assert(geo_draw_validate_capture_v4(&request,blob,at));blob[s->usc.offset+i]^=1;}
  uint32_t saved=s->temps;s->temps=0;assert(geo_draw_validate_capture_v4(&request,blob,at));s->temps=saved;
 }
 struct geo_shader_witness *w=(void *)(blob+request.witness[1].offset);struct geo_shader_insn *rows=(void *)(w+1);uint32_t op=rows[0].opcode;rows[0].opcode=33;assert(geo_draw_validate_capture_v4(&request,blob,at));rows[0].opcode=op;
 w->isa++;assert(a04_m111_build_draw_job_v4(job,&input,0x7000200000ULL,&request,blob,at,scratch,A04_M111_DRAW_SCRATCH));assert(!memcmp(prior,job,sizeof(*job)));w->isa--;
 request.base.target=request.base.vertex;assert(geo_draw_validate_capture_v4(&request,blob,at));request.base.target=1;
 assert(!geo_draw_validate_capture_v4(&request,blob,at));
 /* Malformed pointer-free IR is rejected without assertion/heap/kernel IO.
  * Mutations include opcode/modifier/register/unused fields, not GPU trials. */
 unsigned rejected=0,seed=0x329;
 for(unsigned n=0;n<6000;n++){
  seed=seed*1664525u+1013904223u;uint32_t where=seed%request.witness[1].bytes;
  unsigned char *byte=blob+request.witness[1].offset+where;unsigned char save=*byte;
  *byte^=(unsigned char)(1u<<(n%8));if(geo_draw_validate_capture_v4(&request,blob,at))rejected++;*byte=save;
 }
 assert(rejected>5700);printf("bounded witness mutations rejected %u/6000 (no GPU)\n",rejected);
 printf("REAL CB MATRIX VS%u/PS%u USC bytes -> ABI4 -> native byte-exact copy/PDS; changed USC, forbidden op, undersized temp, ISA and alias rejected\n",v.code_bytes,p.code_bytes);
 /* ABI5: the same real matrix VS plus a source-compiled SAMPLE PS. */
 const uint32_t tex_tokens[]={0x40,27,0x4000059,0x208e46,0,1,0x300005a,0x106000,0,0x4001858,0x107000,0,0x5555,0x3000065,0x1020f2,0,0xa000045,0x1020f2,0,0x208046,0,0,0x107e46,0,0x106000,0,0x100003e};
 struct geo_texture2d_program texture_program;struct geo_compiled_shader tex_ps;
 assert(!geo_pco_compile_texture2d(compiler.context,tex_tokens,sizeof(tex_tokens),&sig,&tex_ps,&texture_program));
 struct geo_draw_request_v5 combined={0};combined.sampled.base=request.base;combined.vertex_witness=request.witness[0];combined.isa=GEO_SHADER_WITNESS_ISA;
 combined.sampled.base.size=sizeof(combined);combined.sampled.base.version=5;
 struct geo_draw_span unused;
 packet_shader(&combined.sampled.base.ps,&tex_ps,(uint32_t *)tex_tokens,sizeof(tex_tokens),NULL,0,&unused,blob,&at);
 combined.sampled.base.blob_bytes=at;combined.sampled.texture=3;
 combined.sampled.texture_desc=(struct geo_surface_desc){sizeof(struct geo_surface_desc),2,2,2,8,GEO_FORMAT_A8R8G8B8,GEO_USAGE_TEXTURE2D,0};combined.sampled.sampler_profile=GEO_TEXTURE_FILTER_PROFILE4|(0x15u<<8)|1u;
 assert(!geo_draw_validate_capture_v5(&combined,blob,at));
 assert(!a04_m111_build_draw_job_v5(job,&input,0x7000200000ULL,0x7000210000ULL,&combined,blob,at,scratch,A04_M111_DRAW_SCRATCH));
 assert(!memcmp(job->data+0x540,matrix,64)&&!memcmp(job->usc+0x400,v.code,v.code_bytes)&&!memcmp(job->usc+0x800,tex_ps.code,tex_ps.code_bytes));
 assert(job->pds[0x208]==12&&job->pds[0x20b]==0x90&&job->pds[0x2ab]==0x10);*prior=*job;
 combined.isa++;assert(a04_m111_build_draw_job_v5(job,&input,0x7000200000ULL,0x7000210000ULL,&combined,blob,at,scratch,A04_M111_DRAW_SCRATCH)&&!memcmp(prior,job,sizeof(*job)));combined.isa--;
 combined.sampled.texture=combined.sampled.base.target;assert(geo_draw_validate_capture_v5(&combined,blob,at));combined.sampled.texture=3;
 for(unsigned i=0;i<tex_ps.code_bytes;i++) {blob[combined.sampled.base.ps.usc.offset+i]^=1;assert(a04_m111_build_draw_job_v5(job,&input,0x7000200000ULL,0x7000210000ULL,&combined,blob,at,scratch,A04_M111_DRAW_SCRATCH));assert(!memcmp(prior,job,sizeof(*job)));blob[combined.sampled.base.ps.usc.offset+i]^=1;}
 combined.reserved=1;assert(geo_draw_validate_capture_v5(&combined,blob,at));combined.reserved=0;
 combined.vertex_witness.bytes--;assert(geo_draw_validate_capture_v5(&combined,blob,at));combined.vertex_witness.bytes++;
 combined.sampled.base.version=4;assert(geo_draw_validate_capture_v5(&combined,blob,at));combined.sampled.base.version=5;
 assert(!a04_m111_build_draw_job_v5(job,&input,0x7000200000ULL,0x7000210000ULL,&combined,blob,at,scratch,A04_M111_DRAW_SCRATCH));
 printf("ABI5 real matrix VS%u + SAMPLE PS%u -> native real USC/VS CB/texture descriptor/PDS; alias/version/ISA/all changed PS bytes rejected atomically\n",v.code_bytes,tex_ps.code_bytes);
 compiler.release(compiler.context,&tex_ps);
 free(job);free(prior);free(scratch);free(blob);compiler.release(compiler.context,&v);compiler.release(compiler.context,&p);geo_pco_device_destroy(&compiler,owner);return 0;
}
