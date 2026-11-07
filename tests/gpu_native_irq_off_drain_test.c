/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Explicit W1C/FW RAM model. No physical IRQ delivery or DMA-stop proof. */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "native_owner_irq.h"
#include "A04BR3RogueRuntimeLayout.h"
static unsigned irql,inject_late_count;static ULONG *base;static RGXFWIF_TRACEBUF *late_trace;
void a04_gpu_dma_barrier(void){__sync_synchronize();} /* host fence double */
unsigned KeGetCurrentIrql(void){return irql;}
ULONG a04_test_read_register(volatile ULONG *p){return *p;}
void a04_test_write_register(volatile ULONG *p,ULONG v)
{*p=v;if(p==base+0x8b0/4 && v==1)base[0x8a8/4]=0;
 if(p==base+0x8a0/4 && !v && inject_late_count){late_trace->aui32InterruptCount[0]++;inject_late_count=0;}}
int main(void)
{
 _Alignas(8) ULONG regs[0x900/4]={0};struct a04_native_irq_core q={0};
 struct a04_rgx_fw_runtime r={0};RGXFWIF_TRACEBUF trace={0};
 RGXFWIF_CCB_CTL f={0,0,31,sizeof(RGXFWIF_FWCCB_CMD)},k={0,0,127,sizeof(RGXFWIF_KCCB_CMD)};
 struct a04_native_irq_receipt irq;struct a04_native_irq_off_observation out;
 base=regs;late_trace=&trace;r.trace=&trace;r.fctl=&f;r.kctl=&k;trace.ePowState=RGXFWIF_POW_ON;
 assert(a04_native_irq_bind(&q,(UCHAR *)regs,sizeof(regs),&r,10)==STATUS_SUCCESS);
 assert(a04_native_irq_begin_off_drain(&q,10)==STATUS_INVALID_DEVICE_STATE);
 irql=5;assert(a04_native_irq_begin_off_drain(&q,9)==STATUS_INVALID_DEVICE_STATE);
 regs[0x8a8/4]=1;
 assert(a04_native_irq_begin_off_drain(&q,10)==STATUS_DEVICE_BUSY && regs[0x8a8/4]==1 && q.draining && !q.masked && !q.sampled_fw_valid);
 assert(a04_native_irq_claim(&q,&irq) && !irq.fault && q.sampled_fw_valid);
 q.sampled_fw_valid=FALSE; /* isolate subsequent no-sample rejection */
 assert(a04_native_irq_begin_off_drain(&q,10)==STATUS_SUCCESS && q.draining && !q.masked);
 trace.ePowState=RGXFWIF_POW_OFF;trace.aui32InterruptCount[0]=3;
 assert(a04_native_irq_close_off(&q,10,&out)==STATUS_INVALID_DEVICE_STATE && !out.source_closed); /* no ISR sample */
 regs[0x8a8/4]=1;
 assert(a04_native_irq_claim(&q,&irq) && !irq.fault && q.sampled_fw_valid && q.sampled_fw_count==3 && !q.masked);
 trace.aui32InterruptCount[0]=4;
 assert(a04_native_irq_close_off(&q,10,&out)==STATUS_DEVICE_BUSY && q.powered && !out.source_closed);
 regs[0x8a8/4]=1;assert(a04_native_irq_claim(&q,&irq) && q.sampled_fw_count==4 && !q.masked);
 f.ui32WriteOffset=1;assert(a04_native_irq_close_off(&q,10,&out)==STATUS_DEVICE_BUSY);
 f.ui32ReadOffset=1;k.ui32WriteOffset=1;assert(a04_native_irq_close_off(&q,10,&out)==STATUS_DEVICE_BUSY);
 k.ui32ReadOffset=1;trace.ePowState=RGXFWIF_POW_ON;
 assert(a04_native_irq_close_off(&q,10,&out)==STATUS_DEVICE_BUSY);
 trace.ePowState=RGXFWIF_POW_OFF;
 assert(a04_native_irq_close_off(&q,10,&out)==STATUS_SUCCESS && out.source_closed && out.epoch==10 && out.fw_count==4 && out.serial==3);
 assert(!q.powered && q.closed && q.masked && !q.draining && !regs[0x8a0/4]);
 regs[0x8a8/4]=1;assert(!a04_native_irq_claim(&q,&irq));
 irql=0;assert(a04_native_irq_unbind_disconnected(&q)==STATUS_SUCCESS);
 assert(a04_native_irq_bind(&q,(UCHAR *)regs,sizeof(regs),&r,11)==STATUS_SUCCESS && !q.sampled_fw_valid);
 irql=5;regs[0x8a8/4]=0;
 assert(a04_native_irq_begin_off_drain(&q,11)==STATUS_SUCCESS);
 f.ui32WriteOffset=2;regs[0x8a8/4]=1;
 assert(a04_native_irq_claim(&q,&irq) && irq.fault==STATUS_DEVICE_BUSY && q.faulted && q.masked);
 assert(a04_native_irq_close_off(&q,11,&out)==STATUS_INVALID_DEVICE_STATE && !out.source_closed);
 assert(a04_native_irq_mask_close(&q,11)==STATUS_SUCCESS);irql=0;
 assert(a04_native_irq_unbind_disconnected(&q)==STATUS_SUCCESS);
 f.ui32ReadOffset=f.ui32WriteOffset;regs[0x8a8/4]=0;
 assert(a04_native_irq_bind(&q,(UCHAR *)regs,sizeof(regs),&r,12)==STATUS_SUCCESS);irql=5;
 assert(a04_native_irq_begin_off_drain(&q,12)==STATUS_SUCCESS);
 regs[0x8a8/4]=1;assert(a04_native_irq_claim(&q,&irq) && !irq.fault);
 inject_late_count=1;
 assert(a04_native_irq_close_off(&q,12,&out)==STATUS_IO_DEVICE_ERROR && !out.source_closed && q.closed && q.faulted);
 puts("source-linked native IRQ off drain: actual-source counter sample, OFF+stable FW/host equality, empty queues, mask/close, stale epoch/fault retention MODEL PASS; not DMA/OS rundown proof");
}
