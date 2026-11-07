/* SPDX-License-Identifier: MIT. CPU producer only; no hardware execution. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_draw_job.h"
static uint64_t get(const uint8_t *p,unsigned at,unsigned n)
{uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[at+i]<<(i*8);return v;}
static void put(uint8_t *p,unsigned at,uint32_t v)
{for(unsigned i=0;i<4;i++)p[at+i]=(uint8_t)(v>>(i*8));}
int main(int argc,char **argv)
{
 assert(argc==2);FILE *f=fopen(argv[1],"r");assert(f);char line[1024];unsigned found=0;uint8_t code[2][64]={{0}};
 while(fgets(line,sizeof(line),f)){
  size_t n=strcspn(line,"\r\n");if(n!=48 && n!=128)continue;
  unsigned which=n==48?0:1;for(size_t i=0;i<n/2;i++){unsigned v;assert(sscanf(line+2*i,"%2x",&v)==1);code[which][i]=(uint8_t)v;}found|=1u<<which;
 }
 fclose(f);assert(found==3);
 _Alignas(8) uint8_t blob[1200]={0};
 struct geo_draw_request r={0};r.size=sizeof(r);r.version=1;r.blob_bytes=sizeof(blob);r.target=1;r.vertex=2;
 r.target_desc=(struct geo_surface_desc){sizeof(r.target_desc),1,800,600,3200,21,1,0};
 r.state.topology=1;r.state.vertex_count=3;r.state.vertex_stride=16;r.state.color_write_mask=15;
 r.state.viewport[2]=0x44480000;r.state.viewport[3]=0x44160000;r.state.viewport[5]=0x3f800000;
 r.vs=(struct geo_draw_shader){.format=2,.stage=0,.tokens={0,12},.usc={16,24},.constants={128,512},.vtxins=4,.vtxouts=4,.input_mask=1};
 r.ps=(struct geo_draw_shader){.format=2,.stage=1,.tokens={40,12},.usc={56,64},.constants={640,512},.temps=5,.shareds=4,.constant_mask=1};
 put(blob,0,0x10040);put(blob,4,3);put(blob,8,0x0100003e);put(blob,40,0x40);put(blob,44,3);put(blob,48,0x0100003e);
 memcpy(blob+16,code[0],24);memcpy(blob+56,code[1],64);put(blob,640,0x3f800000);put(blob,652,0x3f800000);
 struct a04_m111_draw_job *out=malloc(sizeof(*out)),*before=malloc(sizeof(*out));void *scratch=aligned_alloc(8,(A04_M111_DRAW_SCRATCH+7)&~7u);assert(out&&before&&scratch);
 struct a04_m111_clear_job_input in={.output_gpu=0x8200000000ULL,.data_page_gpu=0x65c0100000ULL,.usc_page_gpu=0xba00010000ULL,.pds_page_gpu=0xa800010000ULL,.dummy_region_gpu=0x65c0020000ULL,.dummy_stencil_gpu=0x65c0030000ULL,.hwrt_fw=0xc00848c0,.pr_fence_fw=0xc0084c00};
 for(unsigned test=0;test<2;test++){
  uint64_t vertex=0x82003ff000ULL;
  assert(a04_m111_build_draw_job(out,&in,vertex,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)==GEO_OK);
  assert(get(out->pds,0x150,8)==vertex && get(out->pds,0x148,4)==16 && get(out->pds,0x158,4)==0x80000004u);
  assert(get(out->data,0x510,8)==((599ULL<<48)|(799ULL<<34)|(12ULL<<27)|(1u<<11)|(2u<<8)|(3u<<5)|4));
  assert(get(out->data,0x518,8)==((1ULL<<60)|(1ULL<<59)|((in.output_gpu>>2)<<16)|799u));
  assert(get(out->pds,0x340,8)==in.data_page_gpu+0x500 && get(out->pds,0x348,4)==0x90000008u);
  assert(get(out->three,0xf8,8)==((in.pds_page_gpu-0xa800000000ULL+0x350)<<32 | (in.pds_page_gpu-0xa800000000ULL+0x300)));
  assert(get(out->three,0x110,8)==get(out->three,0xf8,8));
  assert(!memcmp(out->data+0x4a0,blob+640,16) && !memcmp(out->usc+0x80,code[0],24) && !memcmp(out->usc+0xc0,code[1],64));
  assert(get(out->data,0x280,4)==0x43960000u && get(out->data,0x284,4)==0x3f000000u && get(out->data,0x288,4)==0x3f000000u);
  memcpy(before,out,sizeof(*out));
  blob[56]^=1;assert(a04_m111_build_draw_job(out,&in,vertex,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)==GEO_UNSUPPORTED);blob[56]^=1;
  assert(!memcmp(before,out,sizeof(*out)));
  r.ps.constants.offset=sizeof(blob)-4;assert(a04_m111_build_draw_job(out,&in,vertex,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)==GEO_INVALID);r.ps.constants.offset=640;
  r.state.viewport[2]=0x7f800000;assert(a04_m111_build_draw_job(out,&in,vertex,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)==GEO_INVALID);r.state.viewport[2]=0x44480000;
  r.state.vertex_count=6;assert(a04_m111_build_draw_job(out,&in,vertex,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)==GEO_UNSUPPORTED);r.state.vertex_count=3;
  assert(a04_m111_build_draw_job(out,&in,in.output_gpu,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)==GEO_INVALID);
  assert(!memcmp(before,out,sizeof(*out)));
  in.output_gpu+=0x100000;in.data_page_gpu+=0x100000;in.usc_page_gpu+=0x1000;in.pds_page_gpu+=0x2000;
 }
 free(out);free(before);free(scratch);
 puts("dynamic draw CPU: official SM4->PCO exact ISA allowlist, same-target load descriptor/PR+normal background, separate vertex float4, heap relocation, input rejection/unchanged error PASS; not GPU raster proof");
}
