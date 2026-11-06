/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_display_scanout.h"
#include "../windows_hw.h"
#if A04_GE8300_DISPLAY_SCANOUT_EXPERIMENT
/* Offsets from Linux MT8167 ddp_reg.h; access only a PnP-owned OVL resource. */
#define OVL_EN 0x00cu
#define OVL_SRC_CON 0x02cu
#define OVL_L0_CON 0x030u
#define OVL_L0_SIZE 0x038u
#define OVL_L0_PITCH 0x044u
#define OVL_L0_ADDR 0xf40u
#define OVL_REQUIRED_SIZE 0xf44u
struct a04_sg_wait { KEVENT event; PSCATTER_GATHER_LIST list; };
static void a04_display_sg_complete(PDEVICE_OBJECT device, PIRP irp,
    PSCATTER_GATHER_LIST list, PVOID context)
{
    struct a04_sg_wait *wait=(struct a04_sg_wait *)context;
    UNREFERENCED_PARAMETER(device); UNREFERENCED_PARAMETER(irp);
    wait->list=list; KeSetEvent(&wait->event,IO_NO_INCREMENT,FALSE);
}
NTSTATUS a04_display_scanout_map(struct a04_display_scanout *s,
    struct a04_vendor_surface *surface, PDMA_ADAPTER adapter,
    PDEVICE_OBJECT device, PMDL mdl)
{
    struct a04_sg_wait wait; PSCATTER_GATHER_LIST sg; NTSTATUS status;
    uint64_t start=0, next=0, remaining; ULONG i;
    PFN_NUMBER *pfns;
    if (!s || !surface || !adapter || !device || !mdl || s->sg || s->surface ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL || !surface->prepared || surface->hardware_exposed ||
        !surface->pixels.cpu || !surface->pixels.adapter || surface->pixels.bytes!=surface->bytes || surface->width!=800 || surface->height!=1280 ||
        surface->pitch!=3200 || surface->bytes!=4096000 ||
        ((ULONG_PTR)surface->pixels.cpu & 4095u) ||
        MmGetMdlByteOffset(mdl)!=0 || MmGetMdlByteCount(mdl)!=surface->bytes ||
        !(mdl->MdlFlags & MDL_PAGES_LOCKED) ||
        !adapter->DmaOperations || !adapter->DmaOperations->GetScatterGatherList ||
        !adapter->DmaOperations->PutScatterGatherList)
        return STATUS_INVALID_PARAMETER;
    /* Match each locked page to the actual GPU common-buffer backing. Numeric
     * GPU DMA equality alone is deliberately not used as evidence. */
    pfns=MmGetMdlPfnArray(mdl);
    for (i=0;i<surface->bytes/4096u;i++) {
        PHYSICAL_ADDRESS phys=MmGetPhysicalAddress((UCHAR *)surface->pixels.cpu+(SIZE_T)i*4096u);
        if (phys.QuadPart<=0 || (phys.QuadPart & 4095) || (uint64_t)(phys.QuadPart >> 12)!=(uint64_t)pfns[i])
            return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    KeInitializeEvent(&wait.event,NotificationEvent,FALSE);wait.list=NULL;
    status=adapter->DmaOperations->GetScatterGatherList(adapter,device,mdl,
        MmGetMdlVirtualAddress(mdl),surface->bytes,a04_display_sg_complete,&wait,FALSE);
    if (!NT_SUCCESS(status)) return status;
    /* The callback owns this stack context until it signals; do not time out. */
    do { status=KeWaitForSingleObject(&wait.event,Executive,KernelMode,FALSE,NULL); }
    while (status!=STATUS_SUCCESS);
    sg=wait.list;
    if (!sg) return STATUS_DEVICE_CONFIGURATION_ERROR;
    remaining=surface->bytes;
    for(i=0;i<sg->NumberOfElements;i++) {
        uint64_t addr=(uint64_t)sg->Elements[i].Address.QuadPart;
        ULONG len=sg->Elements[i].Length;
        if (!len || len>remaining || addr>UINT32_MAX || len-1u>UINT32_MAX-addr ||
            (i && addr!=next)) goto invalid;
        if (!i) start=addr;
        next=addr+len;remaining-=len;
    }
    if (remaining || !start || (start&3u)) goto invalid;
    s->surface=surface;s->display_adapter=adapter;s->mdl=mdl;s->sg=sg;
    s->display_address=(uint32_t)start;return STATUS_SUCCESS;
invalid:
    adapter->DmaOperations->PutScatterGatherList(adapter,sg,FALSE);
    return STATUS_DEVICE_CONFIGURATION_ERROR;
}
NTSTATUS a04_display_scanout_unmap(struct a04_display_scanout *s)
{
    if (!s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !s->sg || !s->display_adapter)
        return STATUS_INVALID_PARAMETER;
    if (s->attempted) return STATUS_DEVICE_BUSY;
    s->display_adapter->DmaOperations->PutScatterGatherList(s->display_adapter,s->sg,FALSE);
    RtlZeroMemory(s,sizeof(*s));return STATUS_SUCCESS;
}
NTSTATUS a04_display_scanout_switch(struct a04_display_scanout *s,
    volatile void *base, ULONG length, uint32_t old, uint64_t fence,
    a04_display_ready_fn ready, a04_display_boundary_fn boundary, void *context)
{
    volatile ULONG *ovl=(volatile ULONG *)base;NTSTATUS status; ULONG con,format;
    if (!s || !s->sg || !s->surface || s->attempted || !base ||
        ((ULONG_PTR)base&3u) || length<OVL_REQUIRED_SIZE || !old || !fence ||
        !ready || !boundary || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    status=ready(context,fence);if(status!=STATUS_SUCCESS)return NT_SUCCESS(status)?STATUS_DEVICE_NOT_READY:status;
    status=boundary(context);if(status!=STATUS_SUCCESS)return NT_SUCCESS(status)?STATUS_DEVICE_NOT_READY:status;
    /* Linux defines memory source as LARC=0, and 32-bit formats as 2/3. */
    con=READ_REGISTER_ULONG(ovl+OVL_L0_CON/4);format=(con>>12)&15u;
    /* Never guess how to compose with firmware layer 3 or any other layer. */
    if ((con & ((3u<<28)|(3u<<9))) || (format!=2u && format!=3u) ||
        !(READ_REGISTER_ULONG(ovl+OVL_EN/4)&1u) ||
        READ_REGISTER_ULONG(ovl+OVL_SRC_CON/4)!=1u ||
        READ_REGISTER_ULONG(ovl+OVL_L0_SIZE/4)!=((1280u<<16)|800u) ||
        (READ_REGISTER_ULONG(ovl+OVL_L0_PITCH/4)&0xffffu)!=3200u ||
        READ_REGISTER_ULONG(ovl+OVL_L0_ADDR/4)!=old)
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    /* Mark both owners before the first write. No register readback or single
     * vblank is interpreted as latch or old-surface retirement. */
    if (!s->surface->hardware_exposed) {
        status=a04_vendor_surface_mark_exposed(s->surface);
        if (!NT_SUCCESS(status)) return status;
    }
    s->attempted=TRUE;s->saved_address=old;s->ovl=ovl;s->ovl_length=length;
    a04_gpu_dma_barrier();
    WRITE_REGISTER_ULONG(ovl+OVL_L0_ADDR/4,s->display_address);
    return STATUS_SUCCESS; /* write issued, not proof of latch or display */
}
#endif
