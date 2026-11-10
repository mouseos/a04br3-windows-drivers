/* Exact source-reconstructed packet: source-only native consumer regression.
 * Owner IDs and VA below are test-owned representatives, not actual mappings.
 * Same bytecode/blob/span/state metadata/cookie as run173 PID2612. No GPU. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/render_owner/native_draw_job.h"
#include "../drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_program.h"
#define STATUS_SUCCESS 0
#define STATUS_PENDING 259
static uint32_t records[70][4];
static void GeoFactoryCheckpoint(uint32_t id,uint32_t status,uint32_t a,uint32_t b,uint32_t c,uint32_t d){(void)status;records[id][0]=a;records[id][1]=b;records[id][2]=c;records[id][3]=d;}
#include "../drivers/gpu/mt8167/vendor_m1_11/draw_trace_candidate.h"
static void load(const char*p,void*b,size_t n){FILE*f=fopen(p,"rb");assert(f&&fread(b,1,n,f)==n&&fgetc(f)==EOF);fclose(f);}
int main(int argc,char**argv){
 assert(argc==3);struct geo_draw_request_v5 r;_Alignas(8) uint8_t blob[1784];load(argv[1],&r,sizeof(r));load(argv[2],blob,sizeof(blob));
 GeoDrawTraceSnapshot(&r.sampled.base,blob,sizeof(blob),0,9,12);
 const uint32_t expected[][5]={{56,0,9,12,3112824852u},{57,5,1784,1338208353u,3112824852u},{58,800,600,3200,3112824852u},{63,2,2,8,3112824852u},{64,60,24,512,3112824852u},{65,108,88,512,3112824852u},{66,982242685u,4144445243u,1299645893u,3112824852u},{67,3297410195u,97339587u,3727623621u,3112824852u},{68,480,2187499735u,67108865u,3112824852u},{69,1849082071u,2065261263u,1116225057u,3112824852u}};
 for(unsigned i=0;i<sizeof(expected)/sizeof(*expected);i++)assert(!memcmp(records[expected[i][0]],expected[i]+1,16));
 assert(geo_draw_validate_capture_v5(&r,blob,sizeof(blob))==0);
 struct a04_m111_clear_job_input in={0};in.output_gpu=0x7000000000ULL;in.data_page_gpu=0x65c0010000ULL;in.usc_page_gpu=0xba00000000ULL;in.pds_page_gpu=0xa800000000ULL;in.dummy_region_gpu=0x65c0020000ULL;in.dummy_stencil_gpu=0x65c0021000ULL;in.hwrt_fw=0xc0088000;in.pr_fence_fw=0xc0084c00;
 struct a04_m111_draw_job *j=malloc(sizeof(*j)),*before=malloc(sizeof(*j));void*scratch=malloc(A04_M111_DRAW_SCRATCH);assert(j&&before&&scratch);
 memset(j,0xa5,sizeof(*j));memcpy(before,j,sizeof(*j));
 int32_t status=a04_m111_build_draw_job_v5(j,&in,0x7000200000ULL,0x7000210000ULL,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH);
#ifdef EXPECT_OLD
 assert((uint32_t)status==0xc00000bbu&&!memcmp(j,before,sizeof(*j)));puts("353 negative: exact R56..69 reconstructed input rejects C00000BB before cold writes");
#else
 assert(status==0);assert(!memcmp(j->usc+0x800,blob+r.sampled.base.ps.usc.offset,88));
 /* Production scissor state survives wire validation into the immutable ISP
  * table. Target-relative clamping must not change shader or resource identity. */
 {static const int32_t rects[][4]={{13,17,79,113},{-10,-20,27,31},{900,10,1000,40},{0,0,0,0}};
  static const uint32_t clipped[][4]={{13,17,79,113},{0,0,27,31},{800,10,800,40},{0,0,0,0}};
  r.sampled.base.state.scissor_enable=1;
  for(unsigned c=0;c<4;c++){
   memcpy(r.sampled.base.state.scissor,rects[c],sizeof(rects[c]));
   assert(geo_draw_validate_capture_v5(&r,blob,sizeof(blob))==0);
   assert(a04_m111_build_draw_job_v5(j,&in,0x7000200000ULL,0x7000210000ULL,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)==0);
   for(unsigned k=0;k<2;k++)for(unsigned axis=0;axis<2;axis++){
    uint32_t word=0;for(unsigned b=0;b<4;b++)word|=(uint32_t)j->data[0x380+8*k+4*axis+b]<<(8*b);
    assert(word==((clipped[c][axis]<<16)|clipped[c][axis+2]));
   }
   assert(!memcmp(j->usc+0x800,blob+r.sampled.base.ps.usc.offset,88));
  }
  r.sampled.base.state.scissor[0]=2;r.sampled.base.state.scissor[2]=1;
  memcpy(before,j,sizeof(*j));assert(a04_m111_build_draw_job_v5(j,&in,0x7000200000ULL,0x7000210000ULL,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)!=0&&!memcmp(j,before,sizeof(*j)));
  r.sampled.base.state.scissor_enable=0;
 }
 /* Same producer-selected bytes are copied; admission cannot silently change
  * code, register count, aliasing, texture format or framing. */
 for(unsigned k=0;k<88;k++){unsigned at=r.sampled.base.ps.usc.offset+k;blob[at]^=1;assert(a04_m111_texture2d_program(&r.sampled.base.ps,blob,sizeof(blob))!=0);blob[at]^=1;}
 for(unsigned temps=0;temps<9;temps++)if(temps!=6){r.sampled.base.ps.temps=temps;memcpy(before,j,sizeof(*j));assert(a04_m111_build_draw_job_v5(j,&in,0x7000200000ULL,0x7000210000ULL,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)!=0&&!memcmp(j,before,sizeof(*j)));}r.sampled.base.ps.temps=6;
 memcpy(before,j,sizeof(*j));assert(a04_m111_build_draw_job_v5(j,&in,0x7000200000ULL,0x7000200000ULL,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)!=0&&!memcmp(j,before,sizeof(*j)));
 assert(a04_m111_build_draw_job_v5(j,&in,0x7000200000ULL,0x7000210000ULL,&r,blob,100,scratch,A04_M111_DRAW_SCRATCH)!=0);
 puts("354: all R56..69 fields match; exact temps6/USC88 admitted; 88 altered bytes, 8 forged temp bounds, alias and truncation rejected; no GPU completion inferred");
#endif
 free(j);free(before);free(scratch);return 0;
}
