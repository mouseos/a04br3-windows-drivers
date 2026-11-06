/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental m1.11 live PC slot and MMUCACHE transaction. The m1.11
 * command layout comes from A04BR3RogueDDKABI.h; 0x1f is the PC/PD/PT/TLB
 * flag combination in MTK m1.8 DDK rgx_fwif_km.h. Its m1.11 hardware effect
 * on this BVNC is NOT established. Compile and call only under the separate
 * explicit experiment flag; never infer display visibility or DMA stop. */
#include "windows_shared_tables.h"
#include "windows_start.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
#include "../windows_hw.h"
#define SHARED_PC_INDEX 512u
#define MMU_FLAGS_EXPERIMENT 0x1fu
_Static_assert(offsetof(RGXFWIF_MMUCACHEDATA,sMemoryContext)==0 &&
    offsetof(RGXFWIF_MMUCACHEDATA,ui32Flags)==4 &&
    offsetof(RGXFWIF_MMUCACHEDATA,sMMUCacheSync)==8 &&
    offsetof(RGXFWIF_MMUCACHEDATA,ui16MMUCacheSyncUpdateValue)==12,"m1.11 MMUCACHE ABI");
NTSTATUS a04_shared_tables_publish(struct a04_shared_tables *t,struct a04_shared_scanout *shared,
    struct a04_vendor_render_cold *r,struct a04_vendor_start *s)
{
    struct a04_rgx_fw_runtime *fw;RGXFWIF_CCB_CTL k,f;RGXFWIF_KCCB_CMD cmd={0};
    RGXFWIF_FWMEMCONTEXT mem;uint32_t old=0,entry=0,next,sequence;unsigned i;
    NTSTATUS status=STATUS_DEVICE_BUSY;
    if(!t || !shared || !r || !s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !t->ready ||
       t->quarantined || shared->exposed || !shared->mdl || !shared->gpu_sg || !shared->display_sg ||
       !r->mapped || !r->clear_attempted || !r->clear_verified || r->poisoned || r->cleanup_started ||
       !r->copy_owner || !r->buffer[0].cpu || r->buffer[0].bytes!=65536 || !r->buffer[1].cpu || r->buffer[1].bytes!=4096 ||
       !r->firmware || r->firmware!=s->memory || !s->memory->hardware_owned || !s->online ||
       !s->core || !(fw=s->memory->runtime) || !fw->kctl || !fw->fctl || !fw->kcommands ||
       !fw->command_sync || !fw->command_sync_addr.ui32Addr ||
       r->layout.memory>65536u-sizeof(mem) || s->command_sequence>=65534u ||
       !t->table[0].cpu || !t->table[1].cpu || !t->table[2].cpu || !t->table[3].cpu)
        return STATUS_INVALID_PARAMETER;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0)return STATUS_DEVICE_BUSY;
    if(s->shutdown_started || s->command_poisoned || r->cleanup_started || r->poisoned) {
        status=STATUS_INVALID_DEVICE_STATE;goto out;
    }
    a04_gpu_dma_barrier();
    if(a04_rgx_device_read_words(&k,fw->kctl,sizeof(k)) || a04_rgx_device_read_words(&f,fw->fctl,sizeof(f)) ||
       a04_rgx_device_read_words(&mem,(UCHAR *)r->buffer[0].cpu+r->layout.memory,sizeof(mem)) ||
       a04_rgx_device_read_words(&old,(UCHAR *)r->buffer[1].cpu+SHARED_PC_INDEX*4u,4) ||
       a04_rgx_device_read_words(&entry,(UCHAR *)t->table[0].cpu+SHARED_PC_INDEX*4u,4) ||
       k.ui32WrapMask!=127u || k.ui32CmdSize!=sizeof(cmd) || k.ui32ReadOffset>127u || k.ui32WriteOffset>127u ||
       f.ui32WrapMask!=31u || f.ui32CmdSize!=sizeof(RGXFWIF_FWCCB_CMD) || f.ui32ReadOffset>31u || f.ui32WriteOffset>31u ||
       mem.sPCDevPAddr.uiAddr!=(uint64_t)r->buffer[1].logical.QuadPart || old ||
       entry!=((uint32_t)((uint64_t)t->table[1].logical.QuadPart>>8)|1u)) {
        status=STATUS_DEVICE_CONFIGURATION_ERROR;goto out;
    }
    if(k.ui32ReadOffset!=k.ui32WriteOffset || f.ui32ReadOffset!=f.ui32WriteOffset)goto out;
    sequence=++s->command_sequence;
    cmd.eCmdType=RGXFWIF_KCCB_CMD_MMUCACHE;cmd.eDM=0;
    cmd.uCmdData.sMMUCacheData.sMemoryContext.ui32Addr=0xc0080000u+r->layout.memory;
    cmd.uCmdData.sMMUCacheData.ui32Flags=MMU_FLAGS_EXPERIMENT;
    cmd.uCmdData.sMMUCacheData.sMMUCacheSync=fw->command_sync_addr;
    cmd.uCmdData.sMMUCacheData.ui16MMUCacheSyncUpdateValue=(uint16_t)sequence;
    /* Hardware may observe the root immediately. There is no validated
     * removal/retirement path: retain both tables and MDL even on failure. */
    shared->exposed=TRUE;t->quarantined=TRUE;t->published=TRUE;
    if(a04_rgx_device_write_words((UCHAR *)r->buffer[1].cpu+SHARED_PC_INDEX*4u,&entry,4)) {
        status=STATUS_IO_DEVICE_ERROR;goto poison;
    }
    *(volatile uint32_t *)fw->command_sync=0;
    if(a04_rgx_device_write_words(&fw->kcommands[k.ui32WriteOffset],&cmd,sizeof(cmd))) {
        status=STATUS_IO_DEVICE_ERROR;goto poison;
    }
    a04_gpu_dma_barrier();next=(k.ui32WriteOffset+1u)&127u;
    ((volatile RGXFWIF_CCB_CTL *)fw->kctl)->ui32WriteOffset=next;
    a04_gpu_dma_barrier();WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+0xb00),0x10);
    for(i=0;i<20000u;i++) {
        uint32_t read,value,write;
        a04_gpu_dma_barrier();read=((volatile RGXFWIF_CCB_CTL *)fw->kctl)->ui32ReadOffset;
        write=((volatile RGXFWIF_CCB_CTL *)fw->kctl)->ui32WriteOffset;value=*(volatile uint32_t *)fw->command_sync;
        if(read>127u || write!=next || (value && value!=sequence)) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
        if(read==next && value==sequence){t->mmu_ack=TRUE;status=STATUS_SUCCESS;goto out;}
        KeStallExecutionProcessor(50);
    }
    status=STATUS_IO_TIMEOUT;
poison:
    r->poisoned=TRUE;s->command_poisoned=TRUE;
out:
    InterlockedExchange(&s->command_busy,0);return status;
}
