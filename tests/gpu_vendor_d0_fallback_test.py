#!/usr/bin/env python3
"""Execute provider retirement, actual D0Exit and IRQ masking on host doubles."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1];g=root/'drivers/gpu/mt8167'
def function(source,signature):
    start=source.index(signature);brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
s=(g/'ge8300_provider.c').read_text()
fn=function(s,'static NTSTATUS GpuRetireToCpu(')+'\n'+function(s,'NTSTATUS GpuD0Exit(')
# These are the real PnP callback registrations, not a disconnected policy test.
assert 'pnp.EvtDeviceD0Entry=GpuD0Entry; pnp.EvtDeviceD0Exit=GpuD0Exit;' in s
post=function(s,'NTSTATUS GpuPostInterrupts(')
assert 'if(InterlockedCompareExchange(&c->vendor_service_closed,0,0))return STATUS_SUCCESS;' in post
assert 'if(c->attempted) return STATUS_SUCCESS;' in function(s,'NTSTATUS GpuD0Entry(')
pre=r'''
#include <assert.h>
#include <string.h>
#define A04_GE8300_VENDOR_STAGE 1
#define A04_GE8300_VENDOR_START_DIAGNOSTIC 1
#define A04_GE8300_VENDOR_COPY_SERVICE 1
#define A04_GE8300_SHARED_LEASE 1
#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_DEVICE_BUSY (-1)
#define STATUS_NOT_SUPPORTED (-2)
#define STATUS_IO_DEVICE_ERROR (-3)
#define NT_SUCCESS(x) ((x)>=0)
#define UNREFERENCED_PARAMETER(x) (void)(x)
typedef int NTSTATUS;typedef unsigned ULONG;typedef int LONG;typedef void *WDFDEVICE;
typedef int WDF_POWER_DEVICE_STATE;
struct start {int online,shutdown_started,command_poisoned,command_busy,power_off_ack;};
typedef struct {
 LONG vendor_service_closed;int vendor_service_online,vendor_cpu_fallback;
 struct {int hardware_owned;} vendor;struct {int exposed;} vendor_copy;
 struct start vendor_start;
 struct {int poisoned;unsigned cleanup_mask;} vendor_render;
 struct {int shutdown_status,firmware_started;} status;
 struct {int active;} shared_lease;
 struct {int processor_released;} firmware;
 void *vendor_irq,*vendor_lifetime;
} GPU_CONTEXT;
static int locked,masked,cleans,offs,scenario;
static int InterlockedExchange(int *p,int v){int old=*p;*p=v;return old;}
static int InterlockedCompareExchange(int *p,int v,int cmp){int old=*p;if(old==cmp)*p=v;return old;}
static void WdfWaitLockAcquire(void *p,void *t){(void)p;(void)t;assert(!locked);locked=1;}
static void WdfWaitLockRelease(void *p){(void)p;assert(locked);locked=0;}
static GPU_CONTEXT *GpuContext(void *p){return p;}
static int a04_vendor_irq_retire(void *p){(void)p;assert(locked);++masked;return scenario!=1;}
static void a04_vendor_irq_set_power(void *p,int b){(void)p;assert(locked && !b);}
static void GpuPrint(const char *s){(void)s;} static void GpuHex(ULONG n){(void)n;}
static NTSTATUS a04_vendor_render_cleanup_contexts(void *r,struct start *s,unsigned *failed){
 (void)s;assert(locked && masked);++cleans;*failed=4;
 if(scenario==2)return STATUS_DEVICE_BUSY;
 ((GPU_CONTEXT *)0); /* never dereference; r is updated through its field type below */
 unsigned *mask=(unsigned *)((char*)r+sizeof(int));*mask=15;return 0;
}
static NTSTATUS a04_vendor_m1_11_windows_forced_power_off(struct start *s,int coherent,NTSTATUS *idle){
 assert(locked && coherent && cleans);++offs;s->shutdown_started=1;*idle=0;
 if(scenario==3)return STATUS_DEVICE_BUSY;
 if(scenario==4)return 0; /* Reject a success status without the actual ack. */
 s->online=0;s->power_off_ack=1;return 0;
}
'''.replace(' ((GPU_CONTEXT *)0); /* never dereference; r is updated through its field type below */','')
main=r'''
int main(void){
 for(int i=0;i<5;i++){
  GPU_CONTEXT c={0};c.vendor.hardware_owned=1;c.vendor_copy.exposed=1;
  c.vendor_start.online=1;c.vendor_service_online=1;c.shared_lease.active=17;
  scenario=i;masked=cleans=offs=0;
  int result=GpuD0Exit(&c,3);
  assert(!locked && c.vendor_service_closed && !c.vendor_service_online);
  assert(c.vendor.hardware_owned && c.vendor_copy.exposed && c.shared_lease.active==17);
  assert(c.vendor_cpu_fallback==(i==0));assert((result==0)==(i==0));
  if(!i){assert(GpuD0Exit(&c,3)==0 && cleans==1 && offs==1);}
  if(i==1)assert(!cleans && !offs);
  if(i==2)assert(cleans==1 && !offs);
 }
 for(int i=0;i<4;i++){
  GPU_CONTEXT c={0};c.vendor.hardware_owned=1;c.vendor_start.online=1;
  if(i==0)c.vendor_start.command_poisoned=1;
  if(i==1)c.vendor_render.poisoned=1;
  if(i==2)c.vendor_start.command_busy=1;
  if(i==3)c.vendor_start.shutdown_started=1;
  masked=cleans=offs=0;assert(GpuD0Exit(&c,3)!=0 && !c.vendor_cpu_fallback);
  assert(!masked && !cleans && !offs);
 }
 GPU_CONTEXT cold={0};assert(GpuD0Exit(&cold,3)==0 && cold.vendor_cpu_fallback);
 return 0;
}
'''
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as tmp:
    tmp=Path(tmp);p=tmp/'test.c';p.write_text(pre+fn+main)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
print('actual provider D0Exit: bounded retirement, failures, duplicate exit and DMA/lease retention passed')
