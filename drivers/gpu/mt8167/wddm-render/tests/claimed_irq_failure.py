#!/usr/bin/env python3
"""Exercise real GeoOwnerInterrupt with claimed fault/no-source provider results."""
import argparse,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--source',type=Path);a=p.parse_args()
src=a.source or Path(__file__).resolve().parents[1]/'lifecycle.c'
s=src.read_text();at=s.index('BOOLEAN GeoOwnerInterrupt(');b=s.index('{',at);i=b+1;level=1
while level:
 level+=(s[i]=='{')-(s[i]=='}');i+=1
code=s[at:i];a.out.mkdir(parents=True,exist_ok=True)
head=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef unsigned UINT;typedef int BOOLEAN;typedef int32_t NTSTATUS;
#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_PENDING 0x103
#define STATUS_NOT_FOUND ((int32_t)0xc0000225)
#define STATUS_IO_DEVICE_ERROR ((int32_t)0xc0000185)
#define STATUS_INVALID_DEVICE_STATE ((int32_t)0xc0000184)
#define GEO_MAX_JOBS 32
#define RtlZeroMemory(p,n) memset(p,0,n)
struct geo_owner_irq_event {uint32_t size,version;uint64_t epoch,job;uint32_t generation;int32_t status;uint32_t visibility,reserved;};
struct geo_kmd_adapter{int notifications_ready,owner_irql;long service_pending;void*dxgk_device;void(*queue_dpc)(void*);};
struct geo_owner_session{struct geo_kmd_adapter*adapter;struct{void*cookie;uint64_t epoch;}lease;struct{NTSTATUS(*interrupt_event)(void*,ULONG,struct geo_owner_irq_event*);}control;};
static int irql=7,poisoned,completed,queued,reads,mode;
static int KeGetCurrentIrql(void){return irql;}
static long InterlockedExchange(long*p,long n){long old=*p;*p=n;return old;}
static BOOLEAN poison(void*p){(void)p;poisoned++;return TRUE;}
static void queue(void*p){(void)p;queued++;}
static NTSTATUS GeoCompleteFromInterrupt(struct geo_kmd_adapter*a,uint64_t job,uint32_t generation,int32_t status,uint32_t visibility){(void)a;assert(job==88&&generation==3&&status==0&&visibility==3);completed++;return STATUS_SUCCESS;}
static NTSTATUS provider(void*p,ULONG message,struct geo_owner_irq_event*e){(void)p;assert(!message);if(reads++)return STATUS_NOT_FOUND;e->size=sizeof(*e);e->version=2;e->epoch=9;if(mode==0)return STATUS_NOT_FOUND;if(mode==1){e->status=STATUS_IO_DEVICE_ERROR;return e->status;}if(mode==2)return STATUS_INVALID_DEVICE_STATE;if(mode==3)return STATUS_PENDING;e->job=88;e->generation=3;e->visibility=3;return STATUS_SUCCESS;}
'''
tail=r'''
int main(void){struct geo_kmd_adapter a={1,7,0,0,queue};struct geo_owner_session s={&a,{(void*)1,9},{provider}};unsigned cases=0;
for(mode=0;mode<=4;mode++){poisoned=completed=queued=reads=0;a.service_pending=0;BOOLEAN got=GeoOwnerInterrupt(&s,0);if(mode==0){assert(!got&&!poisoned&&!completed&&!queued);}else if(mode==1||mode==2){assert(got&&poisoned==1&&!completed&&!queued);}else if(mode==3){assert(got&&!poisoned&&!completed&&queued==1&&a.service_pending==1);}else{assert(got&&!poisoned&&completed==1&&!queued);}cases++;}
irql=2;reads=0;assert(!GeoOwnerInterrupt(&s,0)&&reads==0);cases++;printf("{\"cases_passed\":%u,\"device_test\":false}\n",cases);return 0;}
'''
c=a.out/'claimed-irq.c';c.write_text(head+code+tail)
bin=a.out/'claimed-irq';r=subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra',str(c),'-o',str(bin)],capture_output=True,text=True);(a.out/'compile.log').write_text(r.stdout+r.stderr);r.check_returncode()
r=subprocess.run([str(bin)],capture_output=True,text=True);(a.out/'result.log').write_text(r.stdout+r.stderr)
if r.returncode: raise SystemExit('Claimed IRQ regression failed (see result.log)')
print(r.stdout,end='')
