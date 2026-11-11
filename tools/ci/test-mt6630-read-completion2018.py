#!/usr/bin/env python3
"""Execute production deliver with inline BthMini-style reentry, ASAN/UBSAN.
Queue/Win32 calls are stubs; no hardware, HCI command or radio success claim.
"""
import argparse, subprocess, hashlib, json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
src=r/'drivers/bluetooth/mt6630/combo_client.c';s=src.read_text();start=s.index('static void deliver(CLIENT *c)');end=s.index('\nstruct RX_DRAIN',start);actual=s[start:end]
pre=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
typedef int NTSTATUS;typedef void *PVOID;typedef unsigned char UCHAR;typedef uintptr_t ULONG_PTR;
#define NT_SUCCESS(x) ((x)>=0)
#pragma pack(push,1)
typedef struct {uint32_t DataLen;UCHAR Type,Data[32];} H,*PBTHX_HCI_READ_WRITE_CONTEXT;
#pragma pack(pop)
#define CONTEXT_BYTES offsetof(H,Data)
#define RtlCopyMemory memcpy
#define RtlSecureZeroMemory(p,n) memset(p,0,n)
struct a04_bt_broker_packet {unsigned type,length;UCHAR data[32];};
struct Q;struct Request{H buffer;unsigned capacity,done;NTSTATUS status;ULONG_PTR used;};typedef struct Request *WDFREQUEST;
typedef struct {struct Q*reads[2];unsigned count;struct a04_bt_broker_packet packets[32];int held;} CLIENT;
struct Q{WDFREQUEST request;};
static CLIENT *owner;static struct Request retry;static unsigned callbacks,mode;static unsigned seen[8];
static void lock(CLIENT*c){assert(!c->held);c->held=1;}
static void unlock(CLIENT*c){assert(c->held);c->held=0;}
static NTSTATUS WdfIoQueueRetrieveNextRequest(struct Q*q,WDFREQUEST*r){assert(owner->held);if(!q->request)return -1;*r=q->request;q->request=NULL;return 0;}
static NTSTATUS WdfRequestRetrieveOutputBuffer(WDFREQUEST r,size_t n,PVOID*out,size_t*bytes){assert(owner->held);if(r->capacity<n)return -2;*out=&r->buffer;*bytes=r->capacity;return 0;}
static void deliver(CLIENT*);
static void WdfRequestCompleteWithInformation(WDFREQUEST r,NTSTATUS status,ULONG_PTR used){
 assert(!owner->held);assert(!r->done);r->done=1;r->status=status;r->used=used;assert(callbacks<8);
 if(!status){assert(used==CONTEXT_BYTES+r->buffer.DataLen);seen[callbacks]=r->buffer.Data[0];}else assert(!used);
 callbacks++;
 /* Asynchronous BthMini completion immediately resubmits READ on the same
  * call stack. Taking the same nonrecursive lock exposes the old deadlock. */
 if(callbacks==1){lock(owner);assert(owner->count==(mode==1?2u:1u));retry.capacity=sizeof(H);owner->reads[0]->request=&retry;deliver(owner);unlock(owner);}
}
'''
post=r'''
int main(void){
 for(mode=1;mode<=2;mode++){
  CLIENT c={0};struct Q event={0},acl={0};struct Request first={0},other={0};memset(&retry,0,sizeof(retry));callbacks=0;memset(seen,0,sizeof(seen));owner=&c;c.reads[0]=&event;c.reads[1]=&acl;
  c.count=mode==1?3:1;c.packets[0]=(struct a04_bt_broker_packet){4,1,{11}};
  if(mode==1){c.packets[1]=(struct a04_bt_broker_packet){2,1,{22}};c.packets[2]=(struct a04_bt_broker_packet){4,1,{33}};}
  first.capacity=mode==1?sizeof(H):0;other.capacity=sizeof(H);event.request=&first;acl.request=&other;
  lock(&c);deliver(&c);assert(c.held);unlock(&c);
  assert(c.count==0&&first.done&&retry.done);assert(!retry.status);
  if(mode==1){assert(callbacks==3&&other.done&&seen[0]==11&&seen[1]==22&&seen[2]==33);}
  else{assert(callbacks==2&&first.status==-2&&seen[1]==11&&!other.done);}
  /* Detached packet is neither delivered twice nor exposed to a later read. */
  for(unsigned i=0;i<32;i++)assert(!c.packets[i].length);
  lock(&c);deliver(&c);unlock(&c);assert(callbacks==(mode==1?3u:2u));
 }
 puts("actual deliver: inline READ reentry, packet detach, mixed queues, failed buffer retry and exactly-once completion PASS");
}
'''
f=o/'actual-deliver.c';f.write_text(pre+actual+post)
for cmd,n in [(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(f),'-o',str(o/'test')],'compile.log'),([str(o/'test')],'test.log')]:
 x=subprocess.run(cmd,capture_output=True,text=True);(o/n).write_text(x.stdout+x.stderr);print(x.stdout+x.stderr,end='');x.check_returncode()
(o/'receipt.json').write_text(json.dumps({'actual_source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'ASAN_UBSAN':True,'inline_reentry_pass':True,'hardware_verified':False,'scope':'queue/lock/request completion only; no transport or RF emulation'},indent=2)+'\n')
