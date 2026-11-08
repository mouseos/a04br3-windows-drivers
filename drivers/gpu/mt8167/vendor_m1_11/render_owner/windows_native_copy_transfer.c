/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Same DM3 TQ/UFO/SLC/GP protocol as validated windows_render_clear.c.
 * Native owner transport only: no private RT, CPU copy, or fake completion. */
#include "windows_native_copy_transfer.h"
#include "windows_native_cold_private.h"
#include "windows_start.h"
#include "native_owner_irq.h"
#include "native_copy_queue.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
#include "../../windows_hw.h"
#define TAG 0x71544741u
struct a04_native_tq_transfer {
 struct a04_native_copy_storage *storage;
 struct a04_vendor_native_cold *native;
 struct a04_native_copy_queue_input input;
 struct a04_native_copy_queue_result queue;
 volatile LONG state;ULONG kernel_next;uint64_t epoch,irq_serial,claimed_serial;
 struct a04_native_irq_core *irq;
 ULONG visibility_fence;uint64_t visibility_serial;
 _Alignas(8) UCHAR ring[4096],workspace[4096];
};
static int queues(struct a04_rgx_fw_runtime *f,RGXFWIF_CCB_CTL *k)
{
 RGXFWIF_CCB_CTL q;a04_gpu_dma_barrier();
 return !f||!f->kctl||!f->fctl||!f->kcommands||!f->command_sync||
  a04_rgx_device_read_words(k,f->kctl,sizeof(*k))||
  a04_rgx_device_read_words(&q,f->fctl,sizeof(q))||k->ui32WrapMask!=127u||
  k->ui32CmdSize!=sizeof(RGXFWIF_KCCB_CMD)||k->ui32ReadOffset>127u||k->ui32WriteOffset>127u||
  q.ui32WrapMask!=31u||q.ui32CmdSize!=sizeof(RGXFWIF_FWCCB_CMD)||
  q.ui32ReadOffset>31u||q.ui32WriteOffset!=q.ui32ReadOffset;
}
static NTSTATUS poison(struct a04_native_tq_transfer *c,struct a04_vendor_start *s,NTSTATUS status)
{
 InterlockedExchange(&c->state,5);c->native->quarantined=TRUE;c->native->render->poisoned=TRUE;
 s->command_poisoned=TRUE;return NT_SUCCESS(status)?STATUS_IO_DEVICE_ERROR:status;
}
NTSTATUS a04_native_tq_transfer_prepare(struct a04_native_tq_transfer **out,
 struct a04_native_copy_storage *storage)
{
 struct a04_vendor_native_cold *n=a04_native_copy_storage_parent(storage);
 struct a04_vendor_render_cold *r;struct a04_native_tq_transfer *c;
 const struct a04_native_copy_job_image *image=a04_native_copy_storage_image(storage);
 RGXFWIF_CCCB_CTL ctl;ULONG sync;
 if(!out||*out||!n||!image||KeGetCurrentIrql()!=PASSIVE_LEVEL||!(r=n->render)||
    r->native_owner!=n||r->poisoned||n->quarantined||!r->firmware||
    r->firmware->hardware_owned||r->sequence==UINT32_MAX||!r->buffer[0].cpu||
    r->layout.control[3]>65536u-sizeof(ctl)||r->layout.commands[3]>65536u-4096u||
    r->layout.sync>65536u-4u)return STATUS_INVALID_PARAMETER;
 if(a04_rgx_device_read_words(&ctl,(UCHAR*)r->buffer[0].cpu+r->layout.control[3],sizeof(ctl))||
    ctl.ui32WrapMask!=4095u||ctl.ui32ReadOffset!=r->client_write[3]||
    ctl.ui32WriteOffset!=r->client_write[3]||
    a04_rgx_device_read_words(&sync,(UCHAR*)r->buffer[0].cpu+r->layout.sync,4u)||sync!=r->sequence)
  return STATUS_DEVICE_BUSY;
 c=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*c),TAG);if(!c)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(c,sizeof(*c));c->storage=storage;c->native=n;
 c->input=(struct a04_native_copy_queue_input){0xc0080000u+r->layout.tq,
  0xc0080000u+r->layout.sync,r->sequence,ctl.ui32ReadOffset,ctl.ui32WriteOffset,ctl.ui32DepOffset,0,1};
 if(a04_rgx_device_read_words(c->ring,(UCHAR*)r->buffer[0].cpu+r->layout.commands[3],4096u)||
    a04_native_copy_queue_build(c->ring,c->workspace,&c->input,image,&c->queue)){
  ExFreePoolWithTag(c,TAG);return STATUS_INVALID_DEVICE_STATE;
 }
 c->state=1;*out=c;return STATUS_SUCCESS;
}
NTSTATUS a04_native_tq_transfer_submit(struct a04_native_tq_transfer *c,
 struct a04_vendor_start *s,struct a04_native_irq_core *irq,ULONG expected)
{
 struct a04_vendor_render_cold *r;struct a04_rgx_fw_runtime *f;
 RGXFWIF_CCB_CTL k;RGXFWIF_CCCB_CTL ctl;ULONG sync;
 if(!c||!s||!irq||KeGetCurrentIrql()<=DISPATCH_LEVEL||c->state!=1||expected>127u||
    !(r=c->native->render)||r->native_owner!=c->native||r->poisoned||c->native->quarantined||
    !s->online||!s->core||s->command_poisoned||s->shutdown_started||
    !s->memory||s->memory!=r->firmware||!s->memory->hardware_owned||!s->memory->uncached||
    !(f=s->memory->runtime)||irq->core!=s->core||irq->runtime!=f||!irq->epoch||
    !irq->powered||irq->masked||irq->closed||irq->faulted||irq->draining)return STATUS_INVALID_DEVICE_STATE;
 if(!a04_native_copy_storage_gpu_owned(c->storage))return STATUS_INVALID_DEVICE_STATE;
 if(InterlockedCompareExchange(&s->command_busy,1,0))return STATUS_DEVICE_BUSY;
 if(queues(f,&k)||k.ui32ReadOffset!=expected||k.ui32WriteOffset!=expected||
    r->sequence!=c->input.previous_value||r->client_write[3]!=c->input.write||
    a04_rgx_device_read_words(&ctl,(UCHAR*)r->buffer[0].cpu+r->layout.control[3],sizeof(ctl))||
    ctl.ui32ReadOffset!=c->input.read||ctl.ui32WriteOffset!=c->input.write||
    ctl.ui32DepOffset!=c->input.dependency||ctl.ui32WrapMask!=4095u||
    a04_rgx_device_read_words(&sync,(UCHAR*)r->buffer[0].cpu+r->layout.sync,4u)||sync!=c->input.previous_value){
  InterlockedExchange(&s->command_busy,0);return STATUS_DEVICE_BUSY;
 }
 /* From here ambiguity retains ownership, including command_busy. No wait,
  * allocation, CPU buffer access or fabricated completion at DIRQL. */
 c->epoch=irq->epoch;c->irq_serial=irq->serial;c->irq=irq;c->kernel_next=(expected+1u)&127u;
 InterlockedExchange(&c->state,2);
 if(c->queue.padding_bytes&&a04_rgx_device_write_words(
    (UCHAR*)r->buffer[0].cpu+r->layout.commands[3]+c->input.write,
    c->ring+c->input.write,sizeof(RGXFWIF_CCB_CMD_HEADER)))return poison(c,s,STATUS_IO_DEVICE_ERROR);
 if(a04_rgx_device_write_words((UCHAR*)r->buffer[0].cpu+r->layout.commands[3]+c->queue.task_offset,
    c->ring+c->queue.task_offset,296u)||
    a04_rgx_device_write_words(&f->kcommands[expected],&c->queue.kick,sizeof(c->queue.kick)))
  return poison(c,s,STATUS_IO_DEVICE_ERROR);
 r->sequence=c->queue.completion_value;r->client_write[3]=c->queue.write_offset;
 a04_gpu_dma_barrier();((volatile RGXFWIF_CCB_CTL*)f->kctl)->ui32WriteOffset=c->kernel_next;
 a04_gpu_dma_barrier();WRITE_REGISTER_ULONG((volatile ULONG*)(void*)(s->core+0xb00),0x10);
 return STATUS_SUCCESS;
}
static NTSTATUS wait_queue(struct a04_rgx_fw_runtime *f,ULONG next)
{
 ULONGLONG began=KeQueryInterruptTime();LARGE_INTEGER delay;RGXFWIF_CCB_CTL k;
 delay.QuadPart=-10000;
 for(;;){
  if(queues(f,&k))return STATUS_IO_DEVICE_ERROR;
  if(k.ui32ReadOffset==next)return STATUS_SUCCESS;
  if(KeQueryInterruptTime()-began>=20000000u)return STATUS_IO_TIMEOUT;
  (void)KeDelayExecutionThread(KernelMode,FALSE,&delay);
 }
}
static NTSTATUS issue(struct a04_vendor_start *s,struct a04_rgx_fw_runtime *f,RGXFWIF_KCCB_CMD *cmd)
{
 RGXFWIF_CCB_CTL k;ULONG next;
 if(queues(f,&k)||k.ui32ReadOffset!=k.ui32WriteOffset)return STATUS_DEVICE_BUSY;
 if(a04_rgx_device_write_words(&f->kcommands[k.ui32WriteOffset],cmd,sizeof(*cmd)))return STATUS_IO_DEVICE_ERROR;
 next=(k.ui32WriteOffset+1u)&127u;
 a04_gpu_dma_barrier();((volatile RGXFWIF_CCB_CTL*)f->kctl)->ui32WriteOffset=next;
 a04_gpu_dma_barrier();WRITE_REGISTER_ULONG((volatile ULONG*)(void*)(s->core+0xb00),0x10);
 return wait_queue(f,next);
}
NTSTATUS a04_native_tq_transfer_begin_finish(struct a04_native_tq_transfer *c,
 struct a04_native_irq_core *q,const struct a04_native_irq_receipt *receipt)
{
 if(!c||!q||!receipt||KeGetCurrentIrql()<=DISPATCH_LEVEL||c->irq!=q||
    InterlockedCompareExchange(&c->state,0,0)!=2||!q->powered||!q->masked||q->closed||
    q->faulted||q->draining||!NT_SUCCESS(receipt->fault)||receipt->epoch!=c->epoch||
    receipt->epoch!=q->epoch||receipt->serial<=c->irq_serial||receipt->serial!=q->serial)
  return STATUS_INVALID_DEVICE_STATE;
 c->claimed_serial=receipt->serial;InterlockedExchange(&c->state,3);return STATUS_SUCCESS;
}
NTSTATUS a04_native_tq_transfer_finish_gpu(struct a04_native_tq_transfer *c,
 struct a04_vendor_start *s,struct a04_native_copy_gpu_result *out)
{
 struct a04_vendor_render_cold *r;struct a04_rgx_fw_runtime *f;RGXFWIF_CCCB_CTL ctl;
 RGXFWIF_KCCB_CMD cmd={0};ULONG value;NTSTATUS status;ULONGLONG began;LARGE_INTEGER delay;
 if(!out)return STATUS_INVALID_PARAMETER;RtlZeroMemory(out,sizeof(*out));
 if(!c||!s||KeGetCurrentIrql()!=PASSIVE_LEVEL||InterlockedCompareExchange(&c->state,0,0)!=3||!(r=c->native->render)||
    !s->memory||s->memory!=r->firmware||!(f=s->memory->runtime)||!s->memory->hardware_owned||
    !s->online||s->command_busy!=1||s->command_poisoned||s->shutdown_started||
    c->claimed_serial<=c->irq_serial)return STATUS_INVALID_DEVICE_STATE;
 /* ISR receipt was captured under the actual IRQ lock. Parent rundown and
  * exclusive service own this immutable job/epoch through the PASSIVE work. */
 out->phase=1;status=wait_queue(f,c->kernel_next);if(!NT_SUCCESS(status))goto fail;
 began=KeQueryInterruptTime();delay.QuadPart=-10000;
 for(;;){
  if(a04_rgx_device_read_words(&ctl,(UCHAR*)r->buffer[0].cpu+r->layout.control[3],sizeof(ctl))||
     a04_rgx_device_read_words(&value,(UCHAR*)r->buffer[0].cpu+r->layout.sync,4u)){status=STATUS_IO_DEVICE_ERROR;goto fail;}
  if(ctl.ui32WrapMask!=4095u||ctl.ui32DepOffset>4095u||(ctl.ui32DepOffset&7u)){status=STATUS_IO_DEVICE_ERROR;goto fail;}
  if(ctl.ui32ReadOffset==c->queue.write_offset&&ctl.ui32WriteOffset==c->queue.write_offset&&
     value==c->queue.completion_value)break;
  if(KeQueryInterruptTime()-began>=20000000u){status=STATUS_IO_TIMEOUT;goto fail;}
  (void)KeDelayExecutionThread(KernelMode,FALSE,&delay);
 }
 out->phase=2;out->task_sequence=value;out->client_write=c->queue.write_offset;
 cmd.eCmdType=RGXFWIF_KCCB_CMD_SLCFLUSHINVAL;cmd.eDM=0;cmd.uCmdData.sSLCFlushInvalData.bInval=TRUE;
 status=issue(s,f,&cmd);if(!NT_SUCCESS(status))goto fail;
 if(s->command_sequence>=0xfffffffeu){status=STATUS_INTEGER_OVERFLOW;goto fail;}
 RtlZeroMemory(&cmd,sizeof(cmd));cmd.eCmdType=RGXFWIF_KCCB_CMD_SYNC;cmd.eDM=0;
 cmd.uCmdData.sSyncData.sSyncObjDevVAddr=f->command_sync_addr;
 cmd.uCmdData.sSyncData.uiUpdateVal=++s->command_sequence;
 *(volatile ULONG*)f->command_sync=0;a04_gpu_dma_barrier();
 status=issue(s,f,&cmd);if(!NT_SUCCESS(status))goto fail;
 began=KeQueryInterruptTime();
 while(*(volatile ULONG*)f->command_sync!=s->command_sequence){
  a04_gpu_dma_barrier();if(KeQueryInterruptTime()-began>=20000000u){status=STATUS_IO_TIMEOUT;goto fail;}
  (void)KeDelayExecutionThread(KernelMode,FALSE,&delay);
 }
 a04_gpu_dma_barrier();out->phase=3;out->flush_sequence=s->command_sequence;
 status=a04_native_copy_storage_cpu_complete(c->storage);if(!NT_SUCCESS(status))goto fail;
 out->phase=4;InterlockedExchange(&c->state,4);
 return STATUS_SUCCESS; /* CPU ownership established; scheduler still waits for fresh real IRQ. */
fail:return poison(c,s,status);
}
NTSTATUS a04_native_tq_transfer_release_unsubmitted(struct a04_native_tq_transfer **out)
{
 struct a04_native_tq_transfer *c;
 if(!out||!(c=*out)||KeGetCurrentIrql()!=PASSIVE_LEVEL||c->state!=1||
    !c->native->render->firmware||c->native->render->firmware->hardware_owned)return STATUS_DEVICE_BUSY;
 ExFreePoolWithTag(c,TAG);*out=NULL;return STATUS_SUCCESS;
}

