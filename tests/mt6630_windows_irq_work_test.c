/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "windows_irq_work.h"
static IO_WORKITEM item;static DEVICE_OBJECT device;static void (*worker)(PDEVICE_OBJECT,PVOID);static void *worker_context;static int freed,calls,fail;
int KeGetCurrentIrql(void){return 0;}
PIO_WORKITEM IoAllocateWorkItem(PDEVICE_OBJECT d){assert(d==&device);return &item;}
void IoFreeWorkItem(PIO_WORKITEM p){assert(p==&item);freed++;}
void IoQueueWorkItem(PIO_WORKITEM p,void (*cb)(PDEVICE_OBJECT,PVOID),int q,PVOID c){assert(p==&item&&q==DelayedWorkQueue&&!worker);worker=cb;worker_context=c;}
void KeInitializeEvent(KEVENT *e,int type,int signaled){(void)type;e->state=signaled;}
void KeClearEvent(KEVENT *e){e->state=0;}
void KeSetEvent(KEVENT *e,int a,int b){(void)a;(void)b;e->state=1;}
NTSTATUS KeWaitForSingleObject(KEVENT *e,int a,int b,int c,void *d){(void)a;(void)b;(void)c;(void)d;if(!e->state){assert(worker);worker(&device,worker_context);worker=0;}return e->state?0:-1;}
static int process(void *p){assert(p==&calls);calls++;return fail;}
int main(void){struct a04_irq_work w={0};assert(!a04_irq_work_init(&w,&device,process,&calls));a04_irq_work_notify(&w);assert(w.busy==1&&!w.idle.state);assert(!a04_irq_work_drain(&w));assert(!calls&&freed==1&&!w.work);
 memset(&w,0,sizeof w);assert(!a04_irq_work_init(&w,&device,process,&calls));a04_irq_work_notify(&w);worker(&device,worker_context);worker=0;assert(calls==1&&w.busy==2);assert(!a04_irq_work_drain(&w)&&freed==2);
 memset(&w,0,sizeof w);assert(!a04_irq_work_init(&w,&device,process,&calls));a04_irq_work_notify(&w);a04_irq_work_notify(&w);assert(w.fault);assert(a04_irq_work_drain(&w)&&freed==2&&w.work);return 0;}
