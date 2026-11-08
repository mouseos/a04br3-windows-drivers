#!/usr/bin/env python3
"""Actual source-linked completed-staging consumer, native/display calls mocked."""
import argparse,subprocess,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
s=(r/'drivers/gpu/mt8167/wddm-display/unified_display.cxx').read_text();start=s.index('extern "C" NTSTATUS GeoDisplayPresentCompleted(');end=s.index('extern "C" NTSTATUS GeoDisplayPower(',start);actual=s[start:end]
pre=r'''#include <stdint.h>
#include <assert.h>
#include <stdio.h>
using UINT=unsigned;using ULONGLONG=uint64_t;using NTSTATUS=int32_t;using PMDL=void*;
#define PASSIVE_LEVEL 0
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define STATUS_INVALID_DEVICE_STATE ((NTSTATUS)0xc0000184)
#define STATUS_PENDING 0x103
#define NT_SUCCESS(s) ((s)>=0)
static unsigned held,closed,acquires,releases,blits,irql;static NTSTATUS acquire_status,release_status,blit_status;
static unsigned KeGetCurrentIrql(){return irql;}
static bool ExAcquireRundownProtection(void*){if(closed)return false;held++;return true;}
static void ExReleaseRundownProtection(void*){assert(held);held--;}
struct Bdd{NTSTATUS PresentCompletedStaging(PMDL m,UINT w,UINT h,UINT p){assert(m==(void*)4&&w==800&&h==600&&p==3200&&held==1);blits++;return blit_status;}};
struct geo_display{Bdd *bdd;bool started;int read_operations;};static geo_display*state(void*p){return (geo_display*)p;}
struct geo_present_read_lease_v1{UINT size,version,width,height,pitch,reserved;PMDL mdl;void*cookie;};
struct geo_present_read_ops_v1{UINT size,version;void*context;NTSTATUS(*acquire)(void*,ULONGLONG,ULONGLONG,UINT,geo_present_read_lease_v1*);NTSTATUS(*release)(void*,void*);};
static NTSTATUS acquire(void*c,ULONGLONG e,ULONGLONG j,UINT g,geo_present_read_lease_v1*v){assert(c==(void*)1&&e==10&&j==20&&g==3&&held==1);acquires++;v->width=800;v->height=600;v->pitch=3200;v->mdl=(void*)4;v->cookie=(void*)5;return acquire_status;}
static NTSTATUS release(void*c,void*k){assert(c==(void*)1&&k==(void*)5&&held==1);releases++;return release_status;}
'''
post=r'''int main(){Bdd b;geo_display d{&b,true,0};geo_present_read_ops_v1 o{sizeof(o),1,(void*)1,acquire,release};
 assert(!GeoDisplayPresentCompleted(&d,&o,10,20,3)&&acquires==1&&releases==1&&blits==1&&!held);
 acquire_status=STATUS_PENDING;assert(GeoDisplayPresentCompleted(&d,&o,10,20,3)==STATUS_DEVICE_NOT_READY&&blits==1&&releases==2&&!held);
 acquire_status=STATUS_INVALID_DEVICE_STATE;assert(GeoDisplayPresentCompleted(&d,&o,10,20,3)==acquire_status&&blits==1&&releases==3&&!held);
 acquire_status=0;blit_status=STATUS_DEVICE_NOT_READY;assert(GeoDisplayPresentCompleted(&d,&o,10,20,3)==blit_status&&releases==4&&!held);
 blit_status=0;release_status=STATUS_PENDING;assert(GeoDisplayPresentCompleted(&d,&o,10,20,3)==STATUS_INVALID_DEVICE_STATE&&releases==5&&!held);
 closed=1;unsigned before=acquires;assert(GeoDisplayPresentCompleted(&d,&o,10,20,3)==STATUS_DEVICE_NOT_READY&&acquires==before&&!held);
 puts("actual completed-staging consumer: no pending-as-success, balanced native lease, release failures, rundown closure PASS");}
'''
f=o/'actual-consumer.cpp';f.write_text(pre+actual+post);x=subprocess.run(['clang++','-nostdlib++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(f),'-l:libstdc++.so.6','-o',str(o/'test')],capture_output=True,text=True);(o/'compile.log').write_text(x.stdout+x.stderr);x.check_returncode();x=subprocess.run([str(o/'test')],capture_output=True,text=True);(o/'test.log').write_text(x.stdout+x.stderr);print(x.stdout,end='');x.check_returncode();(o/'receipt.json').write_text(json.dumps({'ASAN_UBSAN':'PASS','production_consumer_executed':True,'native_and_BDD_mocked':True,'hardware_verified':False},indent=2)+'\n')