LONG a04_native_tq_transfer_state(struct a04_native_tq_transfer *c){return c?InterlockedCompareExchange(&c->state,0,0):0;}
NTSTATUS a04_native_tq_transfer_arm_visibility(struct a04_native_tq_transfer *c,struct a04_vendor_start *s,struct a04_native_irq_core *q) {
 RGXFWIF_CCB_CTL k;RGXFWIF_KCCB_CMD cmd={0};struct a04_rgx_fw_runtime *f;ULONG next;NTSTATUS status;
 if(!c||!s||!q||KeGetCurrentIrql()<=DISPATCH_LEVEL||c->state!=4||c->irq!=q||q->epoch!=c->epoch||
    !q->masked||q->faulted||q->closed||q->draining||!q->powered||s->command_poisoned||!s->online||s->command_sequence>=0xfffffffeu)return STATUS_INVALID_DEVICE_STATE;
 f=s->memory->runtime;if(s->command_busy!=1)return STATUS_INVALID_DEVICE_STATE;
 if(queues(f,&k)||k.ui32ReadOffset!=k.ui32WriteOffset||*(volatile ULONG*)f->command_sync!=s->command_sequence){return STATUS_DEVICE_BUSY;}
 status=a04_native_irq_rearm(q,c->epoch,q->serial);if(!NT_SUCCESS(status))return status;
 c->visibility_serial=q->serial;c->visibility_fence=++s->command_sequence;
 cmd.eCmdType=RGXFWIF_KCCB_CMD_SYNC;cmd.eDM=0;cmd.uCmdData.sSyncData.sSyncObjDevVAddr=f->command_sync_addr;cmd.uCmdData.sSyncData.uiUpdateVal=c->visibility_fence;
 *(volatile ULONG*)f->command_sync=0;InterlockedExchange(&c->state,6);
 if(a04_rgx_device_write_words(&f->kcommands[k.ui32WriteOffset],&cmd,sizeof(cmd)))return poison(c,s,STATUS_IO_DEVICE_ERROR);
 next=(k.ui32WriteOffset+1)&127;a04_gpu_dma_barrier();((volatile RGXFWIF_CCB_CTL*)f->kctl)->ui32WriteOffset=next;
 a04_gpu_dma_barrier();WRITE_REGISTER_ULONG((volatile ULONG*)(void*)(s->core+0xb00),0x10);return STATUS_SUCCESS;
}
NTSTATUS a04_native_tq_transfer_visibility_receipt(struct a04_native_tq_transfer *c,struct a04_vendor_start *s,struct a04_native_irq_core *q,const struct a04_native_irq_receipt *receipt) {
 RGXFWIF_CCB_CTL k;struct a04_rgx_fw_runtime *f;
 if(!c||!s||!q||!receipt||KeGetCurrentIrql()<=DISPATCH_LEVEL||c->state!=6||c->irq!=q||
    q->epoch!=c->epoch||receipt->epoch!=c->epoch||receipt->serial<=c->visibility_serial||receipt->serial!=q->serial||
    !q->masked||q->faulted||q->closed||q->draining||!q->powered||!NT_SUCCESS(receipt->fault)||s->command_busy!=1||s->command_poisoned||!s->online)return STATUS_INVALID_DEVICE_STATE;
 f=s->memory->runtime;a04_gpu_dma_barrier();if(*(volatile ULONG*)f->command_sync!=c->visibility_fence||queues(f,&k)||k.ui32ReadOffset!=k.ui32WriteOffset){InterlockedExchange(&c->state,8);return STATUS_PENDING;}
 NTSTATUS status=a04_native_irq_rearm(q,c->epoch,q->serial);if(!NT_SUCCESS(status))return status;
 InterlockedExchange(&c->state,7);InterlockedExchange(&s->command_busy,0);return STATUS_SUCCESS;
}
NTSTATUS a04_native_tq_transfer_release_retired(struct a04_native_tq_transfer **out) {
 struct a04_native_tq_transfer *c;if(!out||!(c=*out)||KeGetCurrentIrql()!=PASSIVE_LEVEL||
   !c->native->render->firmware||c->native->render->firmware->hardware_owned||
   (c->irq&&(c->irq->core||c->irq->powered)))return STATUS_DEVICE_BUSY;
 ExFreePoolWithTag(c,TAG);*out=NULL;return STATUS_SUCCESS;
}

