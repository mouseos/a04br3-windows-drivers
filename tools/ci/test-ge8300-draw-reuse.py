#!/usr/bin/env python3
"""One live-draw rearm sequence using actual producer/rearm; no pixel sentinel."""
import argparse,subprocess,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[2];out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);d=root/'drivers/gpu/mt8167/vendor_m1_11/render_owner';s=(d/'windows_native_execute.c').read_text();start=s.index('static NTSTATUS rearm_clear(');end=s.index('NTSTATUS a04_vendor_native_run_clear_frames(',start);actual=s[start:end]
pre=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include "native_pair.h"
#include "native_clear_job.h"
#include "A04BR3RogueDeviceWords.h"
typedef int32_t NTSTATUS,LONG;typedef unsigned ULONG;typedef unsigned char UCHAR;typedef int BOOLEAN;
#define A04_GE8300_NATIVE_OWNER_MDL_TARGET 1
#define A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE 0
#define TRUE 1
#define FALSE 0
#define PASSIVE_LEVEL 0
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER -1
#define STATUS_INVALID_DEVICE_STATE -2
#define STATUS_DEVICE_BUSY -3
#define NT_SUCCESS(x) ((x)>=0)
#define RtlZeroMemory(p,n) memset(p,0,n)
#define RtlCopyMemory memcpy
struct a04_vendor_native_candidate{BOOLEAN attempted,verified,published;uint32_t completed_frames,start[2];BOOLEAN draw_job,visibility_delivered,visibility_armed,owner_pending;void*owner_irq;uint64_t owner_epoch,owner_job;uint32_t owner_generation;struct a04_m111_clear_job job;struct a04_m111_pair_result pair;UCHAR rings[8192];};
struct a04_rgx_fw_runtime{ULONG*command_sync;};
struct memory{int hardware_owned;struct a04_rgx_fw_runtime*runtime;};
struct a04_vendor_render_cold{struct memory*firmware;BOOLEAN poisoned,cleanup_started;struct{void*cpu;}buffer[1];struct{ULONG control[2],commands[2],context;}layout;ULONG client_write[2];};
struct a04_vendor_native_cold{struct a04_vendor_native_candidate*candidate;struct a04_vendor_render_cold*render;int quarantined;struct{ULONG three_d_cleanup[1],ta_cleanup[1],hwrt[1];}addresses;};
struct a04_vendor_start{struct memory*memory;BOOLEAN command_poisoned,shutdown_started,online;LONG command_busy;ULONG command_sequence;};
static unsigned ownership_calls,target_writes;
static ULONG KeGetCurrentIrql(void){return 0;}
static LONG InterlockedCompareExchange(LONG*p,LONG n,LONG e){LONG o=*p;if(o==e)*p=n;return o;}
static LONG InterlockedExchange(LONG*p,LONG n){LONG o=*p;*p=n;return o;}
static void a04_gpu_dma_barrier(void){}
static int read_queue(struct a04_rgx_fw_runtime*f,RGXFWIF_CCB_CTL*k){(void)f;memset(k,0,sizeof(*k));return 0;}
static ULONG a04_vendor_native_output_bytes(struct a04_vendor_native_cold*n){(void)n;return 1921024;}
static int a04_vendor_native_storage_write(struct a04_vendor_native_cold*n,int target,unsigned offset,void*data,unsigned bytes){(void)n;(void)target;(void)offset;(void)data;(void)bytes;target_writes++;return -1;}
static int a04_vendor_native_storage_read(struct a04_vendor_native_cold*n,int target,unsigned offset,void*data,unsigned bytes){(void)n;(void)target;(void)offset;(void)data;(void)bytes;return -1;}
static NTSTATUS a04_vendor_native_mdl_gpu_prepare(struct a04_vendor_native_cold*n){assert(n->candidate->visibility_delivered);ownership_calls++;return 0;}
'''
post=r'''
int main(void){
 struct a04_vendor_native_candidate*c=calloc(1,sizeof(*c)+8192);assert(c);_Alignas(8) UCHAR bank[65536]={0};ULONG sync=2;struct a04_rgx_fw_runtime fw={&sync};struct memory mem={1,&fw};struct a04_vendor_start start={&mem,0,0,1,0,2};struct a04_vendor_render_cold r={0};r.firmware=&mem;r.buffer[0].cpu=bank;r.layout.context=0x1000;r.layout.control[0]=0x2000;r.layout.control[1]=0x2100;r.layout.commands[0]=0x5000;r.layout.commands[1]=0x6000;
 struct a04_vendor_native_cold n={0};n.candidate=c;n.render=&r;n.addresses.hwrt[0]=0xc0083000;n.addresses.ta_cleanup[0]=0xc0083000;n.addresses.three_d_cleanup[0]=0xc0083010;
 c->verified=c->attempted=c->published=c->draw_job=c->visibility_delivered=1;c->completed_frames=1;
 memcpy(c->job.ta+0x58,&n.addresses.hwrt[0],4);ULONG ta_sync=0xc0084c00;memcpy(c->job.ta+0x68,&ta_sync,4);memcpy(c->job.three+0x138,&n.addresses.hwrt[0],4);
 for(unsigned frame=1;frame<=3;frame++){
  for(unsigned i=0;i<2;i++){
   RGXFWIF_CCCB_CTL*ctl=(void*)(bank+r.layout.control[i]);ctl->ui32WrapMask=4095;ctl->ui32ReadOffset=ctl->ui32WriteOffset=r.client_write[i];
   RGXFWIF_CLEANUP_CTL*clean=(void*)(bank+0x3000+i*16);clean->ui32SubmittedCommands=clean->ui32ExecutedCommands=frame;
  }
  ((ULONG*)(void*)(bank+0x4c00))[0]=((ULONG*)(void*)(bank+0x4c00))[1]=frame;
  assert(!a04_vendor_native_owner_rearm_draw(&n,&start));assert(!target_writes&&ownership_calls==frame&&!c->visibility_delivered&&!c->verified&&!c->attempted&&!c->published);
  assert(c->pair.ta_write!=r.client_write[0]&&c->pair.three_d_write!=r.client_write[1]);
  /* Actual scheduler/IRQ completion is external, explicitly mocked here. */
  r.client_write[0]=c->pair.ta_write;r.client_write[1]=c->pair.three_d_write;c->completed_frames++;c->verified=c->attempted=c->published=c->visibility_delivered=1;
 }
 assert(!n.quarantined&&!r.poisoned&&!start.command_poisoned);free(c);puts("live identical Draw rearm: 3 same-root frames, real CCB/UFO producer, zero RT sentinel writes; IRQ/cache mocked PASS");return 0;}
'''
f=out/'actual-rearm.c';f.write_text(pre+actual+post);x=subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(d),str(f),str(d/'native_pair.c'),str(d/'native_ccb.c'),'-o',str(out/'test')],capture_output=True,text=True);(out/'compile.log').write_text(x.stdout+x.stderr);x.check_returncode();x=subprocess.run([str(out/'test')],capture_output=True,text=True);(out/'test.log').write_text(x.stdout+x.stderr);print(x.stdout+x.stderr,end='');x.check_returncode();(out/'receipt.json').write_text(json.dumps({'ASAN_UBSAN':True,'actual_rearm_and_CCB_producer':True,'target_writes':0,'hardware_cache_IRQ_mocked':True,'hardware_verified':False,'source_sha256':hashlib.sha256((d/'windows_native_execute.c').read_bytes()).hexdigest()},indent=2)+'\n')
