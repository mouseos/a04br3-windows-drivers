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
assert s.count('GpuD0Entry(')==1 and s.count('GpuD0Exit(')==1, 'production callbacks must never be invoked directly'
fn='#if A04_GE8300_IDLE_TRANSITION_DIAGNOSTIC\n'+function(s,'static NTSTATUS GpuAssignDiagnosticIdle(')+'\n'+function(s,'static void GpuObserveDiagnosticEntry(')+'\n#endif\n'+function(s,'static NTSTATUS GpuRetireToCpu(')+'\n'+function(s,'NTSTATUS GpuD0Exit(')
# These are the real PnP callback registrations, not a disconnected policy test.
assert 'pnp.EvtDeviceD0Entry=GpuD0Entry; pnp.EvtDeviceD0Exit=GpuD0Exit;' in s
add=function(s,'NTSTATUS GpuAdd(')
assert add.index('WdfIoQueueCreate(')<add.index('GpuAssignDiagnosticIdle(device,TRUE)')
post=function(s,'NTSTATUS GpuPostInterrupts(')
assert 'if(InterlockedCompareExchange(&c->vendor_service_closed,0,0))return STATUS_SUCCESS;' in post
assert 'if(c->attempted) return STATUS_SUCCESS;' in function(s,'NTSTATUS GpuD0Entry(')
pre=r'''
#include <assert.h>
#include <string.h>
#include "idle_transition_diagnostic.h"
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
typedef int BOOLEAN;typedef int NTSTATUS;typedef unsigned ULONG;typedef int LONG;typedef void *WDFDEVICE;
typedef int WDF_POWER_DEVICE_STATE;
#define WdfPowerDeviceD3 4
#define WdfFalse 0
#define WdfTrue 1
#define PowerDeviceD3 4
#define IdleCannotWakeFromS0 1
#define DriverManagedIdleTimeout 0
#define IdleDoNotAllowUserControl 0
typedef struct {
 unsigned Size;int IdleCaps,DxState;unsigned IdleTimeout;
 int UserControlOfIdleSettings,Enabled,PowerUpIdleDeviceOnSystemWake,IdleTimeoutType,ExcludeD3Cold;
} WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS;
#if A04_GE8300_IDLE_TRANSITION_DIAGNOSTIC
static int assigns,assign_fail;static WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS captured;
static void WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS *s,int caps){memset(s,0,sizeof(*s));s->Size=sizeof(*s);s->IdleCaps=caps;}
static NTSTATUS WdfDeviceAssignS0IdleSettings(WDFDEVICE d,WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS *s){(void)d;++assigns;captured=*s;return assign_fail?STATUS_IO_DEVICE_ERROR:0;}
#endif
struct start {int online,shutdown_started,command_poisoned,command_busy,power_off_ack;};
typedef struct {
 int attempted,idle_diag_armed,idle_diag_exit_marked,idle_diag_d3_retired,idle_diag_reentry_marked,idle_diag_proof_marked;
 NTSTATUS idle_diag_policy_status;
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
#if A04_GE8300_IDLE_TRANSITION_DIAGNOSTIC
 assert(GpuAssignDiagnosticIdle(0,TRUE)==0 && assigns==1);
 assert(captured.Size==sizeof(captured) && captured.IdleCaps==IdleCannotWakeFromS0 && captured.DxState==PowerDeviceD3);
 assert(captured.IdleTimeout==30000 && captured.IdleTimeoutType==DriverManagedIdleTimeout);
 assert(captured.UserControlOfIdleSettings==IdleDoNotAllowUserControl && captured.Enabled==WdfTrue);
 assert(captured.ExcludeD3Cold==WdfTrue && captured.PowerUpIdleDeviceOnSystemWake==WdfFalse);
 assign_fail=1;assert(GpuAssignDiagnosticIdle(0,TRUE)==STATUS_IO_DEVICE_ERROR);assign_fail=0;
#endif
 for(int i=0;i<5;i++){
  GPU_CONTEXT c={0};c.vendor.hardware_owned=1;c.vendor_copy.exposed=1;
  c.vendor_start.online=1;c.vendor_service_online=1;c.shared_lease.active=17;
  c.idle_diag_armed=1;c.attempted=1;
  scenario=i;masked=cleans=offs=0;
  int result=GpuD0Exit(&c,WdfPowerDeviceD3);
  assert(!locked && c.vendor_service_closed && !c.vendor_service_online);
  assert(c.vendor.hardware_owned && c.vendor_copy.exposed && c.shared_lease.active==17);
  assert(c.vendor_cpu_fallback==(i==0));assert((result==0)==(i==0));
  if(!i){
   assert(GpuD0Exit(&c,WdfPowerDeviceD3)==0 && cleans==1 && offs==1);
#if A04_GE8300_IDLE_TRANSITION_DIAGNOSTIC
   assert(c.idle_diag_exit_marked && c.idle_diag_d3_retired);
   int before=assigns;GpuObserveDiagnosticEntry(&c,0);assert(assigns==before && !c.idle_diag_reentry_marked);
   GpuObserveDiagnosticEntry(&c,WdfPowerDeviceD3);
   assert(assigns==before+1 && captured.Enabled==WdfFalse && !c.idle_diag_armed && c.idle_diag_reentry_marked);
   GpuObserveDiagnosticEntry(&c,WdfPowerDeviceD3);assert(assigns==before+1);
#endif
  }
#if A04_GE8300_IDLE_TRANSITION_DIAGNOSTIC
  if(i){assert(c.idle_diag_exit_marked && !c.idle_diag_d3_retired);int before=assigns;GpuObserveDiagnosticEntry(&c,WdfPowerDeviceD3);assert(assigns==before && !c.idle_diag_reentry_marked);}
#endif
  if(i==1)assert(!cleans && !offs);
  if(i==2)assert(cleans==1 && !offs);
 }
 for(int i=0;i<4;i++){
  GPU_CONTEXT c={0};c.vendor.hardware_owned=1;c.vendor_start.online=1;
  if(i==0)c.vendor_start.command_poisoned=1;
  if(i==1)c.vendor_render.poisoned=1;
  if(i==2)c.vendor_start.command_busy=1;
  if(i==3)c.vendor_start.shutdown_started=1;
  masked=cleans=offs=0;assert(GpuD0Exit(&c,WdfPowerDeviceD3)!=0 && !c.vendor_cpu_fallback);
  assert(!masked && !cleans && !offs);
 }
 GPU_CONTEXT cold={0};assert(GpuD0Exit(&cold,WdfPowerDeviceD3)==0 && cold.vendor_cpu_fallback);
 return 0;
}
'''
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as tmp:
    tmp=Path(tmp);p=tmp/'test.c';p.write_text(pre+fn+main)
    for diagnostic in (0,1):
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',f'-DA04_GE8300_IDLE_TRANSITION_DIAGNOSTIC={diagnostic}','-I'+str(g),str(p),'-o',str(tmp/'test')],check=True)
        subprocess.run([str(tmp/'test')],check=True)
print('actual provider D0Exit: OFF/ON, 30-second KMDF policy, one-shot D3 observations/disarm, failures and DMA retention passed')
