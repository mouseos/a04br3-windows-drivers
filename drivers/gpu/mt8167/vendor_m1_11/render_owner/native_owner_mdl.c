/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * WDDM1.x MAP_APERTURE_SEGMENT supplies locked PHYSICAL system pages.
 * This MT8167 UMA aperture programs its own MMU with those physical PFNs;
 * it is not a packet-DMA transfer and owns no HAL SG/map-register lease.
 * See docs/gpu332-wddm-aperture.md for OS/platform/lifetime provenance.
 */
#include "native_owner_mdl.h"
#include "../../windows_hw.h"
#define TAG 0x644d474eu
#define MAX_BYTES 4194304u
struct a04_native_mdl_binding {
    PDMA_ADAPTER adapter;PMDL mdl;void *cpu;ULONG bytes,pages,targets;
    struct a04_native_mdl_lease lease;
    uint64_t physical[1024];BOOLEAN retained,exposed,pte_live,gpu_owned;
};
NTSTATUS a04_native_mdl_release_cold(struct a04_native_mdl_binding **owner)
{
    struct a04_native_mdl_binding *b;
    if(!owner || !(b=*owner) || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    if(b->exposed || b->targets || b->gpu_owned)return STATUS_DEVICE_BUSY;
    if(b->retained)b->lease.release(b->lease.context,b->mdl);
    ExFreePoolWithTag(b,TAG);*owner=NULL;return STATUS_SUCCESS;
}
static NTSTATUS bind_pages(struct a04_native_mdl_binding **out,PDMA_ADAPTER a,
 PDEVICE_OBJECT pdo,PMDL mdl,ULONG bytes,const struct a04_native_mdl_lease *lease)
{
    struct a04_native_mdl_binding *b;PFN_NUMBER *pfns;NTSTATUS status;ULONG i,j;
    if(!out || *out || !a || !pdo || !mdl || !lease || !lease->context || !lease->acquire || !lease->release ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL || !bytes || bytes>MAX_BYTES || (bytes&4095u) ||
       MmGetMdlByteOffset(mdl) || MmGetMdlByteCount(mdl)!=bytes || !(mdl->MdlFlags&MDL_PAGES_LOCKED) ||
       (mdl->MdlFlags&MDL_IO_SPACE) || !MmGetMdlVirtualAddress(mdl))return STATUS_INVALID_PARAMETER;
    pfns=MmGetMdlPfnArray(mdl);
    for(i=0;i<bytes/4096u;i++){
        if(!pfns[i] || (uint64_t)pfns[i]>0xfffffu)return STATUS_DEVICE_CONFIGURATION_ERROR;
        for(j=0;j<i;j++)if(pfns[i]==pfns[j])return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    b=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*b),TAG);if(!b)return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(b,sizeof(*b));b->adapter=a;b->mdl=mdl;b->bytes=bytes;b->pages=bytes/4096u;b->lease=*lease;
    /* Frontend acquire validates the exact independently pinned WDDM MDL,
     * MAP provenance, map_ref and Normal-RAM allocation origin. No user MDL,
     * arbitrary physical address, MmMapIoSpace or synthetic PFN is admitted. */
    status=lease->acquire(lease->context,mdl);if(!NT_SUCCESS(status)){ExFreePoolWithTag(b,TAG);return status;}
    b->retained=TRUE;b->cpu=MmGetSystemAddressForMdlSafe(mdl,NormalPagePriority);
    if(!b->cpu || ((uintptr_t)b->cpu&4095u)){struct a04_native_mdl_binding *drop=b;(void)a04_native_mdl_release_cold(&drop);return STATUS_INSUFFICIENT_RESOURCES;}
    for(i=0;i<b->pages;i++)b->physical[i]=(uint64_t)pfns[i]<<12;
    *out=b;return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_sync_for_gpu(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    /* CPU writes must reach RAM before the GPU can fetch the supplied PFNs.
     * Physical-page retention is independent of cache ownership. */
    KeFlushIoBuffers(b->mdl,FALSE,TRUE);a04_gpu_dma_barrier();
    /* Cold CPU preparation precedes first root publication. On a retained
     * live aperture this transfers ownership only after the caller's real
     * previous GPU fence; it does not release/map any physical pages. */
    if(b->exposed)b->gpu_owned=TRUE;return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_sync_for_cpu(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    /* Caller proved actual task/SLC/GP completion or parent-off/IRQ-drain.
     * Residency PTEs remain valid until WDDM UNMAP; an idle mapped page is
     * not a HAL transfer resource. There is no HAL adapter buffer to
     * flush/free: WDDM supplied physical pages, not a DMA logical address. */
    KeFlushIoBuffers(b->mdl,TRUE,TRUE);a04_gpu_dma_barrier();b->gpu_owned=FALSE;return STATUS_SUCCESS;
}
const uint64_t *a04_native_mdl_pages(const struct a04_native_mdl_binding *b,ULONG *count)
{if(!b || !count || !b->retained)return NULL;*count=b->pages;return b->physical;}
const uint64_t *a04_native_mdl_physical_identity(const struct a04_native_mdl_binding *b,ULONG *count)
{if(!b || !count)return NULL;*count=b->pages;return b->physical;}
void *a04_native_mdl_cpu(const struct a04_native_mdl_binding *b)
{return b && !b->gpu_owned?b->cpu:NULL;}
const void *a04_native_mdl_cpu_identity(const struct a04_native_mdl_binding *b){return b?b->cpu:NULL;}
PMDL a04_native_mdl_original(const struct a04_native_mdl_binding *b){return b?b->mdl:NULL;}
NTSTATUS a04_native_mdl_mark_exposed(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || b->exposed || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->exposed=TRUE;b->pte_live=TRUE;b->gpu_owned=TRUE;return STATUS_SUCCESS;
}
ULONG a04_native_mdl_extent(const struct a04_native_mdl_binding *b){return b?b->bytes:0;}
PDMA_ADAPTER a04_native_mdl_adapter(const struct a04_native_mdl_binding *b){return b?b->adapter:NULL;}
NTSTATUS a04_native_mdl_hold_cold(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || b->pte_live || b->targets || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->targets=1;return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_drop_cold(struct a04_native_mdl_binding *b)
{
    if(!b || b->exposed || b->pte_live || b->gpu_owned || b->targets!=1 || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->targets=0;return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_retire_after_gpu_off(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->pte_live=FALSE;b->exposed=FALSE;return a04_native_mdl_sync_for_cpu(b);
}
NTSTATUS a04_native_mdl_bind(struct a04_native_mdl_binding **out,PDMA_ADAPTER a,PDEVICE_OBJECT pdo,PMDL mdl,ULONG bytes,const struct a04_native_mdl_lease *lease)
{return bind_pages(out,a,pdo,mdl,bytes,lease);}
NTSTATUS a04_native_mdl_bind_input(struct a04_native_mdl_binding **out,PDMA_ADAPTER a,PDEVICE_OBJECT pdo,PMDL mdl,ULONG bytes,const struct a04_native_mdl_lease *lease)
{return bind_pages(out,a,pdo,mdl,bytes,lease);}
