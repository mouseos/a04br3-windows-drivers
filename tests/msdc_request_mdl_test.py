#!/usr/bin/env python3
"""Compare real baseline/current DMA wrapper and request handlers.

No tablet/device access. All generated files and compiler temps stay in --out.
DMA hardware is mocked; allocation, mapping and flush lifetimes are checked.
"""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
BASE = 'e2db926f1ddd12b4cae341a7df50f832e20c16e5'
REL = 'drivers/storage/mt8167-msdc/miniport.c'
p = argparse.ArgumentParser(); p.add_argument('--out', required=True)
a = p.parse_args(); out = Path(a.out).resolve(); out.mkdir(parents=True, exist_ok=True)
assert ROOT.parent.parent in out.parents, 'Output must be under this project root'
env = dict(os.environ, TMPDIR=str(out))
old = subprocess.check_output(['git','-C',str(ROOT),'show',BASE+':'+REL], text=True)
new = (ROOT/REL).read_text()
def extract(s, start, end):
    i=s.index(start); j=s.index(end,i); return s[i:j]
struct = extract(old,'struct a04_probe_extension {','\nstatic void A04Trace')
sense = extract(old,'static void A04Sense(','\nstatic BOOLEAN A04RunIo')
def handler(s):
    body = extract(s,'static BOOLEAN A04RunIo(','\n#if defined(A04_PASSIVE_IO) && defined(A04_ISOLATED_REQUEST_EXPERIMENT)')
    # Legacy baseline shifts a 32-bit LBA by 32 in a UART-only argument.
    # Normalize that single argument equally in all host fixtures so UBSan
    # can check actual request/MMIO/DMA logic. Production source is untouched;
    # exact full-TU restoration above independently covers its error logging.
    return body.replace('(ULONG)(result.blocks.lba >> 32)',
                        '(ULONG)((uint64_t)result.blocks.lba >> 32)')
def build(s): return extract(s,'static BOOLEAN A04BuildIo(','\n/* One StartIo owner')
common = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pio.h"
#include "dma.h"
#include "scsi.h"
#include "gpt_guard.h"
#include "io_gate.h"
#include "io_trace.h"
typedef uint32_t ULONG;typedef int32_t LONG;typedef uint8_t UCHAR,BOOLEAN;typedef void*PVOID;
#define TRUE 1
#define FALSE 0
#define UNREFERENCED_PARAMETER(p) (void)(p)
#define SRB_STATUS_SUCCESS 1
#define SRB_STATUS_ERROR 4
#define SRB_STATUS_INVALID_REQUEST 6
#define SRB_STATUS_NO_DEVICE 8
#define SRB_STATUS_AUTOSENSE_VALID 0x80
#define SRB_FUNCTION_EXECUTE_SCSI 0
#define SRB_FUNCTION_IO_CONTROL 2
#define STOR_STATUS_SUCCESS 0
#define RequestComplete 0
struct _SRB {UCHAR Function,Cdb[16],CdbLength,PathId,TargetId,Lun,SrbStatus,ScsiStatus;
 void*DataBuffer;ULONG DataTransferLength;void*SenseInfoBuffer;UCHAR SenseInfoBufferLength;void*SrbExtension;};
typedef struct _SRB SCSI_REQUEST_BLOCK,*PSCSI_REQUEST_BLOCK;
typedef struct {ULONG HeaderLength,Timeout,ControlCode,ReturnCode,Length;} SRB_IO_CONTROL,*PSRB_IO_CONTROL;
struct a04_request_extension {void*read_buffer;};
struct input {UCHAR cdb[16],len,function,target;ULONG count,available;int failed,writable,cancel,fail_dma,build_only,no_ext,oom,badfreq,cancel_after;};
struct event {ULONG type,a,b,c;};
struct output {UCHAR data[4096],sense[256];ULONG status,scsi,length,requests,failed,returned,events,notifications;
 struct event event[128];char trace[4096];ULONG tracebytes;};
'''
(out/'common.h').write_text(common)
for name,source,define in [('base',old,''),('verbose',new,''),('quiet',new,'#define A04_MSDC_REQUEST_MDL_REUSE 1\n')]:
    variant = f'#define a04_msdc_dma_sector_timed {name}_dma\n' + define + '#ifndef A04_MSDC_REQUEST_MDL_REUSE\n#define A04_MSDC_REQUEST_MDL_REUSE 0\n#endif\n' + r'''
