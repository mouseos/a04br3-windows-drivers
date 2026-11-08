/* SPDX-License-Identifier: MIT. Actual compile only; no GPU/driver load. */
#include <stdio.h>
#include <stdlib.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/render_owner/native_draw_job.h"
#include <string.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_program.h"
#include "../drivers/gpu/mt8167/wddm-render/compiler/texture2d_sm4.h"
int main(void){
 const uint32_t t[]={0x40u,0x1bu,0x4000059u,0x208e46u,0x0u,0x1u,0x300005au,0x106000u,0x0u,0x4001858u,0x107000u,0x0u,0x5555u,0x3000065u,0x1020f2u,0x0u,0xa000045u,0x1020f2u,0x0u,0x208046u,0x0u,0x0u,0x107e46u,0x0u,0x106000u,0x0u,0x100003eu};
 struct geo_sm4_signatures s={0};struct geo_shader_compiler c;struct geo_compiled_shader out;struct geo_texture2d_program p;void *owned=NULL;
 s.output_count=1;s.outputs[0].mask=15;
 int status=geo_pco_device_create(UINT64_C(0x001600280036001e),&c,&owned);if(status){fprintf(stderr,"device_create %d\n",status);return 1;}
 status=geo_pco_compile_texture2d(c.context,t,sizeof(t),&s,&out,&p);
 if(status){fprintf(stderr,"compile_texture %d\n",status);geo_pco_device_destroy(&c,owned);return 2;}
 if(!out.code||!out.code_bytes||out.shareds<12||out.constant_mask!=1){fprintf(stderr,"bad metadata\n");return 3;}
 printf("code_bytes=%u temps=%u shareds=%u constant_mask=%u\n",out.code_bytes,out.temps,out.shareds,out.constant_mask);
 for(unsigned i=0;i<out.code_bytes;i++)printf("%02x",((const unsigned char *)out.code)[i]);
 puts("");
 /* Cross the real producer/native consumer boundary with owned bytes. */
 _Alignas(8) unsigned char blob[1024]={0};struct geo_draw_shader native={0};
 if(out.code_bytes!=88)return 4;
 memcpy(blob,t,sizeof(t));memcpy(blob+sizeof(t),out.code,out.code_bytes);
 native.format=2;native.stage=1;native.tokens=(struct geo_draw_span){0,sizeof(t)};
 native.usc=(struct geo_draw_span){sizeof(t),out.code_bytes};native.constants=(struct geo_draw_span){sizeof(t)+88,512};
 native.temps=out.temps;native.shareds=out.shareds;native.constant_mask=out.constant_mask;
 if(a04_m111_texture2d_program(&native,blob,sizeof(blob)))return 5;
 for(unsigned i=0;i<88;i++){blob[sizeof(t)+i]^=1;if(!a04_m111_texture2d_program(&native,blob,sizeof(blob)))return 6;blob[sizeof(t)+i]^=1;}
 native.shareds=8;if(!a04_m111_texture2d_program(&native,blob,sizeof(blob)))return 7;native.shareds=12;
 if(!a04_m111_texture2d_program(&native,blob,100))return 8;
 puts("native boundary accepts real codegen and rejects88 altered bytes/forged descriptor registers/truncation");
 /* New sampled PS is genuinely source-compiled above. The unchanged finite
  * flat VS wire profile is a fixture from the already-admitted native path;
  * this test does not qualify the cached Linux VS compiler metadata. */
 const uint32_t vt[]={0x10040u,0xfu,0x300005fu,0x1010f2u,0x0u,0x4000067u,0x1020f2u,0x0u,0x1u,0x5000036u,0x1020f2u,0x0u,0x101e46u,0x0u,0x100003eu};
 const uint8_t vcode[24]={0x58,0xa0,0x06,0x08,0x00,0x80,0x04,0x00,0x00,0x30,0xf3,0xff,0xff,0xff,0xff,0xff,0x44,0xa0,0x80,0x05,0x00,0x00,0x00,0xff};
 struct geo_compiled_shader vout={0};vout.code=vcode;vout.code_bytes=sizeof(vcode);vout.vtxins=vout.vtxouts=4;vout.input_mask=1;
 _Alignas(8) unsigned char jobblob[2048]={0};uint32_t at=0;
 struct geo_draw_request_v3 request={0};struct geo_draw_shader *sh[2]={&request.base.vs,&request.base.ps};
 struct geo_compiled_shader *code[2]={&vout,&out};const uint32_t *tok[2]={vt,t};uint32_t tokbytes[2]={sizeof(vt),sizeof(t)};
 for(unsigned stage=0;stage<2;stage++){
  struct geo_draw_shader *q=sh[stage];q->format=2;q->stage=stage;q->temps=code[stage]->temps;q->shareds=code[stage]->shareds;q->vtxins=code[stage]->vtxins;q->vtxouts=code[stage]->vtxouts;q->input_mask=code[stage]->input_mask;q->constant_mask=code[stage]->constant_mask;
  q->tokens=(struct geo_draw_span){at,tokbytes[stage]};memcpy(jobblob+at,tok[stage],tokbytes[stage]);at+=tokbytes[stage];
  q->usc=(struct geo_draw_span){at,code[stage]->code_bytes};memcpy(jobblob+at,code[stage]->code,code[stage]->code_bytes);at+=code[stage]->code_bytes;
  q->constants=(struct geo_draw_span){at,512};at+=512;
 }
 request.base.size=sizeof(request);request.base.version=3;request.base.blob_bytes=at;request.base.target=1;request.base.vertex=2;request.texture=3;
 request.base.target_desc=(struct geo_surface_desc){sizeof(struct geo_surface_desc),1,800,600,3200,GEO_FORMAT_A8R8G8B8,GEO_USAGE_RENDER_TARGET,0};
 request.texture_desc=(struct geo_surface_desc){sizeof(struct geo_surface_desc),2,4,4,16,GEO_FORMAT_A8R8G8B8,GEO_USAGE_TEXTURE2D,0};request.sampler_profile=1;
 request.base.state.topology=GEO_DRAW_TRIANGLE_LIST;request.base.state.vertex_count=3;request.base.state.vertex_stride=16;request.base.state.color_write_mask=15;
 request.base.state.viewport[2]=0x44480000;request.base.state.viewport[3]=0x44160000;request.base.state.viewport[5]=0x3f800000;
 struct a04_m111_clear_job_input input={0};input.output_gpu=0x7000000000ULL;input.data_page_gpu=0x65c0010000ULL;input.usc_page_gpu=0xba00000000ULL;input.pds_page_gpu=0xa800000000ULL;input.dummy_region_gpu=0x65c0020000ULL;input.dummy_stencil_gpu=0x65c0021000ULL;input.hwrt_fw=0xc0088000;input.pr_fence_fw=0xc0084c00;
 struct a04_m111_draw_job job,unchanged;void *scratch=malloc(A04_M111_DRAW_SCRATCH);if(!scratch)return 11;
 printf("V3 validate%x samplePS%x\n",geo_draw_validate_capture_v3(&request,jobblob,at),a04_m111_texture2d_program(&request.base.ps,jobblob,at));
 int result=a04_m111_build_draw_job_v3(&job,&input,0x7000200000ULL,0x7000210000ULL,&request,jobblob,at,scratch,A04_M111_DRAW_SCRATCH);if(result){fprintf(stderr,"texture job build %x\n",result);return 12;}
 if(memcmp(job.usc+0xc0,out.code,out.code_bytes)||memcmp(job.usc+0x80,vout.code,vout.code_bytes))return 13;
 if(job.pds[0x208]!=12||job.pds[0x20b]!=0x90)return 14;
 unchanged=job;request.sampler_profile=2;
 if(!a04_m111_build_draw_job_v3(&job,&input,0x7000200000ULL,0x7000210000ULL,&request,jobblob,at,scratch,A04_M111_DRAW_SCRATCH)||memcmp(&job,&unchanged,sizeof(job)))return 15;
 request.sampler_profile=1;
 if(!a04_m111_build_draw_job_v3(&job,&input,0x7000200000ULL,0x7000200000ULL,&request,jobblob,at,scratch,A04_M111_DRAW_SCRATCH)||memcmp(&job,&unchanged,sizeof(job)))return 16;
 request.base.version=2;if(!geo_draw_validate_capture_v3(&request,jobblob,at))return 17;
 free(scratch);
 puts("source-compiled SAMPLE PS + baseline VS fixture -> ABI3 -> native descriptor/PDS job; reject alias/version/sampler atomically");
 c.release(c.context,&out);geo_pco_device_destroy(&c,owned);return 0;
}
