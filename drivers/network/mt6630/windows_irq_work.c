/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_irq_work.h"
#if defined(A04_WIFI_IRQ_DIAGNOSTIC) && A04_WIFI_IRQ_DIAGNOSTIC
static void worker(PDEVICE_OBJECT device,PVOID context)
{
 struct a04_irq_work *w=context;UNREFERENCED_PARAMETER(device);
 if(!InterlockedCompareExchange(&w->closing,0,0) && w->process(w->context))InterlockedExchange(&w->fault,1);
 InterlockedExchange(&w->busy,2);KeSetEvent(&w->idle,IO_NO_INCREMENT,FALSE);
}
NTSTATUS a04_irq_work_init(struct a04_irq_work *w,PDEVICE_OBJECT device,int (*process)(void *),void *context)
{
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!w||!device||!process||w->work)return STATUS_INVALID_PARAMETER;
 w->work=IoAllocateWorkItem(device);if(!w->work)return STATUS_INSUFFICIENT_RESOURCES;
 w->busy=w->closing=w->fault=0;w->process=process;w->context=context;
 KeInitializeEvent(&w->idle,NotificationEvent,TRUE);return STATUS_SUCCESS;
}
void a04_irq_work_notify(void *context)
{
 struct a04_irq_work *w=context;
 /* IRQ is masked by ISR until caller explicitly rearms. Concurrent or late
  * notifications indicate a broken contract and must not be dropped as OK. */
 if(!w||!w->work||InterlockedCompareExchange(&w->closing,0,0))return;
 if(InterlockedCompareExchange(&w->busy,1,0)){InterlockedExchange(&w->fault,1);return;}
 KeClearEvent(&w->idle);IoQueueWorkItem(w->work,worker,DelayedWorkQueue,w);
}
int a04_irq_work_drain(struct a04_irq_work *w)
{
 int fault;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!w||!w->work)return -1;
 /* Caller must first mask, disconnect and flush the IRQ DPC, so no notifier
  * can race this close and enqueue after the event has been observed. */
 InterlockedExchange(&w->closing,1);
 if(KeWaitForSingleObject(&w->idle,Executive,KernelMode,FALSE,NULL)!=STATUS_SUCCESS)return -1;
 if(InterlockedCompareExchange(&w->busy,0,0)==1)return -1;
 /* A processing fault means hardware state is uncertain, not that a drained
  * work item may outlive the WDF device. Keep the fault result while freeing
  * the now-dequeued work item and severing its context references. */
 fault=InterlockedCompareExchange(&w->fault,0,0)!=0;
 IoFreeWorkItem(w->work);w->work=NULL;w->process=NULL;w->context=NULL;return fault?-1:0;
}
#endif
