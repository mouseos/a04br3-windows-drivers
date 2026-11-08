#!/usr/bin/env python3
"""One production caller sequence; hardware/retirement/transfer completion mocked."""
import argparse,subprocess,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False);d=r/'drivers/gpu/mt8167/vendor_m1_11/render_owner'
s=(d/'native_owner_factory.c').read_text();runtime=(d/'native_owner_factory_runtime.c').read_text()
def extract(text,name):
 pos=text.index(name+'(');start=text.rfind('\n',0,pos)+1;brace=text.index('{',pos);i=brace+1;depth=1
 while depth:depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]
# Staging branch is verbatim production bind. Other branches are not executed.
bind=s[s.index('static int32_t bind('):s.index('    BOOLEAN vertex=',s.index('static int32_t bind('))]+'    return leave(c,STATUS_NOT_SUPPORTED);\n}\n'
pre=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef int32_t NTSTATUS;typedef int BOOLEAN;typedef unsigned ULONG;typedef uint64_t ULONGLONG;
#define TRUE 1
#define FALSE 0
#define PASSIVE_LEVEL 0
#define STATUS_SUCCESS 0
#define STATUS_DEVICE_BUSY -1
#define STATUS_INVALID_PARAMETER -2
#define STATUS_INVALID_DEVICE_STATE -3
#define STATUS_NOT_SUPPORTED -4
#define STATUS_INTEGER_OVERFLOW -5
#define NT_SUCCESS(x) ((x)>=0)
#define E_JOB_NONE 0
#define E_JOB_PREPARED 1
#define E_JOB_SUBMITTED 2
#define E_JOB_COMPLETE 3
#define E_OWNER_FAILED 3
#define GEO_USAGE_RENDER_TARGET 1
#define GEO_USAGE_STAGING 2
#define GEO_USAGE_INDEX_BUFFER 4
#define GEO_USAGE_TEXTURE2D 5
#define GeoFactoryCheckpoint(...) ((void)0)
typedef struct{ULONG ui32WrapMask,ui32ReadOffset,ui32WriteOffset;}RGXFWIF_CCB_CTL;
struct a04_native_copy_storage{int exists;};struct a04_native_tq_transfer{int phase;};
struct a04_vendor_start{BOOLEAN online,attempted;};
struct a04_native_owner_allocation{void*native,*binding,*frontend_orphan;struct a04_vendor_start start;struct{int hardware_owned;struct{void*kctl;}*runtime;}firmware;};
struct e_native_resource{struct e_native_resource*next;uint64_t id;struct{ULONG usage,pitch,height;}desc;void*linear;uint64_t linear_offset;};
struct e_native_owner{struct e_native_resource*resources;struct a04_native_copy_storage*copy_storage;struct a04_native_tq_transfer*copy_transfer;struct a04_native_owner_allocation*allocation;int memory;uint64_t next_id,surface,copy_destination,hardware_epoch,job;ULONG job_state,state,kick_write;BOOLEAN job_is_copy;};
static unsigned stop_calls,retire_calls,graph_calls,storage_calls,transfer_calls,boot_calls,rearms;
static struct a04_native_copy_storage storage;static struct a04_native_tq_transfer transfer;
static const unsigned char a04_copy_usc_asset[152]={0};
static ULONG KeGetCurrentIrql(void){return 0;}
static NTSTATUS enter(struct e_native_owner*c,BOOLEAN cleanup){(void)c;(void)cleanup;return 0;}
static NTSTATUS leave(struct e_native_owner*c,NTSTATUS s){(void)c;return s;}
static struct e_native_resource*resource(struct e_native_owner*c,uint64_t id){for(struct e_native_resource*r=c->resources;r;r=r->next)if(r->id==id)return r;return NULL;}
static NTSTATUS wait_boundary_locked(struct e_native_owner*c){return (c->job_state==E_JOB_NONE||c->job_state==E_JOB_COMPLETE)?0:STATUS_DEVICE_BUSY;}
static NTSTATUS e_native_stop_locked(struct e_native_owner*c){assert(c->job_state==E_JOB_COMPLETE);stop_calls++;c->allocation->firmware.hardware_owned=0;c->allocation->start.online=0;c->job_state=E_JOB_NONE;return 0;}
static NTSTATUS e_native_retire_graph_locked(struct e_native_owner*c){assert(!c->allocation->firmware.hardware_owned&&!c->allocation->start.online);retire_calls++;c->allocation->start.attempted=0;c->allocation->native=NULL;assert(!c->copy_storage&&!c->copy_transfer);return 0;}
static NTSTATUS e_native_build_graph_locked(struct e_native_owner*c){assert(!c->allocation->native);graph_calls++;c->allocation->native=(void*)1;return 0;}
static NTSTATUS a04_native_allocation_bind_frontend_linear_cold(struct a04_native_owner_allocation*a,void*api,void*mdl,uint32_t bytes,int input,void**out){(void)api;assert(mdl&&bytes==1920000&&!input&&a->native&&!a->firmware.hardware_owned);*out=(void*)2;return 0;}
static NTSTATUS a04_vendor_native_attach_linear_cold(void*n,void*linear,uint64_t aperture){assert(n&&linear&&aperture==1921024);return 0;}
static NTSTATUS a04_native_copy_storage_create(struct a04_native_copy_storage**out,void*n,void*linear,const void*desc,const void*usc,unsigned bytes){assert(!*out&&n&&linear&&desc&&usc&&bytes==152);storage_calls++;*out=&storage;return 0;}
static NTSTATUS a04_native_tq_transfer_prepare(struct a04_native_tq_transfer**out,struct a04_native_copy_storage*s){assert(!*out&&s);transfer_calls++;*out=&transfer;transfer.phase=1;return 0;}
static NTSTATUS e_native_prepare_copy_storage_locked(struct e_native_owner*);
static NTSTATUS e_native_boot_locked(struct e_native_owner*c){assert(!c->allocation->firmware.hardware_owned&&!c->allocation->start.attempted&&c->copy_storage&&c->copy_transfer);boot_calls++;c->hardware_epoch++;c->allocation->start.online=1;c->allocation->start.attempted=1;c->allocation->firmware.hardware_owned=1;return 0;}
static NTSTATUS a04_native_tq_transfer_rearm(struct a04_native_tq_transfer*t,struct a04_vendor_start*s,uint64_t epoch,uint64_t job){assert(t&&s->online&&epoch==8&&job>1&&(t->phase==1||t->phase==7));rearms++;t->phase=1;return 0;}
static int a04_rgx_device_read_words(void*out,void*in,unsigned bytes){assert(bytes==sizeof(RGXFWIF_CCB_CTL));memcpy(out,in,bytes);return 0;}
'''
post=r'''
int main(void){
 RGXFWIF_CCB_CTL k={127,0,0};struct{void*kctl;}fw={&k};
 struct a04_native_owner_allocation a={0};a.native=(void*)1;a.binding=(void*)2;a.start=(struct a04_vendor_start){1,1};a.firmware.hardware_owned=1;a.firmware.runtime=(void*)&fw;
 struct e_native_resource dst={0},src={0};src.next=&dst;src.id=1;src.desc.usage=GEO_USAGE_RENDER_TARGET;dst.id=2;dst.desc.usage=GEO_USAGE_STAGING;dst.desc.pitch=3200;dst.desc.height=600;
 struct e_native_owner c={0};c.resources=&src;c.allocation=&a;c.surface=1;c.next_id=1;c.job_state=E_JOB_COMPLETE;c.hardware_epoch=7;
 assert(!bind(&c,2,(void*)3,0,1920000,1921024));
 assert(stop_calls==1&&retire_calls==1&&graph_calls==1&&c.job_state==E_JOB_NONE&&!a.start.online&&!a.firmware.hardware_owned&&a.binding&&dst.linear);
 assert(storage_calls==1&&transfer_calls==1&&c.copy_transfer&&c.copy_destination==2);
 uint64_t first=0;assert(!prepare_copy(&c,1,2,&first)&&first==2&&c.job_state==E_JOB_PREPARED&&c.job_is_copy);
 assert(boot_calls==1&&rearms==1&&c.hardware_epoch==8);
 /* Mock the EXISTING actual ISR/TQ/SLC/GP/cache + freshIRQ chain as complete.
  * The real transfer chain is exercised separately by the64transaction test. */
 c.job_state=E_JOB_COMPLETE;c.copy_transfer->phase=7;
 uint64_t second=0;assert(!prepare_copy(&c,1,2,&second)&&second==3&&c.job_state==E_JOB_PREPARED);
 assert(stop_calls==1&&retire_calls==1&&graph_calls==1&&boot_calls==1&&rearms==2&&storage_calls==1&&transfer_calls==1);
 puts("production caller sequence: Clear COMPLETE -> actual staging bind/cold_for_bind -> cold graph/real bound handles -> firstCopy asset/boot/arm -> mocked actual completion -> same-root secondCopy rearm PASS");return 0;}
'''
actual=extract(runtime,'e_native_prepare_copy_storage_locked')+'\n'+extract(s,'cold_for_bind')+'\n'+bind+'\n'+extract(s,'prepare_copy')
f=o/'actual-sequence.c';f.write_text(pre+actual+post)
x=subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(f),'-o',str(o/'test')],capture_output=True,text=True);(o/'compile.log').write_text(x.stdout+x.stderr);x.check_returncode()
x=subprocess.run([str(o/'test')],capture_output=True,text=True);(o/'test.log').write_text(x.stdout+x.stderr);print(x.stdout+x.stderr,end='');x.check_returncode()
(o/'receipt.json').write_text(json.dumps({'production_sequence_pass':True,'actual_functions':['bind linear branch','cold_for_bind','prepare_copy','e_native_prepare_copy_storage_locked'],'hardware_retirement_firmware_irq_cache_mocked':True,'ASAN_UBSAN':True,'hardware_verified':False,'sources':{str(x.relative_to(r)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [d/'native_owner_factory.c',d/'native_owner_factory_runtime.c']}},indent=2)+'\n')
