#!/usr/bin/env python3
"""Actual TQ finish function: cache ownership only after task/SLC/GP proof."""
import argparse, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[2]
s=(r/'vendor_m1_11/render_owner/windows_native_copy_transfer.c').read_text()
def function(name):
 start=s.index('NTSTATUS '+name+'(');brace=s.index('{',start);depth=1;i=brace+1
 while depth:
  depth += (s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
code=r'''#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef unsigned char UCHAR;typedef uint32_t ULONG;typedef uint64_t ULONGLONG;typedef int32_t NTSTATUS;typedef struct{int64_t QuadPart;}LARGE_INTEGER;
#define PASSIVE_LEVEL 0
#define TRUE 1
#define FALSE 0
#define KernelMode 0
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER -1
#define STATUS_INVALID_DEVICE_STATE -2
#define STATUS_IO_DEVICE_ERROR -3
#define STATUS_IO_TIMEOUT -4
#define STATUS_INTEGER_OVERFLOW -5
#define NT_SUCCESS(s) ((s)>=0)
#define RtlZeroMemory(p,n) memset(p,0,n)
#define RGXFWIF_KCCB_CMD_SLCFLUSHINVAL 1
#define RGXFWIF_KCCB_CMD_SYNC 2
struct a04_rgx_fw_runtime{void*command_sync;ULONG command_sync_addr;};
struct fw{struct a04_rgx_fw_runtime*runtime;int hardware_owned;};
struct a04_vendor_render_cold{struct fw*firmware;struct{void*cpu;}buffer[1];struct{ULONG control[4],sync;}layout;int poisoned;};
struct native{struct a04_vendor_render_cold*render;int quarantined;};
struct a04_native_copy_storage{int x;};
struct a04_native_tq_transfer{struct native*native;struct a04_native_copy_storage*storage;int state;ULONG kernel_next;ULONGLONG claimed_serial,irq_serial;struct{ULONG write_offset,completion_value;}queue;};
struct a04_vendor_start{struct fw*memory;int online,command_busy,command_poisoned,shutdown_started;ULONG command_sequence;};
struct a04_native_copy_gpu_result{ULONG phase,task_sequence,flush_sequence,client_write;};
typedef struct{ULONG ui32WrapMask,ui32DepOffset,ui32ReadOffset,ui32WriteOffset;}RGXFWIF_CCCB_CTL;
typedef struct{int eCmdType,eDM;union{struct{int bInval;}sSLCFlushInvalData;struct{ULONG sSyncObjDevVAddr,uiUpdateVal;}sSyncData;}uCmdData;}RGXFWIF_KCCB_CMD;
static unsigned irql,cachecalls,issuecalls,waitfail,readfail,issuefail,cachefail,badtask,acksync=1;static ULONGLONG ticks;
static unsigned KeGetCurrentIrql(void){return irql;}
static int InterlockedCompareExchange(int*p,int v,int expected){int x=*p;if(x==expected)*p=v;return x;}
static int InterlockedExchange(int*p,int v){int x=*p;*p=v;return x;}
static ULONGLONG KeQueryInterruptTime(void){return ticks+=10000000u;}
static int KeDelayExecutionThread(int x,int y,LARGE_INTEGER*z){(void)x;(void)y;(void)z;return 0;}
static void a04_gpu_dma_barrier(void){}
static NTSTATUS wait_queue(struct a04_rgx_fw_runtime*f,ULONG n){(void)f;(void)n;return waitfail?-3:0;}
static int a04_rgx_device_read_words(void*d,void*s,ULONG n){if(readfail)return 1;memcpy(d,s,n);if(badtask&&n==4)*(ULONG*)d=0;return 0;}
static NTSTATUS issue(struct a04_vendor_start*s,struct a04_rgx_fw_runtime*f,RGXFWIF_KCCB_CMD*c){issuecalls++;if(issuefail==issuecalls)return -3;if(acksync&&c->eCmdType==2)*(ULONG*)f->command_sync=s->command_sequence;return 0;}
static NTSTATUS a04_native_copy_storage_cpu_complete(struct a04_native_copy_storage*c){(void)c;assert(issuecalls==2);cachecalls++;return cachefail?-3:0;}
static NTSTATUS poison(struct a04_native_tq_transfer*c,struct a04_vendor_start*s,NTSTATUS e){c->state=5;c->native->quarantined=1;c->native->render->poisoned=1;s->command_poisoned=1;return e;}
''' + function('a04_native_tq_transfer_finish_gpu') + r'''
int main(void){unsigned cases=0;for(unsigned mode=0;mode<10;mode++){
 unsigned char bank[128]={0};ULONG gp=0;struct a04_rgx_fw_runtime f={&gp,1};struct fw fw={&f,1};
 struct a04_vendor_render_cold r={0};r.firmware=&fw;r.buffer[0].cpu=bank;r.layout.sync=64;
 struct native n={&r,0};struct a04_native_copy_storage st={0};struct a04_native_tq_transfer c={0};
 c.native=&n;c.storage=&st;c.state=3;c.claimed_serial=2;c.irq_serial=1;c.queue.write_offset=8;c.queue.completion_value=9;
 struct a04_vendor_start s={&fw,1,1,0,0,0};struct a04_native_copy_gpu_result out;
 RGXFWIF_CCCB_CTL ctl={4095,0,8,8};memcpy(bank,&ctl,sizeof(ctl));memcpy(bank+64,&c.queue.completion_value,4);
 irql=cachecalls=issuecalls=waitfail=readfail=issuefail=cachefail=badtask=0;acksync=1;ticks=0;
 switch(mode){case 1:waitfail=1;break;case 2:readfail=1;break;case 3:badtask=1;break;case 4:issuefail=1;break;case 5:issuefail=2;break;case 6:acksync=0;break;case 7:cachefail=1;break;case 8:irql=2;break;case 9:c.state=2;break;}
 NTSTATUS status=a04_native_tq_transfer_finish_gpu(&c,&s,&out);
 assert(s.command_busy==1); /* ownership return never notifies/releases submit gate */
 if(mode==0){assert(status==0&&out.phase==4&&cachecalls==1&&c.state==4&&!n.quarantined);}
 else {assert(status<0);assert(cachecalls==(mode==7));if(mode<8)assert(c.state==5&&n.quarantined&&s.command_poisoned);}
 cases++;
 }printf("copy ownership completion boundary: %u cases passed; actual TQ function, mocked hardware; device unverified\n",cases);return 0;}
'''
source=a.out/'actual-copy-finish.c';source.write_text(code)
cmd=['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(source),'-o',str(a.out/'test')]
x=subprocess.run(cmd,capture_output=True,text=True);(a.out/'compile.log').write_text(x.stdout+x.stderr);x.check_returncode()
x=subprocess.run([str((a.out/'test').resolve())],capture_output=True,text=True);(a.out/'test.log').write_text(x.stdout+x.stderr);print(x.stdout+x.stderr,end='');x.check_returncode()
