/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_irq.h"
#if defined(A04_WIFI_IRQ_DIAGNOSTIC) && A04_WIFI_IRQ_DIAGNOSTIC
#define MSDC_INT 0x0cu
#define MSDC_INTEN 0x10u
#define MSDC_SDIOIRQ 0x80u
static volatile ULONG *reg(struct a04_windows_irq *irq,unsigned off)
{return (volatile ULONG *)(irq->base+off);}
static void irq_dpc(PKDPC dpc,PVOID context,PVOID a,PVOID b)
{
 struct a04_windows_irq *irq=context;
 UNREFERENCED_PARAMETER(dpc);UNREFERENCED_PARAMETER(a);UNREFERENCED_PARAMETER(b);
 if(InterlockedCompareExchange(&irq->active,0,0)&&irq->notify)irq->notify(irq->context);
}
static BOOLEAN irq_isr(PKINTERRUPT object,PVOID context)
{
 struct a04_windows_irq *irq=context;ULONG status,enabled;
 UNREFERENCED_PARAMETER(object);
 if(!InterlockedCompareExchange(&irq->active,0,0)||!InterlockedCompareExchange(&irq->armed,0,0))return FALSE;
 status=READ_REGISTER_ULONG(reg(irq,MSDC_INT));enabled=READ_REGISTER_ULONG(reg(irq,MSDC_INTEN));
 if(!(status&enabled&MSDC_SDIOIRQ))return FALSE;
 /* Mask before acknowledging; only SDIOIRQ is W1C here, never PIO bits. */
 WRITE_REGISTER_ULONG(reg(irq,MSDC_INTEN),enabled&~MSDC_SDIOIRQ);
 InterlockedExchange(&irq->armed,0);
 WRITE_REGISTER_ULONG(reg(irq,MSDC_INT),MSDC_SDIOIRQ);
 (void)KeInsertQueueDpc(&irq->dpc,NULL,NULL);return TRUE;
}
static BOOLEAN arm_locked(PVOID context)
{
 struct a04_windows_irq *irq=context;ULONG enabled;
 if(!InterlockedCompareExchange(&irq->active,0,0))return FALSE;
 enabled=READ_REGISTER_ULONG(reg(irq,MSDC_INTEN));
 WRITE_REGISTER_ULONG(reg(irq,MSDC_INTEN),enabled|MSDC_SDIOIRQ);
 if(!(READ_REGISTER_ULONG(reg(irq,MSDC_INTEN))&MSDC_SDIOIRQ)) {
  WRITE_REGISTER_ULONG(reg(irq,MSDC_INTEN),enabled&~MSDC_SDIOIRQ);return FALSE;
 }
 InterlockedExchange(&irq->armed,1);return TRUE;
}
static BOOLEAN stop_locked(PVOID context)
{
 struct a04_windows_irq *irq=context;ULONG enabled;
 InterlockedExchange(&irq->active,0);InterlockedExchange(&irq->armed,0);
 enabled=READ_REGISTER_ULONG(reg(irq,MSDC_INTEN));
 WRITE_REGISTER_ULONG(reg(irq,MSDC_INTEN),enabled&~MSDC_SDIOIRQ);
 return (READ_REGISTER_ULONG(reg(irq,MSDC_INTEN))&MSDC_SDIOIRQ)?FALSE:TRUE;
}
NTSTATUS a04_windows_irq_connect(struct a04_windows_irq *irq,volatile UCHAR *base,
 const CM_PARTIAL_RESOURCE_DESCRIPTOR *raw,const CM_PARTIAL_RESOURCE_DESCRIPTOR *translated,
 a04_wifi_irq_notify notify,void *context)
{
 ULONG enabled;NTSTATUS status;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!irq||irq->interrupt||!base||!raw||!translated||!notify||
    raw->Type!=CmResourceTypeInterrupt||translated->Type!=CmResourceTypeInterrupt||
    raw->ShareDisposition!=CmResourceShareDeviceExclusive||translated->ShareDisposition!=CmResourceShareDeviceExclusive||
    raw->u.Interrupt.Vector!=141||!translated->u.Interrupt.Affinity||
    (raw->Flags&CM_RESOURCE_INTERRUPT_LATCHED)||(translated->Flags&CM_RESOURCE_INTERRUPT_LATCHED))return STATUS_DEVICE_CONFIGURATION_ERROR;
 irq->base=base;irq->notify=notify;irq->context=context;irq->active=0;irq->armed=0;
 /* No ISR is connected yet; caller has exclusive host ownership and keeps
  * all other access serialized.  Readback failure quarantines the mapping. */
 enabled=READ_REGISTER_ULONG(reg(irq,MSDC_INTEN));
 WRITE_REGISTER_ULONG(reg(irq,MSDC_INTEN),enabled&~MSDC_SDIOIRQ);
 if(READ_REGISTER_ULONG(reg(irq,MSDC_INTEN))&MSDC_SDIOIRQ)return STATUS_DEVICE_NOT_READY;
 KeInitializeDpc(&irq->dpc,irq_dpc,irq);InterlockedExchange(&irq->active,1);
 status=IoConnectInterrupt(&irq->interrupt,irq_isr,irq,NULL,translated->u.Interrupt.Vector,
  (KIRQL)translated->u.Interrupt.Level,(KIRQL)translated->u.Interrupt.Level,LevelSensitive,FALSE,
  translated->u.Interrupt.Affinity,FALSE);
 if(!NT_SUCCESS(status)){InterlockedExchange(&irq->active,0);irq->interrupt=NULL;}
 return status;
}
int a04_windows_irq_arm(struct a04_windows_irq *irq)
{if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!irq||!irq->interrupt||InterlockedCompareExchange(&irq->armed,0,0))return -1;return KeSynchronizeExecution(irq->interrupt,arm_locked,irq)?0:-1;}
int a04_windows_irq_stop(struct a04_windows_irq *irq)
{
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!irq||!irq->interrupt)return -1;
 if(!KeSynchronizeExecution(irq->interrupt,stop_locked,irq))return -1;
 IoDisconnectInterrupt(irq->interrupt);irq->interrupt=NULL;
 (void)KeRemoveQueueDpc(&irq->dpc);KeFlushQueuedDpcs();
 irq->base=NULL;irq->notify=NULL;irq->context=NULL;return 0;
}
#endif
