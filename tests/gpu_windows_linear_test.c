/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Host simulation of fixed linear-copy queue reuse and failure lifetime. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "windows_queue_test.h"
#include "linear_copy_fixture.h"
#include "pvr_rogue_fwif.h"
#include "pvr_rogue_fwif_common.h"
static unsigned submits, releases, fail_after, wraps, invalidations;
static uint8_t *test_dma;
static void *allocate(PDMA_ADAPTER adapter,ULONG size,PHYSICAL_ADDRESS *logical,BOOLEAN cache)
{
    (void)adapter; (void)cache; logical->QuadPart=0x90000000; test_dma=aligned_alloc(4096,size); return test_dma;
}
static void release(PDMA_ADAPTER adapter,ULONG size,PHYSICAL_ADDRESS logical,void *address,BOOLEAN cache)
{ (void)adapter; (void)size; (void)logical; (void)cache; ++releases; free(address); test_dma=NULL; }
unsigned KeGetCurrentIrql(void) { return PASSIVE_LEVEL; }
NTSTATUS KeDelayExecutionThread(int mode,BOOLEAN alertable,const LARGE_INTEGER *delay)
{ (void)mode; (void)alertable; (void)delay; return STATUS_SUCCESS; }
NTSTATUS a04_gpu_dma_sync(void *address,ULONG length,BOOLEAN device_writes)
{ (void)device_writes; return address && length ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER; }
NTSTATUS a04_gpu_firmware_invalidate_mmu(struct a04_gpu_firmware_state *state)
{ if (!state) return STATUS_INVALID_PARAMETER; ++invalidations; return STATUS_SUCCESS; }
NTSTATUS a04_gpu_firmware_submit_kick(struct a04_gpu_firmware_state *s,const struct rogue_fwif_kccb_cmd *kick)
{
    uint8_t *heap=s->memory.heap,*ring; struct rogue_fwif_fwcommoncontext *ctx;
    struct rogue_fwif_cccb_ctl *ctl; struct rogue_fwif_ccb_cmd_header *hdr;
    struct rogue_fwif_ufo *ufo; uint32_t offset,woff;
    if(!test_dma || kick->cmd_type!=ROGUE_FWIF_KCCB_CMD_KICK) return STATUS_INVALID_PARAMETER;
    ctx=(void *)(heap+(kick->cmd_data.cmd_kick_data.context_fw_addr&0xffffffu));
    ring=heap+(ctx->ccb_fw_addr&0xffffffu); ctl=(void *)(heap+(ctx->ccbctl_fw_addr&0xffffffu));
    offset=ctl->read_offset; woff=kick->cmd_data.cmd_kick_data.client_woff_update;
    ++submits; if(fail_after && submits>=fail_after) return STATUS_SUCCESS;
    hdr=(void *)(ring+offset);
    if(hdr->cmd_type==ROGUE_FWIF_CCB_CMD_TYPE_PADDING) { ++wraps; offset=0; hdr=(void *)ring; }
    if(hdr->cmd_type!=ROGUE_FWIF_CCB_CMD_TYPE_TQ_3D || hdr->cmd_size!=sizeof(struct rogue_fwif_cmd_transfer)) return STATUS_DATA_ERROR;
    offset+=(uint32_t)(sizeof(*hdr)+hdr->cmd_size); hdr=(void *)(ring+offset);
    if(hdr->cmd_type==ROGUE_FWIF_CCB_CMD_TYPE_PADDING) { ++wraps; offset=0; hdr=(void *)ring; }
    if(hdr->cmd_type!=ROGUE_FWIF_CCB_CMD_TYPE_UPDATE) return STATUS_DATA_ERROR;
    ufo=(void *)((uint8_t *)hdr+sizeof(*hdr));
    if(ufo->value!=submits || offset+sizeof(*hdr)+sizeof(*ufo)!=woff) return STATUS_DATA_ERROR;
    {
        uint64_t *pt=(void *)(test_dma+7u*4096u);
        size_t src=(size_t)((UINT64_C(0x80000b7000)>>12)&511u);
        size_t dst=(size_t)((UINT64_C(0x80000bd000)>>12)&511u);
        uint64_t src_dma=pt[src]&~UINT64_C(0xfff),dst_dma=pt[dst]&~UINT64_C(0xfff);
        const uint64_t limit=0x90000000u+(14u+A04_GPU_LINEAR_COPY_PAGES+12u)*4096u-4u*4096u;
        if(src_dma<0x90000000u || dst_dma<0x90000000u || src_dma>limit || dst_dma>limit) return STATUS_DATA_ERROR;
        memcpy(test_dma+(size_t)(dst_dma-0x90000000u),
               test_dma+(size_t)(src_dma-0x90000000u),4u*4096u);
    }
    *(uint32_t *)(void *)(heap+(ufo->addr&0xffffffu))=ufo->value; ctl->read_offset=woff;
    return STATUS_SUCCESS;
}
int main(void)
{
    DMA_OPERATIONS ops={allocate,release}; struct fake_adapter adapter={&ops};
    struct a04_gpu_firmware_state s={0}; struct a04_gpu_queue_test_memory m={0};
    void *heap=aligned_alloc(4096,A04_PVR_HEAP_SIZE); NTSTATUS status; uint32_t first_end;
    if(!heap) return 1;
    s.processor_released=TRUE; s.firmware_started=TRUE; s.memory.adapter=&adapter;
    s.memory.heap=heap; s.memory.heap_logical.QuadPart=0x80000000; s.runtime.next_free=0x20000;
    status=a04_gpu_queue_test(&s,&m);
    if(status!=STATUS_SUCCESS || submits!=147 || !wraps || invalidations!=9 || !m.cpu || releases) {
        fprintf(stderr,"linear status=%08x submits=%u wraps=%u invalidations=%u memory=%d releases=%u\n",
                (unsigned)status,submits,wraps,invalidations,m.cpu!=NULL,releases); return 2;
    }
    first_end=s.runtime.next_free;
    if(first_end<=0x20000u) return 7;
    if(a04_gpu_queue_test_free(&s,&m)!=STATUS_DEVICE_BUSY) return 3;
    s.processor_released=FALSE;
    if(a04_gpu_queue_test_free(&s,&m)!=STATUS_SUCCESS || releases!=1) return 4;
    s.processor_released=TRUE; submits=0; fail_after=5;
    status=a04_gpu_queue_test(&s,&m);
    if(status!=STATUS_IO_TIMEOUT || !s.queue_faulted || !m.cpu || releases!=1 || s.runtime.next_free<=first_end ||
       a04_gpu_queue_test_free(&s,&m)!=STATUS_DEVICE_BUSY) return 5;
    s.processor_released=FALSE;
    if(a04_gpu_queue_test_free(&s,&m)!=STATUS_SUCCESS || releases!=2) return 6;
    s.processor_released=TRUE; s.queue_faulted=FALSE; submits=0; fail_after=147;
    status=a04_gpu_queue_test(&s,&m);
    if(status!=STATUS_IO_TIMEOUT || !s.queue_faulted || !m.cpu || releases!=2 ||
       a04_gpu_queue_test_free(&s,&m)!=STATUS_DEVICE_BUSY) return 8;
    s.processor_released=FALSE;
    if(a04_gpu_queue_test_free(&s,&m)!=STATUS_SUCCESS || releases!=3) return 9;
    free(heap); puts("GE8300 repeated linear transfers, remaps, wrap and timeout lifetime simulated"); return 0;
}
