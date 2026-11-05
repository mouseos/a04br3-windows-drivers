/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * m1.11 typed client-CCB UPDATE/PRIORITY KICK based on XNU
 * A04BR3RogueRenderResources.h / A04BR3RogueRenderSubmit.h at
 * f5d43bf1677955e7e367545b9bbbd6449776cdda.
 * Device memory uses aligned 32-bit accesses; no untrusted task packets.
 */
#include "windows_render_cold.h"
#include "windows_start.h"
#include "A04BR3RogueCommand.h"
#include "A04BR3RogueDeviceWords.h"
#include "../windows_hw.h"
#include <stddef.h>
enum { CLIENT_NULL=0x2abc80cfu, CLIENT_UPDATE=0x2abc00d6u, CLIENT_PRIORITY=0x2abc00d9u };
struct gp_packet { RGXFWIF_CCB_CMD_HEADER null_cmd, update; RGXFWIF_UFO ufo; };
struct priority_packet { RGXFWIF_CCB_CMD_HEADER header; uint32_t priority,padding; };
_Static_assert(sizeof(struct gp_packet)==88 && sizeof(struct priority_packet)==48,"m1.11 client packet ABI");
NTSTATUS a04_vendor_render_context_command(struct a04_vendor_render_cold *r,
    struct a04_vendor_start *s,unsigned index,uint32_t priority)
{
    struct a04_rgx_fw_runtime *fw;RGXFWIF_CCCB_CTL client;
    RGXFWIF_CCB_CTL kctl,fctl;RGXFWIF_KCCB_CMD kick={0};
    struct gp_packet gp={0};struct priority_packet pri={0};
    volatile unsigned char *bank;volatile uint32_t *sync,*priority_word;
    uint32_t context_offset,at,next,kread,kwrite,value,sequence=0;unsigned i,dm;
    NTSTATUS status=STATUS_DEVICE_BUSY;
    if(!r || !s || KeGetCurrentIrql()!=PASSIVE_LEVEL || index>3 || r->cleanup_started ||
       (index==2 ? priority!=0xffffffffu : priority>1u) || !r->mapped || r->poisoned ||
       !s->online || !s->memory || !s->memory->hardware_owned || r->firmware!=s->memory ||
       !(fw=s->memory->runtime) || !fw->kctl || !fw->fctl || !fw->kcommands || !s->core ||
       !r->buffer[0].cpu || r->layout.used>A04_VENDOR_RENDER_BANK_BYTES ||
       r->layout.control[index]>A04_VENDOR_RENDER_BANK_BYTES-sizeof(client) ||
       r->layout.commands[index]>A04_VENDOR_RENDER_BANK_BYTES-A04_VENDOR_CLIENT_RING_BYTES ||
       r->layout.sync>A04_VENDOR_RENDER_BANK_BYTES-sizeof(uint32_t))return STATUS_INVALID_PARAMETER;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0)return STATUS_DEVICE_BUSY;
    /* Cleanup can begin between the optimistic check and lock acquisition. */
    if(r->cleanup_started) {status=STATUS_INVALID_DEVICE_STATE;goto out;}
    if(s->command_poisoned || s->shutdown_started) {status=STATUS_IO_DEVICE_ERROR;goto out;}
    bank=(volatile unsigned char *)r->buffer[0].cpu;
    context_offset=index==0 ? r->layout.context+(uint32_t)offsetof(RGXFWIF_FWRENDERCONTEXT,sTAContext) :
        index==1 ? r->layout.context+(uint32_t)offsetof(RGXFWIF_FWRENDERCONTEXT,s3DContext) :
        index==2 ? r->layout.gp : r->layout.tq;
    if(context_offset>A04_VENDOR_RENDER_BANK_BYTES-sizeof(RGXFWIF_FWCOMMONCONTEXT)) {status=STATUS_INVALID_PARAMETER;goto out;}
    at=r->client_write[index];dm=index==0?2u:(index==2?0u:3u);
    if(at>4095u || (at&7u) || at+(index==2?88u:48u)+40u>4096u ||
       (index==2 && r->sequence>=0xfffffffeu)) {status=STATUS_INVALID_DEVICE_STATE;goto out;}
    a04_gpu_dma_barrier();
    if(a04_rgx_device_read_words(&client,bank+r->layout.control[index],sizeof(client)) ||
       a04_rgx_device_read_words(&kctl,fw->kctl,sizeof(kctl)) ||
       a04_rgx_device_read_words(&fctl,fw->fctl,sizeof(fctl))) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
    if(client.ui32WrapMask!=4095u || client.ui32ReadOffset!=at || client.ui32WriteOffset!=at ||
       client.ui32DepOffset>4095u || (client.ui32DepOffset&7u) ||
       kctl.ui32WrapMask!=127u || kctl.ui32CmdSize!=sizeof(kick) ||
       kctl.ui32ReadOffset>127u || kctl.ui32WriteOffset>127u ||
       fctl.ui32WrapMask!=31u || fctl.ui32CmdSize!=sizeof(RGXFWIF_FWCCB_CMD) ||
       fctl.ui32ReadOffset>31u || fctl.ui32WriteOffset>31u) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
    if(kctl.ui32ReadOffset!=kctl.ui32WriteOffset || fctl.ui32ReadOffset!=fctl.ui32WriteOffset)goto out;
    priority_word=(volatile uint32_t *)(void *)(bank+context_offset+offsetof(RGXFWIF_FWCOMMONCONTEXT,ui32Priority));
    sync=(volatile uint32_t *)(void *)(bank+r->layout.sync);
    if(index!=2 && *priority_word==priority){status=STATUS_INVALID_PARAMETER;goto out;}
    if(index==2) {
        sequence=++r->sequence;gp.null_cmd.eCmdType=CLIENT_NULL;gp.update.eCmdType=CLIENT_UPDATE;
        gp.update.ui32CmdSize=8;gp.ufo.puiAddrUFO.ui32Addr=0xc0080000u+r->layout.sync;
        gp.ufo.ui32Value=sequence;*sync=0;
        if(a04_rgx_device_write_words(bank+r->layout.commands[index]+at,&gp,sizeof(gp))) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
        next=at+sizeof(gp);
    } else {
        pri.header.eCmdType=CLIENT_PRIORITY;pri.header.ui32CmdSize=8;pri.priority=priority;
        if(a04_rgx_device_write_words(bank+r->layout.commands[index]+at,&pri,sizeof(pri))) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
        next=at+sizeof(pri);
    }
    kick.eCmdType=RGXFWIF_KCCB_CMD_KICK;kick.eDM=dm;
    kick.uCmdData.sCmdKickData.psContext.ui32Addr=0xc0080000u+context_offset;
    kick.uCmdData.sCmdKickData.ui32CWoffUpdate=next;
    if(a04_rgx_device_write_words(&fw->kcommands[kctl.ui32WriteOffset],&kick,sizeof(kick))) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
    a04_gpu_dma_barrier();kwrite=(kctl.ui32WriteOffset+1u)&127u;
    ((volatile RGXFWIF_CCB_CTL *)fw->kctl)->ui32WriteOffset=kwrite;
    a04_gpu_dma_barrier();r->client_write[index]=next;
    WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+0xb00),0x10);
    for(i=0;i<10000;i++) {
        a04_gpu_dma_barrier();
        kread=((volatile RGXFWIF_CCB_CTL *)fw->kctl)->ui32ReadOffset;
        if(a04_rgx_device_read_words(&client,bank+r->layout.control[index],sizeof(client)) || kread>127u ||
           client.ui32WrapMask!=4095u || client.ui32ReadOffset>4095u || (client.ui32ReadOffset&7u) ||
           client.ui32WriteOffset>4095u || (client.ui32WriteOffset&7u)) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
        value=index==2?*sync:*priority_word;
        if(kread==kwrite && client.ui32ReadOffset==next && client.ui32WriteOffset==next &&
           value==(index==2?sequence:priority)) {status=STATUS_SUCCESS;goto out;}
        KeStallExecutionProcessor(10);
    }
    status=STATUS_IO_TIMEOUT;
