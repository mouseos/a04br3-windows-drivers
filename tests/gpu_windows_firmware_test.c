/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <stdio.h>
#include <stdlib.h>
#include "windows_firmware.h"
#include "pvr_rogue_fwif.h"
#include "pvr_rogue_defs.h"
static struct a04_gpu_firmware_state state;
static unsigned allocations, releases, signal_start, commands_completed, command_error, sync_read, sync_write, no_mmu_ack;
static void *allocate(PDMA_ADAPTER adapter,ULONG size,PHYSICAL_ADDRESS *pa,BOOLEAN cache)
{
    (void)adapter; (void)cache;
    pa->QuadPart=allocations++ ? 0x90000000 : 0x80000000;
    return aligned_alloc(64,size);
}
static void release(PDMA_ADAPTER a,ULONG size,PHYSICAL_ADDRESS pa,void *memory,BOOLEAN cache)
{ (void)a; (void)size; (void)pa; (void)cache; ++releases; free(memory); }
unsigned KeGetCurrentIrql(void) { return PASSIVE_LEVEL; }
void KeStallExecutionProcessor(unsigned delay) { (void)delay; }
PMDL IoAllocateMdl(void *p,ULONG n,BOOLEAN a,BOOLEAN b,void *c) { (void)a; (void)b; (void)c; return ((uintptr_t)p%64 || n%64) ? NULL : p; }
void MmBuildMdlForNonPagedPool(PMDL p) { (void)p; }
void KeFlushIoBuffers(PMDL p,BOOLEAN read,BOOLEAN dma)
{ (void)p; if(dma) { if(read) ++sync_read; else ++sync_write; } }
void IoFreeMdl(PMDL p) { (void)p; }
NTSTATUS KeDelayExecutionThread(int mode,BOOLEAN alertable,const LARGE_INTEGER *interval)
{
    (void)mode; (void)alertable; (void)interval;
    if(signal_start) {
        struct rogue_fwif_sysinit *sys=(void *)((uint8_t *)state.memory.heap+state.runtime.system_init);
        sys->firmware_started=1;
        if(state.firmware_started) {
            uint8_t *heap=state.memory.heap;
            struct rogue_fwif_ccb_ctl *ctl=(void *)(heap+state.runtime.kernel_ccb_control);
            struct rogue_fwif_kccb_cmd *cmds=(void *)(heap+state.runtime.kernel_ccb);
            volatile uint32_t *ret=(void *)(heap+state.runtime.kernel_return_slots);
            volatile uint32_t *power=(void *)(heap+state.runtime.power_sync);
            volatile uint32_t *mmu=(void *)(heap+state.runtime.mmu_sync);
            volatile struct rogue_fwif_osdata *os=(void *)(heap+state.runtime.os_data);
            uint32_t slot=ctl->read_offset;
            if(slot!=ctl->write_offset) {
                if(commands_completed==0 && cmds[slot].cmd_type!=ROGUE_FWIF_KCCB_CMD_HEALTH_CHECK) command_error=1;
                if(commands_completed==1) {
                    if(cmds[slot].cmd_type!=ROGUE_FWIF_KCCB_CMD_MMUCACHE ||
                       cmds[slot].cmd_data.mmu_cache_data.mmu_cache_sync_fw_addr!=(0xc0000000u|state.runtime.mmu_sync) ||
                       !cmds[slot].cmd_data.mmu_cache_data.mmu_cache_sync_update_value) command_error=1;
                    if(!no_mmu_ack) *mmu=cmds[slot].cmd_data.mmu_cache_data.mmu_cache_sync_update_value;
                }
                if(commands_completed==2 && cmds[slot].cmd_type!=ROGUE_FWIF_KCCB_CMD_KICK) command_error=1;
                if(commands_completed==3 && (cmds[slot].cmd_type!=ROGUE_FWIF_KCCB_CMD_POW || cmds[slot].cmd_data.pow_data.pow_type!=ROGUE_FWIF_POW_FORCED_IDLE_REQ)) command_error=1;
                if(commands_completed==4 && (cmds[slot].cmd_type!=ROGUE_FWIF_KCCB_CMD_POW || cmds[slot].cmd_data.pow_data.pow_type!=ROGUE_FWIF_POW_OFF_REQ)) command_error=1;
                /* The MT8167 firmware can advance the execution counter
                 * without writing the health command's return slot. */
                if(commands_completed!=0) ret[slot]=ROGUE_FWIF_KCCB_RTN_SLOT_CMD_EXECUTED;
                *power=1; ++os->kccb_cmds_executed;
                ctl->read_offset=ctl->write_offset; ++commands_completed;
            }
        }
    }
    return STATUS_SUCCESS;
}
int main(int argc,char **argv)
{
    FILE *f; long n; uint8_t *image,*regs; NTSTATUS status; struct rogue_fwif_kccb_cmd kick={0};
    DMA_OPERATIONS ops={allocate,release}; struct fake_adapter adapter={&ops};
    if(argc!=2 || !(f=fopen(argv[1],"rb"))) return 1;
    if(fseek(f,0,SEEK_END) || (n=ftell(f))<=0 || fseek(f,0,SEEK_SET)) return 2;
    image=malloc((size_t)n); regs=calloc(1,0x80000);
    if(!image || !regs || fread(image,1,(size_t)n,f)!=(size_t)n) return 3;
    fclose(f);
    status=a04_gpu_firmware_boot(&state,&adapter,regs,0x80000,image,(size_t)n,A04_PVR_BVNC,FALSE,26000000);
    if(status!=STATUS_NOT_SUPPORTED || allocations || state.processor_released) return 4;
    signal_start=1;
    status=a04_gpu_firmware_boot(&state,&adapter,regs,0x80000,image,(size_t)n,A04_PVR_BVNC,TRUE,26000000);
    if(status!=STATUS_SUCCESS || !state.processor_released || !state.firmware_started || allocations!=2 || releases || !sync_read || sync_write<2) return 5;
    if(a04_gpu_firmware_health(&state)!=STATUS_SUCCESS || commands_completed!=1 || command_error) return 10;
    if(a04_gpu_firmware_invalidate_mmu(&state)!=STATUS_SUCCESS || commands_completed!=2 || command_error) return 11;
    if(a04_gpu_firmware_submit_kick(&state,&kick)!=STATUS_INVALID_PARAMETER || commands_completed!=2) return 13;
    kick.cmd_type=ROGUE_FWIF_KCCB_CMD_KICK;
    if(a04_gpu_firmware_submit_kick(&state,&kick)!=STATUS_SUCCESS || commands_completed!=3 || command_error) return 14;
    if(a04_gpu_firmware_stop(&state,FALSE)!=STATUS_DEVICE_BUSY || releases || !state.processor_released) return 7;
    if(a04_gpu_firmware_shutdown(&state)!=STATUS_IO_TIMEOUT || commands_completed!=5 || command_error || releases || !state.processor_released) return 8;
    *(uint32_t *)(void *)(regs+ROGUE_CR_SIDEKICK_IDLE)=(uint32_t)ROGUE_CR_SIDEKICK_IDLE_MASKFULL;
    *(uint32_t *)(void *)(regs+ROGUE_CR_SLC_IDLE)=(uint32_t)ROGUE_CR_SLC_IDLE_MASKFULL;
    if(a04_gpu_firmware_stop(&state,TRUE)!=STATUS_SUCCESS || releases!=2 || state.processor_released) return 9;
    memset(regs,0,0x80000); memset(&state,0,sizeof(state)); allocations=releases=0; signal_start=1; commands_completed=1; no_mmu_ack=1;
    status=a04_gpu_firmware_boot(&state,&adapter,regs,0x80000,image,(size_t)n,A04_PVR_BVNC,TRUE,26000000);
    if(status!=STATUS_SUCCESS || a04_gpu_firmware_invalidate_mmu(&state)!=STATUS_IO_TIMEOUT ||
       !state.queue_faulted || !state.processor_released || releases) return 12;
    a04_gpu_free_memory(&state.memory); no_mmu_ack=0;
    memset(regs,0,0x80000); memset(&state,0,sizeof(state)); allocations=releases=0; signal_start=0;
    status=a04_gpu_firmware_boot(&state,&adapter,regs,0x80000,image,(size_t)n,A04_PVR_BVNC,TRUE,26000000);
    if(status!=STATUS_IO_TIMEOUT || !state.processor_released || state.firmware_started || allocations!=2 || releases) return 6;
    a04_gpu_free_memory(&state.memory); free(regs); free(image);
    puts("Windows GPU boot composition and timeout lifetime checks passed");
    return 0;
}
