/* SPDX-License-Identifier: MIT
 * First paired geometry/fragment kick based on Linux pvr_cccb.c, pvr_job.c
 * and UniXNU NativePowerVRJobs.cpp (MIT option), Copyright Imagination.
 */
#include "render_pair.h"
#include <stddef.h>
#define RING_SIZE (1u<<15)
_Static_assert(sizeof(struct rogue_fwif_cmd_geom)%8==0,"geometry command alignment");
_Static_assert(sizeof(struct rogue_fwif_cmd_frag)%8==0,"fragment command alignment");
static void copy(void *d,const void *s,size_t n)
{ size_t i; for (i=0;i<n;i++) ((uint8_t *)d)[i]=((const uint8_t *)s)[i]; }
static int object_owned(const struct a04_gpu_fw_arena *a,const struct a04_gpu_fw_object *o,size_t required)
{
    size_t off;
    if (!a || !a->heap || !o || !o->cpu || o->size<required ||
        (o->fw_addr&0xff000000u)!=0xc0000000u || (o->fw_addr&63u)) return 0;
    off=(size_t)(o->fw_addr&0x00ffffffu);
    return off<=a->next && o->size<=a->next-off && o->cpu==a->heap+off && o->dma==a->dma+off;
}
static uint32_t command(uint8_t *ring,uint32_t off,uint32_t type,const void *payload,uint32_t size,uint32_t job)
{
    struct rogue_fwif_ccb_cmd_header h={0};
    h.cmd_type=type; h.cmd_size=size; h.ext_job_ref=job; h.int_job_ref=job;
    copy(ring+off,&h,sizeof(h)); copy(ring+off+sizeof(h),payload,size);
    return off+(uint32_t)sizeof(h)+size;
}
int a04_gpu_prepare_render_pair(const struct a04_gpu_fw_arena *arena,
    struct a04_gpu_render_context *c,const struct a04_gpu_fw_object *hwrt,
    const struct rogue_fwif_cmd_geom *geom,const struct rogue_fwif_cmd_frag *frag,
    uint32_t job,int partial,struct rogue_fwif_kccb_cmd *out)
{
    struct rogue_fwif_cmd_geom g; struct rogue_fwif_cmd_frag f;
    struct rogue_fwif_ufo ufo; struct rogue_fwif_kccb_cmd kick={0};
    struct rogue_fwif_kccb_cmd_kick_data *gkick,*fkick; struct rogue_fwif_cccb_ctl *gc,*fc;
    struct rogue_fwif_fwrendercontext *ctx;
    uint32_t go=0,fo=0,cleanup;
    if (!arena || !c || c->pair_prepared || !geom || !frag || !out || !job || (partial!=0 && partial!=1) ||
        !object_owned(arena,&c->context,sizeof(struct rogue_fwif_fwrendercontext)) ||
        !object_owned(arena,&c->geom_control,sizeof(*gc)) || !object_owned(arena,&c->frag_control,sizeof(*fc)) ||
        !object_owned(arena,&c->geom_ring,RING_SIZE) || !object_owned(arena,&c->frag_ring,RING_SIZE) ||
        !object_owned(arena,&c->geom_fence,sizeof(uint32_t)) || !object_owned(arena,&c->frag_fence,sizeof(uint32_t)) ||
        !object_owned(arena,hwrt,sizeof(struct rogue_fwif_hwrtdata))) return -1;
    if (c->geom_context_fw!=c->context.fw_addr+offsetof(struct rogue_fwif_fwrendercontext,geom_context) ||
        c->frag_context_fw!=c->context.fw_addr+offsetof(struct rogue_fwif_fwrendercontext,frag_context)) return -1;
    gc=c->geom_control.cpu; fc=c->frag_control.cpu;
    ctx=c->context.cpu;
    if (gc->wrap_mask!=RING_SIZE-1 || fc->wrap_mask!=RING_SIZE-1 || gc->read_offset || fc->read_offset ||
        gc->write_offset || fc->write_offset ||
        ctx->geom_context.ccb_fw_addr!=c->geom_ring.fw_addr || ctx->frag_context.ccb_fw_addr!=c->frag_ring.fw_addr ||
        ctx->geom_context.ccbctl_fw_addr!=c->geom_control.fw_addr || ctx->frag_context.ccbctl_fw_addr!=c->frag_control.fw_addr ||
        c->geom_ring.fw_addr==c->frag_ring.fw_addr || c->geom_fence.fw_addr==c->frag_fence.fw_addr ||
        *(volatile uint32_t *)c->geom_fence.cpu || *(volatile uint32_t *)c->frag_fence.cpu) return -1;
    g=*geom; f=*frag;
    g.cmd_shared.hwrt_data_fw_addr=hwrt->fw_addr; f.cmd_shared.hwrt_data_fw_addr=hwrt->fw_addr;
    g.partial_render_geom_frag_fence.addr=c->geom_fence.fw_addr; g.partial_render_geom_frag_fence.value=0;
    go=command(c->geom_ring.cpu,go,ROGUE_FWIF_CCB_CMD_TYPE_GEOM,&g,sizeof(g),job);
    ufo.addr=c->geom_fence.fw_addr; ufo.value=1;
    go=command(c->geom_ring.cpu,go,ROGUE_FWIF_CCB_CMD_TYPE_UPDATE,&ufo,sizeof(ufo),0);
    fo=command(c->frag_ring.cpu,fo,ROGUE_FWIF_CCB_CMD_TYPE_FENCE_PR,&ufo,sizeof(ufo),0);
    fo=command(c->frag_ring.cpu,fo,partial ? ROGUE_FWIF_CCB_CMD_TYPE_FRAG_PR : ROGUE_FWIF_CCB_CMD_TYPE_FRAG,&f,sizeof(f),job);
    ufo.addr=c->frag_fence.fw_addr;
    fo=command(c->frag_ring.cpu,fo,ROGUE_FWIF_CCB_CMD_TYPE_UPDATE,&ufo,sizeof(ufo),0);
    cleanup=hwrt->fw_addr+(uint32_t)offsetof(struct rogue_fwif_hwrtdata,cleanup_state);
    kick.cmd_type=ROGUE_FWIF_KCCB_CMD_COMBINED_GEOM_FRAG_KICK;
    gkick=&kick.cmd_data.combined_geom_frag_cmd_kick_data.geom_cmd_kick_data;
    fkick=&kick.cmd_data.combined_geom_frag_cmd_kick_data.frag_cmd_kick_data;
    gkick->context_fw_addr=c->geom_context_fw; gkick->client_woff_update=go; gkick->client_wrap_mask_update=RING_SIZE-1;
    gkick->num_cleanup_ctl=1; gkick->cleanup_ctl_fw_addr[0]=cleanup;
    fkick->context_fw_addr=c->frag_context_fw; fkick->client_woff_update=fo; fkick->client_wrap_mask_update=RING_SIZE-1;
    if (!partial) { fkick->num_cleanup_ctl=1; fkick->cleanup_ctl_fw_addr[0]=cleanup; }
    c->pair_prepared=partial ? 2u : 1u; c->frag_end=fo; c->hwrt_fw=hwrt->fw_addr; *out=kick; return 0;
}
int a04_gpu_prepare_final_fragment(const struct a04_gpu_fw_arena *arena,
    struct a04_gpu_render_context *c,const struct a04_gpu_fw_object *hwrt,
    const struct rogue_fwif_cmd_frag *frag,uint32_t job,struct rogue_fwif_kccb_cmd *out)
{
    struct rogue_fwif_cmd_frag f; struct rogue_fwif_ufo ufo;
    struct rogue_fwif_kccb_cmd kick={0}; struct rogue_fwif_kccb_cmd_kick_data *k;
    uint32_t off,needed=(uint32_t)(2*sizeof(struct rogue_fwif_ccb_cmd_header)+sizeof(f)+sizeof(ufo));
    if (!arena || !c || c->pair_prepared!=2 || !frag || !out || !job ||
        !object_owned(arena,&c->context,sizeof(struct rogue_fwif_fwrendercontext)) ||
        !object_owned(arena,&c->frag_ring,RING_SIZE) || !object_owned(arena,&c->frag_fence,sizeof(uint32_t)) ||
        !object_owned(arena,hwrt,sizeof(struct rogue_fwif_hwrtdata)) || c->hwrt_fw!=hwrt->fw_addr ||
        c->frag_context_fw!=c->context.fw_addr+offsetof(struct rogue_fwif_fwrendercontext,frag_context) ||
        c->frag_end>RING_SIZE || needed+sizeof(struct rogue_fwif_ccb_cmd_header)>RING_SIZE-c->frag_end)
        return -1;
    off=c->frag_end; f=*frag; f.cmd_shared.hwrt_data_fw_addr=hwrt->fw_addr;
    off=command(c->frag_ring.cpu,off,ROGUE_FWIF_CCB_CMD_TYPE_FRAG,&f,sizeof(f),job);
    ufo.addr=c->frag_fence.fw_addr; ufo.value=2;
    off=command(c->frag_ring.cpu,off,ROGUE_FWIF_CCB_CMD_TYPE_UPDATE,&ufo,sizeof(ufo),0);
    kick.cmd_type=ROGUE_FWIF_KCCB_CMD_KICK; k=&kick.cmd_data.cmd_kick_data;
    k->context_fw_addr=c->frag_context_fw; k->client_woff_update=off; k->client_wrap_mask_update=RING_SIZE-1;
    k->num_cleanup_ctl=1; k->cleanup_ctl_fw_addr[0]=hwrt->fw_addr+(uint32_t)offsetof(struct rogue_fwif_hwrtdata,cleanup_state);
    c->frag_end=off; c->pair_prepared=3; *out=kick; return 0;
}
