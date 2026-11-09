#!/usr/bin/env python3
"""Run the production ISR with explicit DIRQL mocks; not hardware proof."""
import argparse,subprocess,json,hashlib,os
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=True);r=Path(__file__).resolve().parents[1];s=(r/'drivers/gpu/mt8167/display-transport/transport356.c').read_text();i=s.index('static BOOLEAN irq(');b=s.index('{',i);at=b+1;depth=1
while depth:depth+=(s[at]=='{')-(s[at]=='}');at+=1
actual=s[i:at]
pre=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
typedef void *PKINTERRUPT;typedef unsigned ULONG;typedef int LONG;typedef uint64_t ULONGLONG;typedef int BOOLEAN;
#define TRUE 1
#define FALSE 0
#define OVL_INTSTA 8
#define OVL_INTEN 4
struct display356 {int started,closing;volatile ULONG *ovl,*rdma;PKINTERRUPT ovl_interrupt;volatile LONG frame_errors,in_blank,seen_vsync;ULONGLONG ovl_serial,frame_serial;void(*notify)(void *,ULONGLONG);void *notify_context;};
static unsigned dirql=5,event_calls,notified;static volatile ULONG ovl[1024],rdma[1024];
static ULONG rd(struct display356 *d,ULONG n){return d->ovl[n/4];}
static void wr(struct display356 *d,ULONG n,ULONG v){assert(dirql==5);d->ovl[n/4]&=v;}
#define READ_REGISTER_ULONG(p) (*(p))
#define WRITE_REGISTER_ULONG(p,v) ((*(p))&=(v))
static LONG InterlockedOr(volatile LONG *p,LONG v){LONG old=*p;*p|=v;return old;}
static LONG InterlockedExchange(volatile LONG *p,LONG v){LONG old=*p;*p=v;return old;}
void KeSetEvent(void *event,int increment,int wait){(void)event;(void)increment;(void)wait;event_calls++;assert(dirql<=2);}
static void notified_frame(void *p,ULONGLONG serial){assert(dirql==5&&p==(void*)3&&serial==1);notified++;}
'''
main=r'''
int main(void){struct display356 d={0};d.started=1;d.ovl=ovl;d.rdma=rdma;d.ovl_interrupt=(void*)1;d.notify=notified_frame;d.notify_context=(void*)3;
 assert(!irq((void*)2,&d));ovl[1]=2;ovl[2]=2;assert(irq((void*)1,&d));assert(d.ovl_serial==1&&!d.frame_serial&&!notified&&!ovl[2]);
 rdma[0]=6;rdma[1]=2;assert(irq((void*)2,&d));assert(!d.in_blank&&!d.frame_serial&&!notified&&!rdma[1]);
 rdma[1]=4;assert(irq((void*)2,&d));assert(d.in_blank&&d.seen_vsync&&d.frame_serial==1&&notified==1&&!rdma[1]);
 assert(!event_calls&&!d.frame_errors);d.closing=1;rdma[1]=4;assert(!irq((void*)2,&d)&&d.frame_serial==1);return 0;}
'''
f=o/'actual-isr.c';f.write_text(pre+actual+main);env=dict(os.environ,TMPDIR=str(o));args=['clang','-std=c11','-O1','-g','-fsanitize=address,undefined',str(f),'-o',str(o/'actual-isr')];q=subprocess.run(args,capture_output=True,text=True,env=env);(o/'compile.log').write_text(q.stdout+q.stderr);q.check_returncode();q=subprocess.run([str(o/'actual-isr')],capture_output=True,text=True,env=env);(o/'test.log').write_text(q.stdout+q.stderr);q.check_returncode();(o/'receipt.json').write_text(json.dumps(dict(production_ISR_sha256=hashlib.sha256(actual.encode()).hexdigest(),source_sha256=hashlib.sha256(s.encode()).hexdigest(),DIRQL_KeSetEvent_calls=0,real_RDMA_done_only_notifies=True,OVL_not_vsync=True,W0C_ack_verified=True,ASAN_UBSAN=True,hardware_verified=False),indent=2)+'\n');print('PRODUCTION356R2_IRQ_DIRQL_CONTRACT_PASS_HARDWARE_MOCKED')