#define A04_READONLY_DISK_PROBE
#define A04_DMA_PROBE
#define A04_DMA_DISK_PROBE
#define A04_ENABLE_WRITES
#define A04_VIRTUAL_BOUNCE_EXPERIMENT
#define A04_BOOT_EXPERIMENT
#define A04_PASSIVE_IO
#define A04_SERIAL_QUEUE_EXPERIMENT
#define A04_ISOLATED_REQUEST_EXPERIMENT
#define A04_BOOT_PASSIVE_EXPERIMENT
#define A04_EMMC_IDENTITY_HANDOFF
#include "common.h"
'''+struct+r'''
static struct output*o;static const struct input*in;
static UCHAR A04EmmcIdentity[7]={0x11,'T','E','S','T','0','1'};
static BOOLEAN A04EmmcIdentityValid=TRUE;
static void event(ULONG type,ULONG a,ULONG b,ULONG c){assert(o->events<128);o->event[o->events++]=(struct event){type,a,b,c};}
static void A04Trace(const char*s){size_t n=strlen(s);assert(o->tracebytes+n<sizeof(o->trace));memcpy(o->trace+o->tracebytes,s,n+1);o->tracebytes+=(ULONG)n;}
static void A04TraceHex(ULONG n){char s[9];snprintf(s,sizeof(s),"%08X",n);A04Trace(s);}
static ULONG A04Read32(void*p,ULONG off){struct a04_probe_extension*q=p;ULONG v=q->registers[off/4];event(1,off,v,0);return v;}
static UCHAR A04Read8(void*p,ULONG off){return (UCHAR)A04Read32(p,off);}
static void A04Write32(void*p,ULONG off,ULONG v){struct a04_probe_extension*q=p;event(2,off,v,0);q->registers[off/4]=v;}
static LONG InterlockedCompareExchange(volatile LONG*p,LONG n,LONG c){LONG v=*p;if(v==c)*p=n;return v;}
static int StorPortGetSystemAddress(void*p,PSCSI_REQUEST_BLOCK r,void**b){(void)p;*b=r->DataBuffer;event(4,0,0,0);return 0;}
static void StorPortNotification(int what,void*p,PSCSI_REQUEST_BLOCK r){(void)what;(void)p;(void)r;o->notifications++;}
typedef struct {void *buffer; int built;} MDL,*PMDL;
typedef struct {int64_t QuadPart;} LARGE_INTEGER;
static unsigned allocated,freed,built,flushes,transfers;
static PMDL IoAllocateMdl(void *buffer,ULONG bytes,BOOLEAN a,BOOLEAN b,void*c){
 (void)a;(void)b;(void)c;assert(bytes==512);if(in->oom)return NULL;
 PMDL m=calloc(1,sizeof(*m));assert(m);m->buffer=buffer;allocated++;return m;
}
static void IoFreeMdl(PMDL m){assert(m&&m->built);freed++;free(m);}
static void MmBuildMdlForNonPagedPool(PMDL m){assert(m&&!m->built);m->built=1;built++;}
static void KeFlushIoBuffers(PMDL m,BOOLEAN read,BOOLEAN dma){assert(m&&m->built&&dma);flushes++;event(5,read,512,0);}
static LARGE_INTEGER KeQueryPerformanceCounter(LARGE_INTEGER *f){if(f)f->QuadPart=in->badfreq?0:1000;return (LARGE_INTEGER){10};}
enum a04_dma_result a04_msdc_dma_sector_timed(const struct a04_msdc_io*io,uint32_t lba,
 uint64_t bus,uint32_t align,uint32_t polls,int write,uint16_t rca,const struct a04_dma_clock *clock){
 struct a04_probe_extension*p=io->context;ULONG h=2166136261u,j;
 (void)bus;(void)align;assert(polls==100000000u&&rca==1&&clock->ticks==10000);transfers++;
 if(!write)for(j=0;j<512;j++)p->dma_buffer[j]=(UCHAR)(lba*3+j);
 for(j=0;j<512;j++)h=(h^p->dma_buffer[j])*16777619u;
 event(3,lba,write,h);
 if(in->cancel_after==(int)transfers)p->cancelled=1;
 return in->fail_dma==(int)transfers?A04_DMA_INVALID:A04_DMA_OK;
}
'''+extract(source,'struct a04_clock_context {','\n#endif\n\n#ifdef A04_ENABLE_WRITES')+build(source)+sense+handler(source)+f'''
void {name}_run(const struct input*i,struct output*r){{
 struct a04_probe_extension p={{0}};struct a04_request_extension ext={{0}};
 SCSI_REQUEST_BLOCK s={{0}};ULONG regs[64]={{0}};UCHAR dma[512]={{0}};unsigned j;
 memset(r,0,sizeof(*r));o=r;in=i;allocated=freed=built=flushes=transfers=0;for(j=0;j<sizeof(r->data);j++)r->data[j]=(UCHAR)(j*7+5);
 p.registers=regs;p.dma_buffer=dma;p.sectors=4096;p.requests=i->count;p.failed=i->failed;
 p.write_ready=i->writable;p.cancelled=i->cancel;p.esp_first=50;p.esp_last=59;p.write_first=100;p.write_last=999;
 regs[4]=0xd700u;s.Function=i->function;s.CdbLength=i->len;memcpy(s.Cdb,i->cdb,16);
 s.TargetId=i->target;s.DataBuffer=r->data;s.DataTransferLength=i->available;
 s.SenseInfoBuffer=r->sense;s.SenseInfoBufferLength=sizeof(r->sense)-1;s.SrbExtension=i->no_ext?NULL:&ext;
 r->returned=i->build_only?A04BuildIo(&p,&s):A04RunIo(&p,&s);
 assert(allocated==freed&&allocated==built);
#if A04_MSDC_REQUEST_MDL_REUSE
 assert(allocated==(flushes?1u:0u));
#else
 assert(allocated==flushes);
#endif
 if(flushes==8)assert(allocated==(A04_MSDC_REQUEST_MDL_REUSE?1u:8u));
 r->status=s.SrbStatus;r->scsi=s.ScsiStatus;r->length=s.DataTransferLength;r->requests=p.requests;r->failed=p.failed;
}}
'''
    (out/(name+'.c')).write_text(variant)
main = r'''
#include "common.h"
void base_run(const struct input*,struct output*);void verbose_run(const struct input*,struct output*);void quiet_run(const struct input*,struct output*);
static ULONG cases,normal_old,normal_new,error_cases;
static void run(struct input*i){struct output a,b,c;
 base_run(i,&a);verbose_run(i,&b);quiet_run(i,&c);
 assert(!memcmp(&a,&b,sizeof(a)));assert(!strcmp(a.trace,c.trace));
 if(c.status!=SRB_STATUS_SUCCESS&&!i->build_only)assert(strstr(c.trace,"A04WIN CDB="));
 if(i->fail_dma && !i->cancel_after)assert(strstr(c.trace,"A04WIN transfer failed result="));
 normal_old+=a.tracebytes;normal_new+=c.tracebytes;if(c.status!=1)error_cases++;
 a.tracebytes=b.tracebytes=c.tracebytes=0;memset(a.trace,0,sizeof(a.trace));memset(b.trace,0,sizeof(b.trace));memset(c.trace,0,sizeof(c.trace));
 assert(!memcmp(&a,&c,sizeof(a)));cases++;
}
int main(void){
 struct input i={.len=10,.available=4096,.writable=1};ULONG counts[]={0,94,95,96,126,127,128,0xfffffffeu,0xffffffffu};
 UCHAR commands[]={0,0x03,0x12,0x1a,0x25,0x28,0x2a,0x35,0x88,0x8a,0xff};
 for(unsigned c=0;c<sizeof(commands);c++)for(unsigned n=0;n<sizeof(counts)/sizeof(*counts);n++){
  memset(i.cdb,0,sizeof(i.cdb));i.cdb[0]=commands[c];i.count=counts[n];i.len=commands[c]>=0x80?16:10;
  if(i.len==16){i.cdb[9]=101;i.cdb[13]=8;}else{i.cdb[5]=101;i.cdb[8]=8;}
  run(&i);
 }
 i=(struct input){.len=10,.available=4096,.writable=1,.cdb={0x28,0,0,0,0,100,0,0,8}};
 for(unsigned n=0;n<16;n++){i.count=n;run(&i);}
 i.cdb[0]=0x2a;i.cdb[5]=50;for(unsigned n=0;n<16;n++){i.count=n;run(&i);}
 for(int sector=1;sector<=8;sector++){i.fail_dma=sector;run(&i);}i.fail_dma=0;
 for(int sector=1;sector<=8;sector++){i.cancel_after=sector;run(&i);}i.cancel_after=0;
 i.oom=1;run(&i);i.oom=0;i.badfreq=1;run(&i);i.badfreq=0;
 for(int blocks=1;blocks<=8;blocks++){i.cdb[8]=blocks;run(&i);}i.cdb[8]=8;
 i.cancel=1;run(&i);i.cancel=0;
 i.failed=1;run(&i);i.failed=0;i.writable=0;run(&i);i.writable=1;
 i.cdb[5]=20;run(&i);i.target=1;run(&i);i.target=0;
 i.function=2;run(&i);i.function=0;i.cdb[0]=0x28;i.build_only=1;run(&i);i.no_ext=1;run(&i);
 printf("{\"cases\":%u,\"original_trace_bytes\":%u,\"quiet_trace_bytes\":%u,\"error_cases\":%u,\"request_data_status_mmio_dma_equal\":true,\"disabled_exactly_matches_baseline\":true,\"retained_diagnostics_match\":true}\n",cases,normal_old,normal_new,error_cases);
 return 0;
}
'''
(out/'main.c').write_text(main)
core=ROOT/'drivers/storage/mt8167-msdc'
exe=out/'handlers'
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-fsanitize=address,undefined','-I'+str(core),'-I'+str(out),str(out/'main.c')]+[str(out/(x+'.c')) for x in ('base','verbose','quiet')]+[str(core/(x+'.c')) for x in ('scsi','cdb','scsi_capacity','gpt_guard')]+['-o',str(exe)]
subprocess.run(cmd,check=True,env=env)
result=subprocess.check_output([str(exe)],text=True,env=env)
report=json.loads(result);report['baseline_commit']=BASE;report['mdl_allocations_for_4k_request']={'baseline':8,'candidate':1};report['harness_only_legacy_log_shift_cast']=True
report['baseline_miniport_sha256']=hashlib.sha256(old.encode()).hexdigest()
report['candidate_miniport_sha256']=hashlib.sha256(new.encode()).hexdigest()
# Hardware DMA is mocked; real DMA/STOP and reset/power behavior remain
# hardware validation requirements. No synthetic stall proves hardware safety.
(out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(result,end='')
