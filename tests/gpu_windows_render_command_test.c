/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "windows_firmware.h"
#include "pvr_rogue_fwif.h"
static struct a04_gpu_firmware_state s;
static unsigned mode, expected_type, expected_cleanup_type, expected_fw_addr;
unsigned KeGetCurrentIrql(void) { return PASSIVE_LEVEL; }
NTSTATUS a04_gpu_dma_sync(void *p,ULONG n,BOOLEAN wr) { (void)p;(void)n;(void)wr;return STATUS_SUCCESS; }
static void write32(void *context,uint32_t reg,uint32_t value) { (void)context;(void)reg;(void)value; }
NTSTATUS KeDelayExecutionThread(int x,BOOLEAN b,const LARGE_INTEGER *interval)
{
    struct rogue_fwif_ccb_ctl *ctl=(void *)((char *)s.memory.heap+s.runtime.kernel_ccb_control);
    struct rogue_fwif_kccb_cmd *cmd=(void *)((char *)s.memory.heap+s.runtime.kernel_ccb);
    volatile uint32_t *returns=(void *)((char *)s.memory.heap+s.runtime.kernel_return_slots);
    volatile struct rogue_fwif_osdata *os=(void *)((char *)s.memory.heap+s.runtime.os_data);
    uint32_t slot=ctl->read_offset;
    (void)x;(void)b;(void)interval;
    if(slot!=ctl->write_offset) {
        assert(cmd[slot].cmd_type==expected_type);
        if (expected_type==ROGUE_FWIF_KCCB_CMD_CLEANUP) {
            assert((unsigned)cmd[slot].cmd_data.cleanup_data.cleanup_type==expected_cleanup_type);
            assert(cmd[slot].cmd_data.cleanup_data.cleanup_data.context_fw_addr==expected_fw_addr);
        }
        if (mode==1) returns[slot]=ROGUE_FWIF_KCCB_RTN_SLOT_CMD_EXECUTED;
        if (mode==2) returns[slot]=ROGUE_FWIF_KCCB_RTN_SLOT_CMD_EXECUTED|ROGUE_FWIF_KCCB_RTN_SLOT_CLEANUP_BUSY;
        if (mode==3) returns[slot]=ROGUE_FWIF_KCCB_RTN_SLOT_POLL_FAILURE;
        ++os->kccb_cmds_executed;ctl->read_offset=ctl->write_offset;
    }
    return STATUS_SUCCESS;
}
static void init(void)
{
    struct rogue_fwif_ccb_ctl *ctl;
    memset(&s,0,sizeof(s));s.memory.heap=calloc(1,A04_PVR_HEAP_SIZE);assert(s.memory.heap);
    s.processor_released=TRUE;s.firmware_started=TRUE;s.io.write32=write32;
    s.runtime.kernel_ccb_control=0x10000;s.runtime.kernel_ccb=0x11000;
    s.runtime.kernel_return_slots=0x20000;s.runtime.power_sync=0x21000;
    s.runtime.mmu_sync=0x22000;s.runtime.os_data=0x23000;
    ctl=(void *)((char *)s.memory.heap+s.runtime.kernel_ccb_control);ctl->wrap_mask=127u;
}
int main(void)
{
    struct rogue_fwif_kccb_cmd cmd={0};
    init();expected_type=ROGUE_FWIF_KCCB_CMD_COMBINED_GEOM_FRAG_KICK; mode=1;cmd.cmd_type=expected_type;
    assert(a04_gpu_firmware_submit_kick(&s,&cmd)==STATUS_SUCCESS);
    expected_type=ROGUE_FWIF_KCCB_CMD_CLEANUP;
    expected_cleanup_type=ROGUE_FWIF_CLEANUP_HWRTDATA;expected_fw_addr=0xc0010000;
    assert(a04_gpu_firmware_cleanup(&s,expected_cleanup_type,expected_fw_addr)==STATUS_SUCCESS);
    mode=2;expected_cleanup_type=ROGUE_FWIF_CLEANUP_FREELIST;expected_fw_addr=0xc0010040;
    assert(a04_gpu_firmware_cleanup(&s,expected_cleanup_type,expected_fw_addr)==STATUS_DEVICE_BUSY);
    mode=0;expected_cleanup_type=ROGUE_FWIF_CLEANUP_FWCOMMONCONTEXT;expected_fw_addr=0xc0010080;
    assert(a04_gpu_firmware_cleanup(&s,expected_cleanup_type,expected_fw_addr)==STATUS_IO_TIMEOUT && s.queue_faulted);
    free(s.memory.heap);init();expected_type=ROGUE_FWIF_KCCB_CMD_CLEANUP;mode=3;
    expected_cleanup_type=ROGUE_FWIF_CLEANUP_HWRTDATA;expected_fw_addr=0xc0010000;
    assert(a04_gpu_firmware_cleanup(&s,expected_cleanup_type,expected_fw_addr)==STATUS_DEVICE_CONFIGURATION_ERROR && s.queue_faulted);
    free(s.memory.heap);return 0;
}
