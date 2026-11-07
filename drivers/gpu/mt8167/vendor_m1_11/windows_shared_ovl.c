/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_shared_ovl.h"
#define OVL_EN 0x00cu
#define OVL_INTSTA 0x008u
#define OVL_SRC_CON 0x02cu
#define OVL_L0_CON 0x030u
#define OVL_L0_SIZE 0x038u
#define OVL_L0_PITCH 0x044u
#define OVL_L0_ADDR 0xf40u
#define OVL_MINIMUM 0xf44u
NTSTATUS a04_shared_ovl_switch(struct a04_shared_ovl *o,struct a04_shared_scanout *s,
    const struct a04_ge8300_shared_lease *lease,volatile void *base,ULONG bytes,uint32_t expected,
    a04_shared_ovl_ready ready,a04_shared_ovl_boundary boundary,void *ctx)
{
    volatile ULONG *regs=(volatile ULONG *)base;ULONG con,format;NTSTATUS status;
    if(!o || o->attempted || !s || !s->exposed || !s->mdl || !s->gpu_sg || !s->display_sg || !s->display_dma ||
       !lease || lease->shared_mdl_owner!=s || !lease->diagnostic_task || !lease->diagnostic_flush ||
       lease->diagnostic_matches!=1024 || !base || ((ULONG_PTR)base&3u) || bytes<OVL_MINIMUM || !expected ||
       !ready || !boundary || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    status=ready(ctx,s,lease->diagnostic_task,lease->diagnostic_flush);
    if(status!=STATUS_SUCCESS)return NT_SUCCESS(status)?STATUS_DEVICE_NOT_READY:status;
    status=boundary(ctx);if(status!=STATUS_SUCCESS)return NT_SUCCESS(status)?STATUS_DEVICE_NOT_READY:status;
    con=READ_REGISTER_ULONG(regs+OVL_L0_CON/4);format=(con>>12)&15u;
    if((con&((3u<<28)|(3u<<9))) || (format!=2u && format!=3u) ||
       !(READ_REGISTER_ULONG(regs+OVL_EN/4)&1u) || READ_REGISTER_ULONG(regs+OVL_SRC_CON/4)!=1u ||
       READ_REGISTER_ULONG(regs+OVL_L0_SIZE/4)!=((1280u<<16)|800u) ||
       (READ_REGISTER_ULONG(regs+OVL_L0_PITCH/4)&0xffffu)!=3200u ||
       READ_REGISTER_ULONG(regs+OVL_L0_ADDR/4)!=expected)return STATUS_DEVICE_CONFIGURATION_ERROR;
    /* The callbacks must also verify byte order and pixel layout. */
    o->base=regs;o->saved=expected;o->attempted=TRUE;
    KeMemoryBarrier();WRITE_REGISTER_ULONG(regs+OVL_L0_ADDR/4,s->display_dma);
    return STATUS_SUCCESS; /* write issued, not latch or scanout completion */
}
NTSTATUS a04_shared_ovl_restore(struct a04_shared_ovl *o,struct a04_shared_scanout *s,
    a04_shared_ovl_boundary boundary,void *ctx)
{
    NTSTATUS status;
    if(!o || !s || !o->attempted || o->restore_attempted || !o->base || !s->exposed ||
       !boundary || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    status=boundary(ctx);if(status!=STATUS_SUCCESS)return NT_SUCCESS(status)?STATUS_DEVICE_NOT_READY:status;
    if(!(READ_REGISTER_ULONG(o->base+OVL_EN/4)&1u) || READ_REGISTER_ULONG(o->base+OVL_SRC_CON/4)!=1u ||
       READ_REGISTER_ULONG(o->base+OVL_L0_ADDR/4)!=s->display_dma)return STATUS_DEVICE_CONFIGURATION_ERROR;
    o->restore_attempted=TRUE;KeMemoryBarrier();WRITE_REGISTER_ULONG(o->base+OVL_L0_ADDR/4,o->saved);
    return STATUS_SUCCESS; /* neither surface can yet be released */
}
