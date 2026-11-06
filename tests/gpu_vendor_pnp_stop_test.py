#!/usr/bin/env python3
"""Exercise the provider's actual D0 exit against IRQ/firmware mock outcomes."""
from pathlib import Path
import os, subprocess, tempfile
s=(Path(__file__).resolve().parents[1]/'drivers/gpu/mt8167/ge8300_provider.c').read_text()
a=s.index('NTSTATUS GpuD0Exit('); b=s.index('{',a); n=1; e=b+1
while n:
    n+=(s[e]=='{')-(s[e]=='}'); e+=1
fn=s[a:e]
pre=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#define A04_GE8300_VENDOR_STAGE 1
#define A04_GE8300_VENDOR_START_DIAGNOSTIC 1
#define A04_GE8300_VENDOR_COPY_SERVICE 1
#define A04_GE8300_VENDOR_COPY_DIAGNOSTIC 1
#define A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC 0
#define A04_GE8300_COPY_SERVICE 0
#define A04_GE8300_RENDER_DIAGNOSTIC 0
#define STATUS_DEVICE_BUSY (-1)
#define STATUS_SUCCESS 0
#define NT_SUCCESS(x) ((x)>=0)
#define TRUE 1
#define FALSE 0
#define UNREFERENCED_PARAMETER(x) (void)(x)
typedef int NTSTATUS; typedef uint32_t ULONG; typedef int WDF_POWER_DEVICE_STATE;
typedef void *WDFDEVICE; typedef unsigned char UCHAR;
struct a04_vendor_irq_snapshot { unsigned count; int valid; };
typedef struct { int vendor_service_closed,vendor_surprise_removed,vendor_service_online; struct { int power_off_ack,shutdown_started; } vendor_start; struct a04_vendor_irq_snapshot vendor_irq_snapshot; void *vendor_irq,*vendor_lifetime; UCHAR *core; struct {int exposed;} vendor_copy;struct {int hardware_owned;} vendor;struct {int processor_released;} firmware; } GPU_CONTEXT;
static int flushes,stops,locks,read_count,drained=1,fwcount=7,irq_status=0;
static GPU_CONTEXT *GpuContext(WDFDEVICE d){return d;}
static long InterlockedExchange(int *p,long v){long old=*p;*p=v;return old;}
static long InterlockedCompareExchange(int *p,long v,long c){long old=*p;if(old==c)*p=v;return old;}
static void a04_vendor_irq_flush(void *p){(void)p;assert(!locks);++flushes;}
static int a04_vendor_irq_work_drained(void *p){(void)p;return drained;}
static void a04_vendor_irq_set_power(void *p,int on){(void)p;assert(!on);}
static void WdfWaitLockAcquire(void *p,void *t){(void)p;(void)t;assert(!locks);locks++;}
static void WdfWaitLockRelease(void *p){(void)p;assert(locks);locks--;}
static int a04_vendor_m1_11_windows_irq_snapshot(void *s,int coherent,struct a04_vendor_irq_snapshot *x){(void)s;(void)coherent;x->valid=1;x->count=fwcount;return irq_status;}
static int a04_vendor_m1_11_windows_stop(void *s,int irq,ULONG *bad,uint64_t *v){(void)s;assert(irq);*bad=0;*v=0;stops++;return 0;}
static ULONG read_reg(volatile ULONG *p){read_count++;return *p;}
#define READ_REGISTER_ULONG(p) read_reg(p)
static void GpuPrint(const char *s){(void)s;} static void GpuHex(ULONG v){(void)v;}
'''
main=r'''
int main(void){GPU_CONTEXT c={0};ULONG regs[0x900/4]={0};c.core=(UCHAR*)regs;c.vendor_start.power_off_ack=1;c.vendor.hardware_owned=1;c.vendor_copy.exposed=1;c.vendor_irq_snapshot.valid=1;c.vendor_irq_snapshot.count=7;
 assert(GpuD0Exit(&c,0)==STATUS_DEVICE_BUSY && flushes==1 && stops==1 && c.vendor.hardware_owned && c.vendor_copy.exposed);
 c.vendor_start.shutdown_started=0;fwcount=8;assert(GpuD0Exit(&c,0)==STATUS_DEVICE_BUSY && stops==1);
 fwcount=7;drained=0;assert(GpuD0Exit(&c,0)==STATUS_DEVICE_BUSY && stops==1);
 drained=1;regs[0x8a8/4]=1;assert(GpuD0Exit(&c,0)==STATUS_DEVICE_BUSY && stops==1);
 regs[0x8a8/4]=0;c.vendor_surprise_removed=1;int before=flushes,reads=read_count;assert(GpuD0Exit(&c,0)==STATUS_DEVICE_BUSY && flushes==before && read_count==reads && stops==1);
 return 0;}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'test.c'; exe=Path(d)/'test'; c.write_text(pre+fn+'\n'+main)
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PnP D0 exit gates RGXStop on IRQ drain and retains owned DMA')
