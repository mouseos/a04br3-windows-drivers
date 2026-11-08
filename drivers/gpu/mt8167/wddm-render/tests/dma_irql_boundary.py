#!/usr/bin/env python3
"""Execute production legacy-DMA boundary helpers, including errors and callback."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser();p.add_argument('--scratch',type=Path,required=True);a=p.parse_args();a.scratch.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[2];s=(r/'vendor_m1_11/render_owner/native_owner_mdl.c').read_text();fn=s[s.index('static NTSTATUS get_sg_dispatch('):s.index('NTSTATUS a04_native_mdl_release_cold(')]
shim=r'''
#include <assert.h>
#include <stddef.h>
typedef int NTSTATUS;typedef unsigned ULONG;typedef int BOOLEAN;typedef unsigned KIRQL;typedef void* PVOID;typedef void* PMDL;typedef void* PDEVICE_OBJECT;typedef void* PIRP;typedef void* PSCATTER_GATHER_LIST;
#define DISPATCH_LEVEL 2
struct map_wait{int called;};
struct adapter;typedef struct adapter* PDMA_ADAPTER;
typedef void(*CALLBACK)(PDEVICE_OBJECT,PIRP,PSCATTER_GATHER_LIST,PVOID);
struct ops{NTSTATUS(*GetScatterGatherList)(PDMA_ADAPTER,PDEVICE_OBJECT,PMDL,PVOID,ULONG,CALLBACK,PVOID,BOOLEAN);void(*PutScatterGatherList)(PDMA_ADAPTER,PSCATTER_GATHER_LIST,BOOLEAN);};struct adapter{struct ops*DmaOperations;};
static KIRQL irql;static int raise_count,lower_count,result,get_calls,put_calls,callback_mode;static BOOLEAN direction;
static void KeRaiseIrql(KIRQL level,KIRQL*previous){assert(irql==0&&level==2);*previous=irql;irql=level;raise_count++;}
static void KeLowerIrql(KIRQL previous){assert(irql==2&&previous==0);irql=previous;lower_count++;}
static void mapped(PDEVICE_OBJECT d,PIRP p,PSCATTER_GATHER_LIST list,PVOID ctx){assert(irql==2&&d==(void*)3&&!p&&list==(void*)4);((struct map_wait*)ctx)->called++;}
static NTSTATUS get(PDMA_ADAPTER a,PDEVICE_OBJECT d,PMDL m,PVOID addr,ULONG bytes,CALLBACK callback,PVOID ctx,BOOLEAN to_device){assert(irql==2&&a&&d==(void*)3&&m==(void*)5&&addr==(void*)6&&bytes==4096&&to_device==direction);get_calls++;if(callback_mode)callback(d,0,(void*)4,ctx);return result;}
static void put(PDMA_ADAPTER a,PSCATTER_GATHER_LIST sg,BOOLEAN to_device){assert(irql==2&&a&&sg==(void*)4&&to_device==direction);put_calls++;}
'''
main=r'''
int main(void){struct ops ops={get,put};struct adapter a={&ops};struct map_wait wait;for(direction=0;direction<2;direction++){for(callback_mode=0;callback_mode<2;callback_mode++){for(result=-1;result<=0;result++){wait.called=0;assert(get_sg_dispatch(&a,(void*)3,(void*)5,(void*)6,4096,&wait,direction)==result);assert(irql==0&&wait.called==callback_mode);}}put_sg_dispatch(&a,(void*)4,direction);assert(irql==0);}assert(get_calls==8&&put_calls==2&&raise_count==10&&lower_count==10);return 0;}
'''
f=a.scratch/'dma-irql.c';f.write_text(shim+fn+main);exe=a.scratch/'dma-irql';subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(f),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True);print('PRODUCTION_SG_CALLS_AT_DISPATCH_RETURN_PASSIVE_ON_SUCCESS_FAILURE_AND_CALLBACK_PASS')
