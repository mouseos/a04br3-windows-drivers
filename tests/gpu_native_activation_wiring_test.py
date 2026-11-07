#!/usr/bin/env python3
"""実providerのactivation blockを抽出してhostでcompile/run。GPU実行ではない。"""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'drivers/gpu/mt8167/ge8300_provider.c').read_text()
post=source[source.index('NTSTATUS GpuPostInterrupts('):source.index('NTSTATUS GpuD0Exit(')]
a=post.index('#if A04_GE8300_VENDOR_NATIVE_CLEAR_ACTIVATE\n        /* vendor_lifetime')
b=post.index('\n#endif\n#if A04_GE8300_VENDOR_RENDER_COLD',a)+len('\n#endif')
block=post[a:b]
start=post.index('#if A04_GE8300_VENDOR_NATIVE_BACKING\n    if(c->vendor_native')
stop=post.index('    if(!c->vendor.staged',start)
retained=post[start:stop]
entry=source[source.index('NTSTATUS GpuD0Entry('):source.index('NTSTATUS GpuPostInterrupts(')]
start=entry.index('#if A04_GE8300_VENDOR_NATIVE_BACKING\n        {')
stop=entry.index('        /* Retain device context',start)
prepare=entry[start:stop]
assert post.index('WdfWaitLockAcquire')<post.index('a04_vendor_m1_11_windows_fence')<a<b<post.index('c->vendor_service_online=TRUE')<post.index('WdfWaitLockRelease')
assert post.rstrip().endswith('#endif') and 'return STATUS_SUCCESS;' in post[b:]
pre=r'''
#include <assert.h>
#include <string.h>
#include "windows_native_cold.h"
#include "windows_start.h"
#include "native_activation_policy.h"
struct a04_vendor_render_cold {void *copy_owner;};
struct fake_copy {int exposed,prepared,task_frozen;void *render;};
struct fake_context {
 NTSTATUS vendor_native_prep_status;
 int vendor_native_activation_attempted,vendor_copy_device_ref,vendor_service_online;
 LONG vendor_service_closed;
 struct fake_copy vendor_copy;struct a04_vendor_render_cold vendor_render;
 struct {NTSTATUS health_status;} status;
 struct {uint32_t active;} shared_lease;
 void *shared_owner;int shared_common_active;
 struct a04_vendor_native_cold *vendor_native;struct a04_vendor_start vendor_start;
};
BOOLEAN a04_vendor_native_job_staged(const struct a04_vendor_native_cold *n){return n!=NULL;}
unsigned irql,calls;NTSTATUS wanted;int retain;char uart[4096];
unsigned KeGetCurrentIrql(void){return irql;}
void GpuPrint(const char *s){assert(strlen(uart)+strlen(s)<sizeof(uart));strcat(uart,s);}
void GpuHex(uint32_t x){(void)x;}
NTSTATUS a04_vendor_native_run_once(struct a04_vendor_native_cold *n,struct a04_vendor_start *s,struct a04_vendor_native_result *r)
{(void)n;(void)s;calls++;r->phase=4;r->matching_pixels=480000;r->completed_frames=1;return wanted;}
NTSTATUS a04_vendor_native_run_clear_frames(struct a04_vendor_native_cold *n,struct a04_vendor_start *s,ULONG frames,struct a04_vendor_native_result *r)
{assert(frames==A04_GE8300_VENDOR_NATIVE_CLEAR_FRAMES);NTSTATUS status=a04_vendor_native_run_once(n,s,r);if(status==STATUS_SUCCESS)r->completed_frames=frames;return status;}
NTSTATUS a04_vendor_native_allocate_cold_ex(struct a04_vendor_native_cold **n,struct a04_vendor_render_cold *r,PDMA_ADAPTER adapter,ULONG maps,ULONG width,ULONG height,struct a04_vendor_native_diagnostic *d)
{(void)r;(void)adapter;(void)maps;(void)width;(void)height;*n=(wanted==STATUS_SUCCESS || retain)?(void *)n:NULL;d->status=wanted;return wanted;}
ULONG a04_vendor_native_bytes(const struct a04_vendor_native_cold *n){return n?0x13c000:0;}
NTSTATUS prepare_child(struct fake_context *c,NTSTATUS status){PDMA_ADAPTER adapter=NULL;ULONG copy_map_registers=1100;

'''
middle=r'''
return status;}
NTSTATUS retained_child(struct fake_context *c){
'''
activation=r'''
return STATUS_DEVICE_NOT_READY;}
void activate(struct fake_context *c){(void)c;
'''
postlude=r'''
}
void init(struct fake_context *c){
 memset(c,0,sizeof(*c));calls=irql=0;retain=0;wanted=STATUS_SUCCESS;uart[0]=0;
 c->vendor_native=(void *)c;c->vendor_native_prep_status=STATUS_SUCCESS;
 c->vendor_copy_device_ref=1;c->vendor_copy.exposed=c->vendor_copy.prepared=c->vendor_copy.task_frozen=1;
 c->vendor_copy.render=&c->vendor_render;c->vendor_render.copy_owner=&c->vendor_copy;
}
int main(void){struct fake_context c;
 init(&c);wanted=STATUS_INSUFFICIENT_RESOURCES;
 assert(prepare_child(&c,STATUS_SUCCESS)==STATUS_SUCCESS && !c.vendor_native && c.vendor_native_prep_status==wanted);
 assert(retained_child(&c)==STATUS_DEVICE_NOT_READY);activate(&c);assert(!calls && !c.vendor_service_closed);
 init(&c);wanted=STATUS_INSUFFICIENT_RESOURCES;retain=1;
 assert(prepare_child(&c,STATUS_SUCCESS)==wanted && c.vendor_native);
 assert(retained_child(&c)==STATUS_SUCCESS);activate(&c);assert(!calls);
 init(&c);assert(prepare_child(&c,STATUS_SUCCESS)==STATUS_SUCCESS && c.vendor_native);
 assert(retained_child(&c)==STATUS_DEVICE_NOT_READY);activate(&c);
#if A04_GE8300_VENDOR_NATIVE_CLEAR_ACTIVATE
 assert(calls==1 && c.vendor_native_activation_attempted);
 assert(strstr(uart,"STATUS=") && strstr(uart,"PHASE=") && strstr(uart,"MATCH=") && strstr(uart,"MISMATCH=") && strstr(uart,"GUARD="));
 activate(&c);assert(calls==1);
 init(&c);c.vendor_native=0;activate(&c);assert(!calls && !c.vendor_service_closed && !strstr(uart,"POISON"));
 init(&c);c.vendor_native_prep_status=STATUS_INSUFFICIENT_RESOURCES;activate(&c);assert(!calls && !c.vendor_service_closed && !strstr(uart,"POISON"));
 init(&c);irql=1;activate(&c);assert(!calls);
 init(&c);c.status.health_status=STATUS_IO_TIMEOUT;activate(&c);assert(!calls);
 init(&c);c.shared_lease.active=1;activate(&c);assert(!calls);
 init(&c);c.shared_owner=&c;activate(&c);assert(!calls);
 init(&c);c.shared_common_active=1;activate(&c);assert(!calls);
 init(&c);c.vendor_service_online=1;activate(&c);assert(!calls);
 init(&c);c.vendor_copy.render=0;activate(&c);assert(!calls);
 init(&c);wanted=STATUS_IO_TIMEOUT;activate(&c);
 assert(calls==1 && c.vendor_service_closed && !c.vendor_service_online && c.status.health_status==STATUS_IO_TIMEOUT);
 activate(&c);assert(calls==1 && strstr(uart,"DMA_RETAINED"));
#else
 assert(!calls && !c.vendor_native_activation_attempted);
#endif
 return 0;}
'''
tmp=root/'build/native-activation-wiring';tmp.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['TMPDIR']=str(tmp)
with tempfile.TemporaryDirectory(dir=tmp) as directory:
 p=Path(directory);(p/'test.c').write_text(pre+prepare+middle+retained+activation+block+postlude)
 for enabled,frames in [(0,1),(1,1),(1,4)]:
  cmd=['cc','-std=c11','-Wall','-Wextra','-Werror',f'-DA04_GE8300_VENDOR_NATIVE_CLEAR_ACTIVATE={enabled}',f'-DA04_GE8300_VENDOR_NATIVE_CLEAR_FRAMES={frames}','-DA04_GE8300_VENDOR_NATIVE_BACKING=1','-DA04_GE8300_VENDOR_NATIVE_CLEAR_JOB=1','-DA04_GE8300_SHARED_COMMON_PAGE=1','-I'+str(root/'tests/gpu_windows_stubs'),'-I'+str(root/'drivers/gpu/mt8167/vendor_m1_11'),str(p/'test.c'),'-o',str(p/'test')]
  subprocess.run(cmd,env=env,check=True);subprocess.run([str(p/'test')],env=env,check=True)
print('Production D0 activation block: flag0/1, PASSIVE/GP/lease/admission, UART, one-shot/failure closure PASS; host model only')
