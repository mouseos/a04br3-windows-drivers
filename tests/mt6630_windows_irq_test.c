/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "windows_irq.h"
static ULONG regs[1024];static int queued,notifications,disconnects,flushes,ignore_mask,no_mmio;static BOOLEAN (*isr)(PKINTERRUPT,PVOID);static PVOID isr_context;static KINTERRUPT object;static PKDPC pending;
int KeGetCurrentIrql(void){return 0;}
ULONG READ_REGISTER_ULONG(volatile ULONG *p){assert(!no_mmio);return *p;}
void WRITE_REGISTER_ULONG(volatile ULONG *p,ULONG v){assert(!no_mmio);if(p==&regs[3])*p&=~v;else if(p==&regs[4]&&ignore_mask)*p|=0x80;else *p=v;}
void KeInitializeDpc(PKDPC d,void (*fn)(PKDPC,PVOID,PVOID,PVOID),PVOID ctx){d->fn=fn;d->context=ctx;}
BOOLEAN KeInsertQueueDpc(PKDPC d,PVOID a,PVOID b){(void)a;(void)b;if(queued)return 0;queued=1;pending=d;return 1;}
BOOLEAN KeRemoveQueueDpc(PKDPC d){(void)d;return 0;}
void KeFlushQueuedDpcs(void){flushes++;if(queued){queued=0;pending->fn(pending,pending->context,0,0);}}
NTSTATUS IoConnectInterrupt(PKINTERRUPT *out,BOOLEAN (*fn)(PKINTERRUPT,PVOID),PVOID ctx,void *lock,ULONG v,KIRQL a,KIRQL b,int mode,BOOLEAN share,KAFFINITY affinity,BOOLEAN save)
{(void)lock;(void)a;(void)b;(void)save;assert(v==155&&mode==LevelSensitive&&!share&&affinity==1);isr=fn;isr_context=ctx;*out=&object;return 0;}
void IoDisconnectInterrupt(PKINTERRUPT p){assert(p==&object);disconnects++;}
BOOLEAN KeSynchronizeExecution(PKINTERRUPT p,BOOLEAN (*f)(PVOID),PVOID c){assert(p==&object);return f(c);}
static void notify(void *p){assert(p==&notifications);notifications++;}
int main(void){struct a04_windows_irq irq={0};CM_PARTIAL_RESOURCE_DESCRIPTOR raw={0},tr={0};
 raw.Type=tr.Type=CmResourceTypeInterrupt;raw.ShareDisposition=tr.ShareDisposition=CmResourceShareDeviceExclusive;raw.u.Interrupt.Vector=141;tr.u.Interrupt.Vector=155;tr.u.Interrupt.Level=4;tr.u.Interrupt.Affinity=1;
 raw.u.Interrupt.Vector=142;assert(a04_windows_irq_connect(&irq,(UCHAR*)regs,&raw,&tr,notify,&notifications)<0);raw.u.Interrupt.Vector=141;
 regs[4]=0x180;assert(!a04_windows_irq_connect(&irq,(UCHAR*)regs,&raw,&tr,notify,&notifications));assert(regs[4]==0x100);assert(!a04_windows_irq_arm(&irq)&&regs[4]==0x180);
 regs[3]=0x180;assert(isr(&object,isr_context)&&regs[3]==0x100&&regs[4]==0x100&&queued);assert(!isr(&object,isr_context));assert(!a04_windows_irq_stop(&irq)&&disconnects==1&&flushes==1&&!notifications&&!irq.base);
 memset(&irq,0,sizeof irq);regs[4]=0;assert(!a04_windows_irq_connect(&irq,(UCHAR*)regs,&raw,&tr,notify,&notifications));assert(!a04_windows_irq_arm(&irq));
 regs[3]=0x80;assert(isr(&object,isr_context)&&queued);ignore_mask=1;assert(a04_windows_irq_stop(&irq)<0);assert(disconnects==2&&flushes==2&&!irq.interrupt&&!irq.base&&!notifications);
 memset(&irq,0,sizeof irq);regs[4]=0x80;assert(a04_windows_irq_connect(&irq,(UCHAR*)regs,&raw,&tr,notify,&notifications)<0&&!irq.interrupt);
 ignore_mask=0;regs[4]=0;memset(&irq,0,sizeof irq);assert(!a04_windows_irq_connect(&irq,(UCHAR*)regs,&raw,&tr,notify,&notifications));assert(!a04_windows_irq_arm(&irq));regs[3]=0x80;assert(isr(&object,isr_context)&&queued);
 no_mmio=1;assert(!a04_windows_irq_remove(&irq));assert(disconnects==3&&flushes==3&&!irq.interrupt&&!irq.base&&!notifications);assert(!a04_windows_irq_remove(&irq));return 0;}