NTSTATUS a04_native_tq_transfer_wait_visibility_retry(struct a04_native_tq_transfer *c,struct a04_vendor_start *s) {
 ULONGLONG began;LARGE_INTEGER delay;RGXFWIF_CCB_CTL k;struct a04_rgx_fw_runtime *f;
 if(!c||!s||KeGetCurrentIrql()!=PASSIVE_LEVEL||a04_native_tq_transfer_state(c)!=8||!s->online||s->command_poisoned)return STATUS_INVALID_DEVICE_STATE;
 f=s->memory->runtime;began=KeQueryInterruptTime();delay.QuadPart=-10000;
 for(;;){
  a04_gpu_dma_barrier();if(queues(f,&k))return poison(c,s,STATUS_IO_DEVICE_ERROR);
  if(k.ui32ReadOffset==k.ui32WriteOffset&&*(volatile ULONG*)f->command_sync==c->visibility_fence){InterlockedExchange(&c->state,4);return STATUS_SUCCESS;}
  if(KeQueryInterruptTime()-began>=20000000u)return poison(c,s,STATUS_IO_TIMEOUT);
  (void)KeDelayExecutionThread(KernelMode,FALSE,&delay);
 }
 /* No scheduler completion here: caller must arm ANOTHER real GP IRQ. */
}

