/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_start.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueStartupExecutor.h"
#include "A04BR3RogueDeviceWords.h"
#include "windows_uncached.h"
#include "../windows_hw.h"
#define TRAMPOLINE_BYTES 0x4000u
#define TRAMPOLINE_ALLOCATION (2u*TRAMPOLINE_BYTES)
struct start_io { struct a04_vendor_start *owner; NTSTATUS sync_status; };
static uint32_t rd32(void *ctx,uint32_t off)
{
    struct start_io *io=ctx;
    return READ_REGISTER_ULONG((volatile ULONG *)(void *)(io->owner->core+off));
}
static uint64_t rd64(void *ctx,uint32_t off)
{
    uint32_t high=rd32(ctx,off+4),low=rd32(ctx,off);
    return ((uint64_t)high<<32)|low;
}
static void wr32(void *ctx,uint32_t off,uint32_t v)
{
    struct start_io *io=ctx;
    WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(io->owner->core+off),v);
}
static void wr64(void *ctx,uint32_t off,uint64_t v)
{
    wr32(ctx,off,(uint32_t)v);wr32(ctx,off+4,(uint32_t)(v>>32));
}
static void delay(void *ctx,uint32_t us)
{ UNREFERENCED_PARAMETER(ctx);KeStallExecutionProcessor(us); }
static uint32_t shared(void *ctx,uint32_t physical)
{
    struct start_io *io=ctx;struct a04_vendor_buffer *b=&io->owner->memory->buffers[4];
    uint64_t offset;
    if (physical<b->dma || (physical&3u)) return 0xffffffffu;
    offset=(uint64_t)physical-b->dma;
    if (offset>b->bytes-4u) return 0xffffffffu;
    io->sync_status=io->owner->memory->uncached ? STATUS_SUCCESS :
        a04_gpu_dma_sync((unsigned char *)b->cpu+(size_t)offset,4,TRUE);
    if (!NT_SUCCESS(io->sync_status)) return 0xffffffffu;
    a04_gpu_dma_barrier();
    return *(volatile uint32_t *)((unsigned char *)b->cpu+(size_t)offset);
}
static int power_valid(const struct a04_vendor_power_snapshot *p)
{
    const uint32_t domains=(1u<<25)|(1u<<24)|(1u<<4);
    return p && (p->spm_status&domains)==domains &&
        (p->spm_status_2&domains)==domains && !(p->mfg_clock_status&15u) &&
        ((p->top_mux0>>8)&7u)==4u && ((p->top_mux1>>8)&63u)==8u &&
        !(p->top_slow&((3u<<18)|(3u<<20)));
}
NTSTATUS a04_vendor_m1_11_windows_start(struct a04_vendor_start *s,
    struct a04_vendor_m1_11_memory *m,volatile UCHAR *core,ULONG core_bytes,
    uint64_t observed_bvnc,const struct a04_vendor_power_snapshot *power,BOOLEAN irq_is_masked)
{
    struct a04_rgx_start_input input={0};struct a04_rgx_start_plan plan;
    struct start_io context;struct a04_rgx_exec_io io;
    uint64_t addr,off;unsigned i,tries;uint32_t irq=0,updated;int result;
    NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;
    if(!s || !m || !core || KeGetCurrentIrql()!=PASSIVE_LEVEL || s->attempted ||
       s->trampoline_allocation || !m->adapter || !m->staged || m->hardware_owned ||
       core_bytes<0xa104u || ((ULONG_PTR)core&3u) || !power_valid(power) || irq_is_masked!=TRUE ||
       observed_bvnc!=0x001600280036001eULL || !m->runtime || !m->runtime->init)
        return STATUS_INVALID_DEVICE_STATE;
    s->memory=m;s->core=core;
    if(m->uncached) {
        struct a04_vendor_uncached allocation={0};
        status=a04_vendor_uncached_allocate(&allocation,m->adapter,TRAMPOLINE_ALLOCATION);
        if(!NT_SUCCESS(status)) return status;
        s->trampoline_allocation=allocation.cpu;s->trampoline_logical=allocation.logical;
    } else s->trampoline_allocation=m->adapter->DmaOperations->AllocateCommonBuffer(m->adapter,
        TRAMPOLINE_ALLOCATION,&s->trampoline_logical,FALSE);
    if(!s->trampoline_allocation) return STATUS_INSUFFICIENT_RESOURCES;
    status=STATUS_DEVICE_CONFIGURATION_ERROR;
    addr=(uint64_t)s->trampoline_logical.QuadPart;
    off=(0u-addr)&(TRAMPOLINE_BYTES-1u);
    if(addr>UINT64_MAX-off || (uintptr_t)s->trampoline_allocation>UINTPTR_MAX-(uintptr_t)off) goto fail;
    s->trampoline_dma=addr+off;s->trampoline_cpu=(unsigned char *)s->trampoline_allocation+(size_t)off;
    if(!a04_rgx_mips_physical_range(s->trampoline_dma,TRAMPOLINE_BYTES,TRAMPOLINE_BYTES) ||
       !a04_rgx_fw_pointer(s->trampoline_cpu,TRAMPOLINE_BYTES) ||
       a04_rgx_fw_overlap(s->trampoline_dma,TRAMPOLINE_BYTES,0x1fc00000u,TRAMPOLINE_BYTES)) goto fail;
    for(i=0;i<5;i++) if(a04_rgx_fw_overlap(s->trampoline_dma,TRAMPOLINE_BYTES,m->buffers[i].dma,m->buffers[i].bytes) ||
       a04_rgx_fw_alias(s->trampoline_cpu,TRAMPOLINE_BYTES,m->buffers[i].cpu,m->buffers[i].bytes)) goto fail;
    if(m->uncached) {
        volatile uint32_t *words=(volatile uint32_t *)s->trampoline_cpu;
        if((uintptr_t)words&3u) goto fail;
        for(i=0;i<TRAMPOLINE_BYTES/sizeof(*words);i++) words[i]=0;
        a04_gpu_dma_barrier();status=STATUS_SUCCESS;
    } else {
        RtlZeroMemory(s->trampoline_cpu,TRAMPOLINE_BYTES);
        status=a04_gpu_dma_sync(s->trampoline_cpu,TRAMPOLINE_BYTES,FALSE);
    }
    if(!NT_SUCCESS(status)) goto fail;
    input.bvnc=observed_bvnc;input.table_pa=m->buffers[0].dma;input.code_pa=m->buffers[1].dma;
    input.data_pa=m->buffers[2].dma;input.init_pa=m->buffers[4].dma;
    input.init_bytes=m->buffers[4].bytes;input.started_offset=offsetof(RGXFWIF_INIT,bFirmwareStarted);
    input.trampoline_pa=s->trampoline_dma;input.strict_same_address_ordering=1;
    input.poll_budget=20000;input.poll_delay_us=50;input.evidence=A04_START_ALL;
    context.owner=s;context.sync_status=STATUS_SUCCESS;
    input.ready_initial=shared(&context,(uint32_t)(input.init_pa+input.started_offset));
    if(!NT_SUCCESS(context.sync_status) || a04_rgx_start_plan(&input,&plan)!=A04_START_OK) {
        status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;
    }
    io.context=&context;io.read32=rd32;io.read64=rd64;io.write32=wr32;io.write64=wr64;
    io.wait_us=delay;io.shared32=shared;
    /* From this point neither the stage nor trampoline may be freed until a
     * separate implementation proves both GPU reset and fabric quiescence. */
    m->hardware_owned=TRUE;s->attempted=TRUE;
    result=a04_rgx_execute_start(&input,&io,&irq);
    if(!result) {
        uint32_t comp=(uint32_t)(input.init_pa+offsetof(RGXFWIF_INIT,sRGXCompChecks)+offsetof(RGXFWIF_COMPCHECKS,bUpdated));
        for(tries=0;tries<1000;tries++) {
            updated=shared(&context,comp);
            if(updated==1) break;
            if(updated!=0) { result=A04_EXEC_BUS;break; }
            delay(&context,10);
        }
        if(tries==1000) result=A04_EXEC_TIMEOUT;
        if(!result) {
            RGXFWIF_COMPCHECKS *c=&m->runtime->init->sRGXCompChecks;
            RGXFWIF_COMPCHECKS snapshot;
            status=m->uncached ? STATUS_SUCCESS : a04_gpu_dma_sync(c,sizeof(*c),TRUE);
            if(!NT_SUCCESS(status) || a04_rgx_device_read_words(&snapshot,c,sizeof(snapshot)) ||
               snapshot.ui32DDKVersion!=0x1000b || snapshot.ui32DDKBuild!=5425693 ||
               snapshot.ui32FWProcessorVersion!=1 || snapshot.sFWBVNC.ui32LayoutVersion!=3 ||
               snapshot.sFWBVNC.ui64BVNC!=input.bvnc || snapshot.sHWBVNC.ui64BVNC!=input.bvnc) result=A04_EXEC_BUS;
        }
    }
    if(irq&1u) wr32(&context,0x8b0,1);
    if(result) { wr64(&context,0x100,0x100000000ULL);(void)rd64(&context,0x100);delay(&context,3);return STATUS_IO_DEVICE_ERROR; }
    s->online=TRUE;return STATUS_SUCCESS;
fail:
    m->adapter->DmaOperations->FreeCommonBuffer(m->adapter,TRAMPOLINE_ALLOCATION,s->trampoline_logical,s->trampoline_allocation,FALSE);
    RtlZeroMemory(s,sizeof(*s));return status;
}
