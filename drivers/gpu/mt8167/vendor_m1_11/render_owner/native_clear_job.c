/* SPDX-License-Identifier: MIT
 * Complete scalar record profile: original UM/KM/FW1.11@5425693 clear,
 * capture04 ev_23a2611a79b90fdc1374766579b6795f0c473575fdd08b59e53b719d77707fc6.
 * Unnamed fixed scalars are deliberately not generalized to another profile.
 * Only source-proven address fields are regenerated. No replayable captured
 * VA, binary record template, or captured code/resource blob is embedded.
 * Code alternative: source-generated packed-colour background and shared-reg
 * single-emit EOT; bindings come from native_clear_codespace, not capture's
 * texture-reading background or immediate-constant EOT program.
 */
#include "native_clear_job.h"
#include "native_clear_surface.h"
static int range(uint64_t va,uint64_t n,uint64_t low,uint64_t high,uint64_t align)
{ return va>=low && va<high && n<=high-va && !(va&(align-1u)); }
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<b+m && b<a+n; }
static void word(uint8_t *p,unsigned at,uint64_t v,unsigned n)
{ unsigned i;for(i=0;i<n;i++)p[at+i]=(uint8_t)(v>>(8u*i)); }
int a04_m111_build_clear_job_dims(struct a04_m111_clear_job *out,
    const struct a04_m111_clear_job_input *in,uint32_t width,uint32_t height)
{
    struct a04_m111_clear_job made={0};struct a04_m111_clear_surface surface;
    struct a04_m111_clear_code_bindings bindings;
    struct a04_m111_ta_fields ta={0};struct a04_m111_ta_registers regs={0};
    struct a04_m111_3d_fields three={0};uint64_t csr[8][2]={{0}};
    uint64_t va[4],len[4];uint32_t colour;unsigned i,j;
    if(width!=800u || (height!=600u && height!=1280u))return 1;
    if(!out || !in || overlap((uintptr_t)out,sizeof(*out),(uintptr_t)in,sizeof(*in)))return 1;
    if(!range(in->data_page_gpu,4096,UINT64_C(0x65c0000000),UINT64_C(0x6600000000),4096) ||
       !range(in->usc_page_gpu,4096,UINT64_C(0xba00000000),UINT64_C(0xba40000000),4096) ||
       !range(in->pds_page_gpu,4096,UINT64_C(0xa800000000),UINT64_C(0xa840000000),4096) ||
       !range(in->output_gpu,((uint64_t)width*height*4u),4096,UINT64_C(1)<<40,128) ||
       !range(in->dummy_region_gpu,128,UINT64_C(0x65c0000000),UINT64_C(0x6600000000),64) ||
       !range(in->dummy_stencil_gpu,2048,UINT64_C(0x65c0000000),UINT64_C(0x6600000000),16) ||
       !range(in->hwrt_fw,0x188,UINT64_C(0xc0000000),UINT64_C(0xc1000000),8) ||
       !range(in->pr_fence_fw,4,UINT64_C(0xc0000000),UINT64_C(0xc1000000),4))return 1;
    va[0]=in->data_page_gpu;len[0]=4096;va[1]=in->output_gpu;len[1]=((uint64_t)width*height*4u);
    va[2]=in->dummy_region_gpu;len[2]=128;va[3]=in->dummy_stencil_gpu;len[3]=2048;
    for(i=0;i<4;i++)for(j=0;j<i;j++)if(overlap(va[i],len[i],va[j],len[j]))return 1;
    if(overlap(in->hwrt_fw,0x188,in->pr_fence_fw,4))return 1;
    /* Reject output aliases with code heaps even if its start is elsewhere. */
    if(overlap(in->output_gpu,len[1],in->usc_page_gpu,4096) ||
       overlap(in->output_gpu,len[1],in->pds_page_gpu,4096))return 1;
    colour=(uint32_t)in->rgba[0]|((uint32_t)in->rgba[1]<<8)|
        ((uint32_t)in->rgba[2]<<16)|((uint32_t)in->rgba[3]<<24);
    if(a04_m111_build_clear_surface(&surface,in->output_gpu,width,height,width) ||
       a04_m111_build_clear_data(&made.data,in->data_page_gpu,width,height) ||
       a04_m111_build_clear_codespace(&made.code,in->usc_page_gpu,in->pds_page_gpu,
           colour,surface.emit[0],surface.emit[1]) ||
       a04_m111_build_clear_code_bindings(&bindings,in->pds_page_gpu))return 1;
    regs.ppp_ctrl=0x203;regs.vdm_stream=made.data.vdm_gpu;regs.tpu=0x20;
    regs.word38=0x5f00;regs.pds_code40=bindings.ta_ppp_pds_base;
    ta.word08=0x20000;ta.flags=3;ta.hwrt=in->hwrt_fw;
    ta.pr_fence_addr=in->pr_fence_fw;ta.dummy_region=in->dummy_region_gpu;
    three.word00=0x1ffc;three.word20=0xb300f;three.hwrt=in->hwrt_fw;
    three.dummy_stencil=in->dummy_stencil_gpu|1u;
    csr[0][0]=surface.csr[0];csr[0][1]=surface.csr[1];
    if(a04_m111_patch_ta_registers(made.ta,sizeof(made.ta),&regs) ||
       a04_m111_patch_ta_fields(made.ta,sizeof(made.ta),&ta) ||
       a04_m111_patch_3d_fields(made.three,sizeof(made.three),&three) ||
       a04_m111_patch_3d_fast_clear_rgba8(made.three,sizeof(made.three),
           in->rgba[0],in->rgba[1],in->rgba[2],in->rgba[3]) ||
       a04_m111_patch_3d_pbe(made.three,sizeof(made.three),csr) ||
       a04_m111_patch_3d_background(made.three,sizeof(made.three),bindings.background,bindings.background))return 1;
    word(made.three,0x14,0x3f800000u,4);word(made.three,0x18,0x100,4);
    word(made.three,0x24,0x20,4);word(made.three,0x28,bindings.pixel_event_info,4);
    word(made.three,0x30,bindings.pixel_event_code,4);word(made.three,0x34,bindings.pixel_event_data,4);
    word(made.three,0x38,made.data.scissor_gpu,8);word(made.three,0x40,made.data.dbias_gpu,8);
    *out=made;return 0;
}

int a04_m111_build_clear_job_800x600_cold(struct a04_m111_clear_job *out,const struct a04_m111_clear_job_input *in)
{return a04_m111_build_clear_job_dims(out,in,800,600);}