NTSTATUS a04_native_tq_transfer_drain_for_stop(struct a04_native_tq_transfer *c,struct a04_vendor_start *s) {
 NTSTATUS status;LONG phase;struct a04_native_copy_gpu_result result;
 if(!c||!s||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 phase=a04_native_tq_transfer_state(c);
 if(phase==1||phase==7)return STATUS_SUCCESS;
 /* A genuine ISR receipt may have arrived after admission closed. Completing
  * its bounded TQ/SLC/GP/cache work is stop progress, never a scheduler fence. */
 if(phase==3){status=a04_native_tq_transfer_finish_gpu(c,s,&result);if(!NT_SUCCESS(status))return status;phase=4;}
 if(phase==8){status=a04_native_tq_transfer_wait_visibility_retry(c,s);if(!NT_SUCCESS(status))return status;phase=4;}
 if(phase==6){
  ULONGLONG began=KeQueryInterruptTime();LARGE_INTEGER delay;RGXFWIF_CCB_CTL k;struct a04_rgx_fw_runtime *f=s->memory->runtime;delay.QuadPart=-10000;
  for(;;){a04_gpu_dma_barrier();if(queues(f,&k))return poison(c,s,STATUS_IO_DEVICE_ERROR);
   if(k.ui32ReadOffset==k.ui32WriteOffset&&*(volatile ULONG*)f->command_sync==c->visibility_fence)break;
   if(KeQueryInterruptTime()-began>=20000000u)return poison(c,s,STATUS_IO_TIMEOUT);
   (void)KeDelayExecutionThread(KernelMode,FALSE,&delay);
  }phase=4;
 }
 if(phase!=4)return STATUS_DEVICE_BUSY; /* submitted/no-IRQ or fault: retain */
 /* Parent still must perform cleanup/FWoff/reset/IRQdrain/VerifyOff. Only
  * release the command gate after real DMA/GP/cache work, not its resources. */
 InterlockedExchange(&s->command_busy,0);return STATUS_SUCCESS;
}
