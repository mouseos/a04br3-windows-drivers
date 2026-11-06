#!/usr/bin/env python3
"""Exercise real parent GPU interface callbacks with a mocked INFRA mapping."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'drivers/bus/mt8167-shared/wdf_parent_probe.c').read_text()
a=s.index('static NTSTATUS ParentGpuAcquire('); b=s.index('\n#endif',a)
fns=s[a:b]
pre=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include "gpu_lease.h"
#define PASSIVE_LEVEL 0
#define STATUS_INVALID_PARAMETER (-1)
#define STATUS_DEVICE_NOT_READY (-2)
#define STATUS_DEVICE_BUSY (-3)
#define STATUS_DEVICE_HARDWARE_ERROR (-4)
#define STATUS_NOT_SUPPORTED (-5)
#define STATUS_INVALID_DEVICE_STATE (-6)
#define STATUS_SUCCESS 0
#define READ_REGISTER_ULONG(p) (*(p))
typedef int NTSTATUS;typedef uint32_t ULONG;typedef void *PVOID;typedef void *WDFDEVICE;typedef unsigned char UCHAR;
struct a04_gpu_bus_snapshot{ULONG axi_si1,protect_enable,protect_status;};
typedef struct {void *infra;int removed,admissionClosed,pwrapLock;struct a04_gpu_observation_lease gpuLease;} A04_PARENT_CONTEXT;
static A04_PARENT_CONTEXT *ParentContext(WDFDEVICE d){return d;}
static int KeGetCurrentIrql(void){return 0;}
static int InterlockedCompareExchange(int *p,int v,int c){int x=*p;if(x==c)*p=v;return x;}
static int ParentLock(A04_PARENT_CONTEXT *c){if(c->admissionClosed)return -1;assert(!c->pwrapLock);c->pwrapLock=1;return 0;}
static void ParentUnlock(A04_PARENT_CONTEXT *c){assert(c->pwrapLock);c->pwrapLock=0;}
static void ExAcquireFastMutex(int *p){assert(!*p);*p=1;}
static void ExReleaseFastMutex(int *p){assert(*p);*p=0;}
'''
main=r'''
int main(void){A04_PARENT_CONTEXT c={0};ULONG regs[0x300/4]={0},g=0,old;struct a04_gpu_bus_snapshot out={0};c.infra=regs;
 regs[0x204/4]=0x80;regs[0x220/4]=0x24;regs[0x228/4]=0x20;
 assert(ParentGpuAcquire(&c,&g)==0 && g==1);
 assert(ParentGpuAcquire(&c,&old)==STATUS_DEVICE_BUSY);
 assert(ParentGpuSnapshot(&c,g,&out)==0 && out.axi_si1==0x80 && out.protect_enable==0x24 && out.protect_status==0x20);
 assert(ParentGpuAuthorize(&c,g)==STATUS_NOT_SUPPORTED);
 assert(ParentGpuSnapshot(&c,g+1,&out)==STATUS_DEVICE_NOT_READY);
 regs[0x228/4]=0xffffffffu;out.protect_status=123;
 assert(ParentGpuSnapshot(&c,g,&out)==STATUS_DEVICE_HARDWARE_ERROR && out.protect_status==123);
 c.admissionClosed=1;assert(ParentGpuSnapshot(&c,g,&out)==STATUS_DEVICE_NOT_READY);
 assert(ParentGpuRelease(&c,g+1)==STATUS_INVALID_DEVICE_STATE && c.gpuLease.active);
 assert(ParentGpuRelease(&c,g)==0 && !c.gpuLease.active);
 return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';x=Path(d)/'test';p.write_text(pre+fns+main)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(root/'drivers/bus/mt8167-shared'),str(p),'-o',str(x)],check=True)
 subprocess.run([str(x)],check=True)
print('GPU parent callbacks: lease, snapshot, denial and closed rundown pass')
