#!/usr/bin/env python3
"""Execute actual transfer state machine + actual CCB producer, mocked hardware/WDK."""
import argparse, subprocess,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--clear-cycle',action='store_true');a=p.parse_args();root=Path(__file__).resolve().parents[2];out=a.output.resolve()
if not out.is_relative_to(root) or out.exists():raise ValueError('New checkout-local output required')
out.mkdir(parents=True);d=root/'drivers/gpu/mt8167/vendor_m1_11/render_owner';src=d/'windows_native_copy_transfer.c'
actual='\n'.join(x for x in src.read_text().splitlines() if not x.startswith('#include'))
pre=r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "native_copy_queue.h"
#include "A04BR3RogueDeviceWords.h"
typedef int32_t NTSTATUS,LONG;typedef uint32_t ULONG;typedef uint64_t ULONGLONG;typedef uint8_t UCHAR;typedef int BOOLEAN;typedef struct{int64_t QuadPart;}LARGE_INTEGER;
#define TRUE 1
#define FALSE 0
#define PASSIVE_LEVEL 0
#define DISPATCH_LEVEL 2
#define NonPagedPoolNx 0
#define STATUS_SUCCESS 0
#define STATUS_UNSUCCESSFUL -1
#define STATUS_IO_DEVICE_ERROR -2
#define STATUS_INVALID_PARAMETER -3
#define STATUS_DEVICE_BUSY -4
#define STATUS_INVALID_DEVICE_STATE -5
#define STATUS_IO_TIMEOUT -6
#define STATUS_INTEGER_OVERFLOW -7
#define STATUS_PENDING 0x103
#define STATUS_INSUFFICIENT_RESOURCES -8
#define STATUS_NOT_SUPPORTED -9
#define NT_SUCCESS(x) ((x)>=0)
#define RtlZeroMemory(p,n) memset(p,0,n)
static unsigned irql,cache_gpu,cache_cpu,doorbells,allocs,frees;static uint64_t ticks;
static ULONG KeGetCurrentIrql(void){return irql;}
static ULONGLONG KeQueryInterruptTime(void){return ticks+=10000;}
static int KeDelayExecutionThread(int a,int b,LARGE_INTEGER*c){(void)a;(void)b;(void)c;return 0;}
#define KernelMode 0
static LONG InterlockedExchange(volatile LONG*p,LONG n){LONG o=*p;*p=n;return o;}
static LONG InterlockedCompareExchange(volatile LONG*p,LONG n,LONG e){LONG o=*p;if(o==e)*p=n;return o;}
static void*ExAllocatePoolWithTag(int p,size_t n,ULONG t){(void)p;(void)t;allocs++;return calloc(1,n);}
static void ExFreePoolWithTag(void*p,ULONG t){(void)t;frees++;free(p);}
static void a04_gpu_dma_barrier(void){}
struct a04_rgx_fw_runtime{RGXFWIF_CCB_CTL*kctl,*fctl;RGXFWIF_KCCB_CMD*kcommands;ULONG*command_sync;RGXFWIF_DEV_VIRTADDR command_sync_addr;};
struct firmware{BOOLEAN hardware_owned,uncached;struct a04_rgx_fw_runtime*runtime;};
struct a04_vendor_render_cold{struct a04_vendor_native_cold*native_owner;BOOLEAN poisoned,cleanup_started;struct firmware*firmware;ULONG sequence,client_write[4];struct{void*cpu;}buffer[1];struct{ULONG control[4],commands[4],sync,tq;}layout;};
struct a04_vendor_native_cold{struct a04_vendor_render_cold*render;BOOLEAN quarantined;};
struct a04_vendor_start{BOOLEAN online,command_poisoned,shutdown_started;volatile LONG command_busy;ULONG command_sequence;volatile UCHAR*core;struct firmware*memory;BOOLEAN attempted;};
struct a04_native_irq_core{volatile UCHAR*core;struct a04_rgx_fw_runtime*runtime;uint64_t epoch,serial;BOOLEAN powered,masked,closed,faulted,draining;};
struct a04_native_irq_receipt{uint64_t epoch,serial;ULONG status,ack_status;NTSTATUS fault;};
struct a04_native_copy_storage{struct a04_vendor_native_cold*n;struct a04_native_copy_job_image image;BOOLEAN owned;};
struct a04_native_copy_gpu_result{ULONG phase,task_sequence,flush_sequence,client_write;};
static struct a04_vendor_native_cold*a04_native_copy_storage_parent(struct a04_native_copy_storage*s){return s->n;}
static const struct a04_native_copy_job_image*a04_native_copy_storage_image(struct a04_native_copy_storage*s){return &s->image;}
static const struct a04_native_copy_job_image*a04_native_copy_storage_clear_image(struct a04_native_copy_storage*s){return &s->image;}
static NTSTATUS a04_native_copy_storage_gpu_prepare(struct a04_native_copy_storage*s){assert(irql==0);cache_gpu++;s->owned=1;return 0;}
static NTSTATUS a04_native_copy_storage_cpu_complete(struct a04_native_copy_storage*s){assert(irql==0);cache_cpu++;s->owned=0;return 0;}
static BOOLEAN a04_native_copy_storage_gpu_owned(struct a04_native_copy_storage*s){return s->owned;}
static NTSTATUS a04_native_irq_rearm(struct a04_native_irq_core*q,uint64_t epoch,uint64_t serial){assert(irql>2);if(q->epoch!=epoch||q->serial!=serial)return -1;q->masked=0;return 0;}
static void write_doorbell(volatile ULONG*p,ULONG v);
#define WRITE_REGISTER_ULONG(p,v) write_doorbell(p,v)
'''
# The firmware typed alias is extracted from the shipped ABI, not a hand-sized command packet.
pre=pre.replace('RGXFWIF_DEV_VIRTADDR command_sync_addr','RGXFWIF_DEV_VIRTADDR command_sync_addr')
factory=(d/'native_owner_factory.c').read_text();begin=factory.index('static int32_t prepare_copy(');end=factory.index('struct discard_call',begin);factory=factory[begin:end]
factory_mocks=r'''
#define GeoFactoryCheckpoint(...) ((void)0)
#define GEO_USAGE_RENDER_TARGET 1
#define GEO_USAGE_STAGING 2
#define E_JOB_PREPARED 1
#define E_JOB_COMPLETE 3
#define E_OWNER_FAILED 3
struct e_native_resource{uint64_t id;struct{unsigned usage;}desc;void*linear;};
struct test_allocation{struct a04_vendor_start start;struct firmware firmware;void*binding;};
static unsigned boot_calls;
struct e_native_owner{struct e_native_resource resources[2];struct test_allocation*allocation;struct a04_native_tq_transfer*copy_transfer;uint64_t copy_destination,surface,next_id,hardware_epoch,job;ULONG kick_write,job_state,state;BOOLEAN job_is_copy;};
static NTSTATUS enter(struct e_native_owner*c,BOOLEAN cleanup){(void)c;(void)cleanup;return 0;}
static NTSTATUS leave(struct e_native_owner*c,NTSTATUS status){(void)c;return status;}
static NTSTATUS wait_boundary_locked(struct e_native_owner*c){return c->job_state==E_JOB_COMPLETE?0:STATUS_DEVICE_BUSY;}
static struct e_native_resource*resource(struct e_native_owner*c,uint64_t id){for(int i=0;i<2;i++)if(c->resources[i].id==id)return &c->resources[i];return NULL;}
static NTSTATUS e_native_prepare_copy_storage_locked(struct e_native_owner*c){assert(c->copy_transfer&&!c->allocation->firmware.hardware_owned);return 0;}
/* Cold first-use bootstrap only. No stop/retire/rebuild stub exists. */
static NTSTATUS e_native_boot_locked(struct e_native_owner*c){assert(!c->allocation->start.online&&!c->allocation->start.attempted&&!c->allocation->firmware.hardware_owned);boot_calls++;c->allocation->start.online=1;c->allocation->start.attempted=1;c->allocation->firmware.hardware_owned=1;c->allocation->start.memory->hardware_owned=1;return 0;}
'''
post=r'''
static struct a04_native_tq_transfer*active;
static void write_doorbell(volatile ULONG*p,ULONG v){(void)p;assert(v==0x10);doorbells++;struct a04_vendor_render_cold*r=active->native->render;struct a04_rgx_fw_runtime*f=r->firmware->runtime;
 ULONG at=f->kctl->ui32ReadOffset;RGXFWIF_KCCB_CMD*cmd=&f->kcommands[at];
 if(cmd->eCmdType==RGXFWIF_KCCB_CMD_KICK){RGXFWIF_CCCB_CTL*ctl=(void*)((UCHAR*)r->buffer[0].cpu+r->layout.control[3]);ctl->ui32ReadOffset=ctl->ui32WriteOffset=ctl->ui32DepOffset=active->queue.write_offset;*(ULONG*)((UCHAR*)r->buffer[0].cpu+r->layout.sync)=active->queue.completion_value;}
 if(cmd->eCmdType==RGXFWIF_KCCB_CMD_SYNC)*f->command_sync=cmd->uCmdData.sSyncData.uiUpdateVal;
 f->kctl->ui32ReadOffset=f->kctl->ui32WriteOffset;
}
int main(void){
 _Alignas(8) UCHAR data[65536]={0},core[4096]={0};RGXFWIF_CCB_CTL k={0},fctl={0};RGXFWIF_KCCB_CMD cmds[128]={0};ULONG cs=0;
 struct a04_rgx_fw_runtime fw={&k,&fctl,cmds,&cs,{0}};struct firmware mem={0,1,&fw};
 struct a04_vendor_render_cold r={0};struct a04_vendor_native_cold n={&r,0};r.native_owner=&n;r.firmware=&mem;r.buffer[0].cpu=data;r.layout.control[3]=0;r.layout.commands[3]=4096;r.layout.sync=8192;r.layout.tq=9000;
 RGXFWIF_CCCB_CTL*ctl=(void*)data;ctl->ui32WrapMask=4095;k.ui32WrapMask=127;k.ui32CmdSize=sizeof(RGXFWIF_KCCB_CMD);fctl.ui32WrapMask=31;fctl.ui32CmdSize=sizeof(RGXFWIF_FWCCB_CMD);
 struct a04_native_copy_storage storage={&n,{0},0};struct a04_native_tq_transfer*c=NULL;
 struct a04_vendor_start s={0,0,0,0,0,core,&mem,0};struct a04_native_irq_core q={core,&fw,23,0,1,0,0,0,0};
 assert(!a04_native_tq_transfer_prepare(&c,&storage));active=c;mem.hardware_owned=1;
 struct test_allocation allocation={s,mem,(void*)1};allocation.firmware.hardware_owned=0;struct e_native_owner owner={0};owner.allocation=&allocation;owner.resources[0]=(struct e_native_resource){1,{GEO_USAGE_RENDER_TARGET},NULL};owner.resources[1]=(struct e_native_resource){2,{GEO_USAGE_STAGING},(void*)1};owner.copy_transfer=c;owner.copy_destination=2;owner.surface=1;owner.hardware_epoch=23;
 unsigned negative=0;ULONG last_write=0;
 for(unsigned j=1;j<=64;j++){
  irql=0;allocation.start=s;owner.next_id=j-1;owner.job_state=E_JOB_COMPLETE;uint64_t job=0;assert(!prepare_copy(&owner,1,2,&job)&&job==j&&owner.job_is_copy);s=allocation.start;assert(c->input.write==last_write);assert(c->queue.completion_value==j);
  assert(a04_native_tq_transfer_rearm(c,&s,23,j+1)<0);negative++;
  assert(a04_native_tq_transfer_release_unsubmitted(&c)<0&&c);negative++;
  irql=3;assert(a04_native_tq_transfer_submit(c,&s,&q,k.ui32WriteOffset,j+1,j)<0);negative++;
  assert(a04_native_tq_transfer_submit(c,&s,&q,k.ui32WriteOffset,j,0)<0);negative++;
  q.epoch=24;assert(a04_native_tq_transfer_submit(c,&s,&q,k.ui32WriteOffset,j,j)<0);negative++;q.epoch=23;
  assert(!a04_native_tq_transfer_submit(c,&s,&q,k.ui32WriteOffset,j,j));assert(s.command_busy==1&&storage.owned);
  q.masked=1;struct a04_native_irq_receipt receipt={23,q.serial,0,0,0};
  assert(a04_native_tq_transfer_begin_finish(c,&q,&receipt)<0);negative++;
  receipt.serial=++q.serial;assert(!a04_native_tq_transfer_begin_finish(c,&q,&receipt));
  irql=0;struct a04_native_copy_gpu_result result;assert(!a04_native_tq_transfer_finish_gpu(c,&s,&result));assert(result.phase==4&&!storage.owned&&s.command_busy==1);
  assert(a04_native_tq_transfer_rearm(c,&s,23,j+1)<0);negative++;
  irql=3;assert(!a04_native_tq_transfer_arm_visibility(c,&s,&q));q.masked=1;
  assert(a04_native_tq_transfer_visibility_receipt(c,&s,&q,&receipt)<0);negative++;
  receipt.serial=++q.serial;assert(!a04_native_tq_transfer_visibility_receipt(c,&s,&q,&receipt));assert(!s.command_busy&&c->state==7);
  assert(a04_native_tq_transfer_identity(c,23,j,j));assert(!a04_native_tq_transfer_identity(c,23,j,j+1));negative++;
  last_write=c->queue.write_offset;
 }
 irql=0;assert(a04_native_tq_transfer_rearm(c,&s,24,65)<0);negative++;
 assert(a04_native_tq_transfer_release_retired(&c)<0&&c);negative++;
 /* Only actual shutdown proof supplied by factory permits graph retirement. */
 mem.hardware_owned=0;q.core=NULL;q.powered=0;assert(!a04_native_tq_transfer_release_retired(&c)&&!c);
 assert(boot_calls==1);assert(allocs==1&&frees==1&&cache_gpu==64&&cache_cpu==64&&doorbells==256);
 printf("persistent copy: 64 complete transactions, %u negative gates; actual CCB/transfer code; hardware/cache mocked\n",negative);return 0;
}
'''

if a.clear_cycle:
 full=(d/'native_owner_factory.c').read_text();factory=full[full.index('static int32_t prepare(void *p,'):full.index('struct discard_call')]
 pre=pre.replace('struct a04_native_copy_job_image image;BOOLEAN owned;','struct a04_native_copy_job_image image;BOOLEAN owned;struct a04_native_copy_job_image clear_image;').replace('a04_native_copy_storage_clear_image(struct a04_native_copy_storage*s){return &s->image;}','a04_native_copy_storage_clear_image(struct a04_native_copy_storage*s){return &s->clear_image;}')
 factory_mocks=factory_mocks.replace('void*binding;','void*binding;void*native;').replace('ULONG kick_write,job_state,state;','ULONG kick_write,job_state,state,colour;struct a04_native_copy_storage*copy_storage;')
 factory_mocks+=r"""
#define native_progress(...) ((void)0)
static NTSTATUS e_native_stop_locked(struct e_native_owner*c){(void)c;assert(!"Live Clear must not stop firmware");return -1;}
static NTSTATUS e_native_retire_graph_locked(struct e_native_owner*c){(void)c;assert(!"Live Clear must not retire graph");return -1;}
static NTSTATUS e_native_build_graph_locked(struct e_native_owner*c){(void)c;assert(!"Live Clear must not rebuild graph");return -1;}
static NTSTATUS a04_vendor_native_recolour_bound_cold(void*n,ULONG colour){(void)n;(void)colour;assert(!"Live Clear must not rewrite USC/PDS");return -1;}
"""
 post=post.replace('struct a04_native_copy_storage storage={&n,{0},0};','struct a04_native_copy_storage storage={0};storage.n=&n;\n _Alignas(8) struct a04_native_clear_job_input fill={UINT64_C(0x8200000000),4194304,UINT64_C(0x8200000000),1921024,800,600,800,0,0};assert(!a04_native_clear_job_build(&storage.clear_image,&fill));')
 post=post.replace('struct test_allocation allocation={s,mem,(void*)1};','struct test_allocation allocation={s,mem,(void*)1,NULL};')
 post=post.replace('owner.copy_transfer=c;','owner.copy_storage=&storage;owner.copy_transfer=c;').replace('j<=64','j<=4')
 post=post.replace('assert(!prepare_copy(&owner,1,2,&job)&&job==j&&owner.job_is_copy);','assert(!(j%2?prepare_copy(&owner,1,2,&job):prepare(&owner,1,j==2?0xa5a5a5a5u:0xff00ff00u,&job))&&job==j&&owner.job_is_copy);')
 post=post.replace('s=allocation.start;assert(c->input.write==last_write);','s=allocation.start;assert(c->input.write==last_write);if(!(j%2)){struct a04_native_copy_job_image expected=storage.clear_image;expected.task[2]=owner.colour;assert(!memcmp(c->ring+c->queue.task_offset+40,expected.task,208));}')
 post=post.replace('cache_gpu==64&&cache_cpu==64&&doorbells==256','cache_gpu==4&&cache_cpu==4&&doorbells==16').replace('persistent copy: 64 complete transactions','Copy/Clear alternating: 4 complete transactions')

f=out/'actual-transfer.c';f.write_text(pre+actual+factory_mocks+factory+post)
cmd=['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g','-I'+str(d),str(f),str(d/'native_copy_queue.c'),str(d/'native_ccb.c'),'-o',str(out/'test')]
if a.clear_cycle:cmd[cmd.index('-o'):cmd.index('-o')]=[str(d/'native_copy_job.c'),str(d/'native_copy_state.c'),str(d.parent/'clear_image.c')]
x=subprocess.run(cmd,capture_output=True,text=True);(out/'compile.log').write_text(x.stdout+x.stderr);x.check_returncode()
x=subprocess.run([str(out/'test')],capture_output=True,text=True);(out/'test.log').write_text(x.stdout+x.stderr);print(x.stdout+x.stderr,end='');x.check_returncode()
(out/'receipt.json').write_text(json.dumps({'actual_transfer_source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'complete_transactions':4 if a.clear_cycle else 64,'clear_copy_alternation':a.clear_cycle,'ASAN_UBSAN':True,'hardware_cache_irq_mocked':True,'hardware_verified':False},indent=2)+'\n')