poison:
    r->poisoned=TRUE;s->command_poisoned=TRUE;
out:
    InterlockedExchange(&s->command_busy,0);return status;
}

/* m1.11 RGXFWRequestCommonContextCleanUp: drain the client CCB, issue
 * CLEANUP on its data master, and wait for RUN without BUSY. No memory
 * is released here; successful FW cleanup is not fabric quiescence. */
NTSTATUS a04_vendor_render_cleanup_contexts(struct a04_vendor_render_cold *r,
    struct a04_vendor_start *s,unsigned *failed_index)
{
    struct a04_rgx_fw_runtime *fw; RGXFWIF_CCB_CTL k,f; RGXFWIF_CCCB_CTL client;
    RGXFWIF_KCCB_CMD cmd; volatile unsigned char *bank; volatile uint32_t *sync;
    uint32_t off,next,value; unsigned index,i; NTSTATUS status=STATUS_DEVICE_BUSY;
    if(failed_index)*failed_index=4;
    if(!r || !s || !failed_index || KeGetCurrentIrql()!=PASSIVE_LEVEL || !r->mapped || r->poisoned ||
       (r->cleanup_mask&~15u) || !s->online || !s->memory || !s->memory->hardware_owned ||
       r->firmware!=s->memory || !(fw=s->memory->runtime) || !fw->kctl || !fw->fctl ||
       !fw->kcommands || !fw->command_sync || !fw->command_sync_addr.ui32Addr || !s->core ||
       !r->buffer[0].cpu || r->layout.used>A04_VENDOR_RENDER_BANK_BYTES) return STATUS_INVALID_PARAMETER;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0)return STATUS_DEVICE_BUSY;
    if(s->command_poisoned || s->shutdown_started){status=STATUS_IO_DEVICE_ERROR;goto out;}
    bank=(volatile unsigned char *)r->buffer[0].cpu;sync=(volatile uint32_t *)fw->command_sync;
    for(index=0;index<4;index++) {
        if(r->cleanup_mask&(1u<<index))continue;
        *failed_index=index;
        off=index==0 ? r->layout.context+(uint32_t)offsetof(RGXFWIF_FWRENDERCONTEXT,sTAContext) :
            index==1 ? r->layout.context+(uint32_t)offsetof(RGXFWIF_FWRENDERCONTEXT,s3DContext) :
            index==2 ? r->layout.gp : r->layout.tq;
        if(off>A04_VENDOR_RENDER_BANK_BYTES-sizeof(RGXFWIF_FWCOMMONCONTEXT) ||
           r->layout.control[index]>A04_VENDOR_RENDER_BANK_BYTES-sizeof(client) ||
           r->client_write[index]>4095u || (r->client_write[index]&7u)) {status=STATUS_INVALID_PARAMETER;goto out;}
        a04_gpu_dma_barrier();
        if(a04_rgx_device_read_words(&k,fw->kctl,sizeof(k)) || a04_rgx_device_read_words(&f,fw->fctl,sizeof(f)) ||
           a04_rgx_device_read_words(&client,bank+r->layout.control[index],sizeof(client)) ||
           k.ui32WrapMask!=127u || k.ui32CmdSize!=sizeof(cmd) || k.ui32ReadOffset>127u || k.ui32WriteOffset>127u ||
           f.ui32WrapMask!=31u || f.ui32CmdSize!=sizeof(RGXFWIF_FWCCB_CMD) || f.ui32ReadOffset>31u || f.ui32WriteOffset>31u ||
           client.ui32WrapMask!=4095u || client.ui32ReadOffset>4095u || client.ui32WriteOffset>4095u) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
        if(k.ui32ReadOffset!=k.ui32WriteOffset || f.ui32ReadOffset!=f.ui32WriteOffset ||
           client.ui32ReadOffset!=r->client_write[index] || client.ui32WriteOffset!=r->client_write[index]) goto out;
        RtlZeroMemory(&cmd,sizeof(cmd)); cmd.eCmdType=RGXFWIF_KCCB_CMD_CLEANUP;
        cmd.eDM=index==0?2u:(index==2?0u:3u);
        cmd.uCmdData.sCleanupData.eCleanupType=RGXFWIF_CLEANUP_FWCOMMONCONTEXT;
        cmd.uCmdData.sCleanupData.uCleanupData.psContext.ui32Addr=0xc0080000u+off;
        cmd.uCmdData.sCleanupData.sSyncObjDevVAddr=fw->command_sync_addr;*sync=0;
        if(a04_rgx_device_write_words(&fw->kcommands[k.ui32WriteOffset],&cmd,sizeof(cmd))) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
        r->cleanup_started=TRUE;
        a04_gpu_dma_barrier();next=(k.ui32WriteOffset+1u)&127u;
        ((volatile RGXFWIF_CCB_CTL *)fw->kctl)->ui32WriteOffset=next;
        a04_gpu_dma_barrier();WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+0xb00),0x10);
        status=STATUS_IO_TIMEOUT;
        for(i=0;i<20000u;i++){
            a04_gpu_dma_barrier();value=*sync;
            if(value&~3u){status=STATUS_IO_DEVICE_ERROR;goto poison;}
            if((value&1u) && ((volatile RGXFWIF_CCB_CTL *)fw->kctl)->ui32ReadOffset==next){
                if(value&2u){status=STATUS_DEVICE_BUSY;goto out;}
                r->cleanup_mask|=1u<<index;status=STATUS_SUCCESS;break;
            }
            KeStallExecutionProcessor(50);
        }
        if(!NT_SUCCESS(status))goto poison;
    }
    *failed_index=4;status=STATUS_SUCCESS;goto out;
poison:
    r->poisoned=TRUE;s->command_poisoned=TRUE;
out:
    InterlockedExchange(&s->command_busy,0);return status;
}
