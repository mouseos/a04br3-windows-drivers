/* SPDX-License-Identifier: MIT
 * Default-off fixed Mesa GE8300 render diagnostic. No caller-provided commands. */
#include "windows_render_diagnostic.h"
#if A04_GE8300_RENDER_EXPERIMENT
#include "render_fixed_command.h"
#include "render_fixed_ppp.h"
#include "render_fixed_vdm.h"
#include "render_pds_data.h"
#include "gpu_mmu.h"
#define G UINT64_C(0x8000000000)
#define P UINT64_C(0xda00000000)
#define U UINT64_C(0xe000000000)
#define PAGES 1179u
#define Z (A04_RENDER_PAYLOAD_OFFSET)
#define STEP(n,call) do { d->stage=(n);status=(call);if(!NT_SUCCESS(status)) goto fail; } while(0)
static const struct a04_windows_render_map maps[]={
    {G,0,64,0,A04_GPU_MMU_READ_ONLY},{G+0x40000,64,1,0,A04_GPU_MMU_READ_ONLY},
    {G+0x41000,65,4,0,A04_GPU_MMU_SLC_BYPASS},{G+0x45000,69,1,0,A04_GPU_MMU_SLC_BYPASS},
    {G+0x46000,70,2,0,A04_GPU_MMU_SLC_BYPASS},{G+0x48000,72,2,0,A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_PM_FW_PROTECT},
    {G+0x4a000,1175,4,0,0},{P,1162,1,1,A04_GPU_MMU_READ_ONLY},
    {P+0x1000,1163,10,1,0},{P+0xb000,1173,1,1,0},{U,1174,1,2,A04_GPU_MMU_READ_ONLY}
};
static void ranges(const struct a04_windows_render_memory *m,struct a04_render_owned_range *r)
{
    size_t i;for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *x=&m->maps[i];
        r[i].gpu_va=x->gpu_va;r[i].dma=(uint64_t)m->logical.QuadPart+Z+(uint64_t)x->payload_page*4096u;
        r[i].size=(uint64_t)x->pages*4096u;r[i].access=A04_RENDER_RANGE_READ;
        if (!(x->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT))) r[i].access|=A04_RENDER_RANGE_WRITE;
        if ((x->mmu_flags&A04_GPU_MMU_READ_ONLY) && (x->gpu_va>>32)==UINT64_C(0xe0)) r[i].access|=A04_RENDER_RANGE_EXEC;
    }
}
static NTSTATUS verify_fixed(struct a04_windows_render_memory *m,const struct a04_render_owned_range *r,
    const struct a04_render_fixed_ppp_input *pp,const struct a04_render_fixed_vdm_input *vd)
{
    struct a04_render_fixed_ppp ppp;uint8_t vdm[60],bg[16],data[16],code[16]={0,0,2,0xf4,0,0,0,0xd3};
    uint8_t usc[128],frag[48],eot[128],event[16],staticpds[256]={0},staticusc[128]={0};
    static const uint8_t scissor[8]={49,0,0,0,64,0,0,0};
    static const uint8_t nop[8]={0x44,0xa0,0x80,0x05,0,0,0,0xff};size_t i;
    if (a04_render_make_fixed_ppp(pp,r,m->map_count,&ppp) || a04_render_make_fixed_vdm(vd,r,m->map_count,vdm) ||
        a04_render_make_fixed_background(P+0x580,r,m->map_count,bg,data) ||
        a04_render_make_fragment_pds(U+0xe40,0,r,m->map_count,frag) ||
        a04_render_make_fixed_eot(G+0x41000,r,m->map_count,eot) ||
        a04_render_make_pixel_event(U+0xec0,r,m->map_count,event)) return STATUS_INVALID_DEVICE_STATE;
    a04_render_make_fixed_background_usc(usc);
    staticpds[32]=2;staticpds[35]=0xf4;staticpds[128]=2;staticpds[131]=0xf4;
    for(i=0;i<8;++i) staticusc[i]=nop[i];
#define CHECK(a,n,b,u) do { if(!NT_SUCCESS(a04_windows_render_verify_staged(m,(a),(n),(b),(u)))) return STATUS_INVALID_DEVICE_STATE; } while(0)
    CHECK(P,256,staticpds,256);CHECK(U,128,staticusc,128);CHECK(G,262144,NULL,0);
    CHECK(G+0x40000,60,vdm,60);CHECK(G+0x40100,8,scissor,8);
    CHECK(P+0x640,100,ppp.state,100);CHECK(P+0x6b0,12,ppp.finish,12);
    CHECK(P+0x580,16,bg,16);CHECK(P+0x590,16,data,16);CHECK(P+0x5a0,16,code,16);
    CHECK(P+0x5c0,48,frag,48);CHECK(U+0xe40,128,usc,128);CHECK(U+0xec0,128,eot,128);CHECK(P+0x600,16,event,16);
#undef CHECK
    return a04_windows_render_verify_zero_target(m,G+0x41000);
}
NTSTATUS a04_windows_render_diagnostic_run(struct a04_windows_render_diagnostic *d,struct a04_gpu_firmware_state *s)
{
    struct a04_windows_render_memory *m;struct a04_render_context_static_input cs={0};
    struct rogue_fwif_geom_registers_caswitch state;struct a04_windows_render_freelist_layout fl[2]={{G+0x48000,74,64,1},{G+0x49000,138,1024,2}};
    struct a04_render_hwrt_params hw={0};struct a04_render_fixed_ppp_input pp={P+0x480,P+0x540,P+0x500};
    struct a04_render_fixed_vdm_input vd={P+0x410,P+0x430,P+0x400,P+0x420,P+0x640,P+0x6b0};
    struct a04_render_fixed_command_input ci={G+0x40000,G,G+0x40100,G+0x41000,P+0x5c0,P+0x5a0,P+0x590,P+0x590,P+0x600};
    struct a04_render_owned_range r[A04_RENDER_MAX_MAPS];struct rogue_fwif_cmd_geom geom;struct rogue_fwif_cmd_frag frag;
    NTSTATUS status=STATUS_INVALID_PARAMETER;unsigned i,x,y;const uint8_t *pixels;
    if (!d || !s || d->stage || d->memory.cpu || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    m=&d->memory;d->result=STATUS_INVALID_DEVICE_STATE;
    STEP(1,a04_windows_render_memory_create(m,s,PAGES,maps,sizeof(maps)/sizeof(maps[0])));
    STEP(2,a04_windows_render_memory_bind_vm(m));
    STEP(3,a04_windows_render_stage_static_heap_programs(m));
    for(i=0;i<2;++i) {
        uint64_t pt=P+0x100+i*0x80u,sr=P+0x200+i*0x100u;
        uint64_t store=U+(i?0x600u:0x80u),load=U+(i?0x900u:0x380u);
        struct a04_render_context_static_slot *q=&cs.slot[i];
        q->pt_store_data=pt;q->pt_store_code=pt+32;q->pt_resume_data=pt+64;q->pt_resume_code=pt+96;
        q->sr_store_data=sr;q->sr_store_code=sr+64;q->sr_resume_data=sr+128;q->sr_resume_code=sr+192;
        STEP(4+i*3,a04_windows_render_stage_context_pt(m,P+0xb000+i*128u,q->pt_store_data,q->pt_store_code,q->pt_resume_data,q->pt_resume_code));
        STEP(5+i*3,a04_windows_render_stage_bounded_context_usc(m,store,load));
        STEP(6+i*3,a04_windows_render_stage_bounded_context_sr(m,P+0x1000+i*0x5000u,store,load,q->sr_store_data,q->sr_store_code,q->sr_resume_data,q->sr_resume_code));
    }
    cs.vdm_state=G+0x45000;cs.geom_state=G+0x45100;
    STEP(10,a04_windows_render_make_bounded_static(m,&cs,&state));
    STEP(11,a04_windows_render_prepare_context(m,1,G+0x45400,4,&state));
    STEP(12,a04_windows_render_prepare_freelists(m,fl));
    hw.width=64;hw.height=64;hw.tpc_size=128;hw.region_header_size=16;hw.tpc_gpu=G+0x46000;
    hw.vheap_gpu=G+0x46100;hw.region_gpu=G+0x46700;hw.pm_mlist_gpu=G+0x47000;
    STEP(13,a04_windows_render_prepare_target(m,&hw));
    STEP(14,a04_windows_render_reserve_zero_border(m,G));
    STEP(15,a04_windows_render_stage_triangle_usc(m,U+0xb80,U+0xc80));
    STEP(16,a04_windows_render_stage_triangle_vertex_pds(m,P+0x440,G+0x4a000,P+0x400,P+0x410));
    STEP(17,a04_windows_render_stage_triangle_vertex_code(m,P+0x420,P+0x430));
    STEP(18,a04_windows_render_stage_triangle_fragment_pds(m,P+0x480));
    STEP(19,a04_windows_render_stage_triangle_uniform(m,P+0x520,G+0x4c000,P+0x530,P+0x500,P+0x540));
    STEP(20,a04_windows_render_stage_fixed_background(m,P+0x580,P+0x590,P+0x5a0));
    STEP(21,a04_windows_render_stage_background_shader(m,U+0xe40,P+0x5c0));
    STEP(22,a04_windows_render_stage_fixed_eot(m,U+0xec0,G+0x41000));
    STEP(23,a04_windows_render_stage_pixel_event(m,P+0x600,U+0xec0));
    STEP(24,a04_windows_render_stage_fixed_ppp(m,&pp,P+0x640,P+0x6b0));
    STEP(25,a04_windows_render_stage_fixed_vdm(m,&vd,G+0x40000));
    STEP(26,a04_windows_render_stage_fixed_scissor(m,G+0x40100));
    ranges(m,r);STEP(27,verify_fixed(m,r,&pp,&vd));
    if (a04_render_make_fixed_commands(&ci,r,m->map_count,&geom,&frag)) { status=STATUS_INVALID_DEVICE_STATE;goto fail; }
    STEP(28,a04_windows_render_prepare_partial_pair(m,&geom,&frag,&frag,1));
    STEP(29,a04_windows_render_publish_context(m));
    STEP(30,a04_windows_render_submit_pair(m));
    STEP(31,a04_windows_render_collect_pair(m));
    pixels=(const uint8_t *)m->cpu+Z+65u*4096u;
    /* 64x64 PBE twiddled surface: Morton-order hypothesis, not hardware proof. */
    for(y=0;y<64;++y) for(x=0;x<64;++x) {
        unsigned bit,index=0;int bg,tri,edge,inside;const uint8_t *p;
        for(bit=0;bit<6;++bit) index|=((x>>bit)&1u)<<(2u*bit) | ((y>>bit)&1u)<<(2u*bit+1u);
        p=pixels+4u*index;
        bg=p[0]==0x22 && p[1]==0x44 && p[2]==0x66 && p[3]==0xff;
        tri=p[0]==0xff && p[1]==0 && p[2]==0xff && p[3]==0xff;
        if(bg) ++d->background_pixels;
        else if(tri) { ++d->triangle_pixels;d->triangle_rows[y][x/32u]|=1u<<(x%32u); }
        else ++d->other_pixels;
        edge=x>=8u && x<49u && y>=8u && x+y==63u;
        inside=x>=8u && x<49u && y>=8u && x+y<63u;
        if(!edge && ((inside && !tri) || (!inside && !bg))) ++d->spatial_mismatch;
    }
    d->stage=32;status=(d->background_pixels && d->triangle_pixels>=1107u && d->triangle_pixels<=1148u &&
        !d->other_pixels && !d->spatial_mismatch) ? STATUS_SUCCESS : STATUS_DATA_ERROR;
fail:
    d->result=status;return status;
}
#endif
