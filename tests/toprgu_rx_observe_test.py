#!/usr/bin/env python3
"""One host batch: actual poll/parser/formatter, fake MMIO, macro-OFF identity."""
import argparse,hashlib,json,os,re,resource,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
repo=Path(__file__).resolve().parents[1];out=a.output.resolve()
if not out.is_relative_to(repo.parent.parent.resolve()) or out.exists():raise ValueError('New project-root output required')
out.mkdir(parents=True);driver=repo/'drivers/system/mt8167-toprgu';src=(driver/'wdf_toprgu.c').read_text()
resource.setrlimit(resource.RLIMIT_CORE,(0,0));env={**os.environ,'TMPDIR':str(out)}

def fn(name,occurrence=0):
 starts=list(re.finditer(r'^(?:static )?(?:VOID|void|NTSTATUS) '+name+r'\(',src,re.M));start=starts[occurrence].start();b=src.index('{',start);level=1;i=b+1
 while level:
  if src[i]=='{':level+=1
  if src[i]=='}':level-=1
  i+=1
 return src[start:i]+'\n'

prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "reset_command.h"
#include "handoff_policy.h"
#include "watchdog_policy.h"
#if A04_TOPRGU_RX_OBSERVE
#include "rx_observe.h"
#endif
#define A04_TOPRGU_THERMAL_STOP 0
#define A04_TOPRGU_DEADLINE_SECONDS 0
#define VOID void
#define TRUE 1
#define FALSE 0
#define STATUS_INVALID_PARAMETER (-1)
#define STATUS_DEVICE_BUSY (-2)
#define STATUS_DEVICE_NOT_READY (-3)
#define STATUS_SUCCESS 0
#define NT_SUCCESS(s) ((s)>=0)
#define A04_UART_SCREEN_SEND 1
#define UNREFERENCED_PARAMETER(v) (void)(v)
#define WDF_REL_TIMEOUT_IN_MS(v) (v)
#define RtlCopyMemory memcpy
typedef uint32_t ULONG;typedef uint64_t ULONGLONG;typedef uint8_t UCHAR;typedef int BOOLEAN;typedef int32_t LONG,NTSTATUS;
typedef void *PVOID;typedef void *WDFTIMER;typedef void *WDFQUEUE;typedef void *WDFREQUEST;
typedef union {int64_t QuadPart;} LARGE_INTEGER;
typedef struct {
 volatile ULONG *wdt,*uart;LONG running,busy,backupStarted,ownedFour;int backupDpc,backupTimer,suspendedWatchdog,deadlineReported;
 ULONG savedMode,ticks,backupCalls;ULONGLONG started;struct mt8167_reset_parser parser;
 ULONG screenLen,screenPos;UCHAR screenBuf[96];void *screenTimer;
#if A04_TOPRGU_RX_OBSERVE
 struct a04_rx_observe rx;
#endif
} TOPRGU_CONTEXT;
static ULONG wdt[16],uart[16];static TOPRGU_CONTEXT c;
static uint64_t now=UINT64_C(10000000);static unsigned active=4,cpu=1,affinity=1;
static unsigned nrbr,nlsr,nlcr,resets,ntx;static unsigned char input[256];static unsigned input_len,input_pos;static uint32_t oe,thre=32;
static unsigned reglog[1024],regcount;static char tx[10000];
static LONG InterlockedCompareExchange(volatile LONG *p,LONG n,LONG old){LONG v=*p;if(v==old)*p=n;return v;}
static LONG InterlockedExchange(volatile LONG *p,LONG n){LONG v=*p;*p=n;return v;}
static unsigned KeQueryActiveProcessorCount(void *p){(void)p;return active;}
static unsigned KeGetCurrentProcessorNumber(void){return cpu;}
static unsigned ToprguMpidrAffinity(void){return affinity;}
static uint64_t KeQueryInterruptTime(void){return now;}
static void KeSetTargetProcessorDpc(int *p,int n){(void)p;(void)n;}
static void KeSetTimerEx(int *p,LARGE_INTEGER d,int n,int *q){(void)p;(void)d;(void)n;(void)q;}
static ULONG readreg(volatile ULONG *p){
 if(p>=uart&&p<uart+16){unsigned off=(unsigned)(p-uart);assert(regcount<1024);reglog[regcount++]=off;
  if(off==3){nlcr++;return uart[3];}
  if(off==5){nlsr++;uint32_t r=thre|oe|(input_pos<input_len?1u:0u);oe=0;return r;}
  if(off==0){nrbr++;assert(input_pos<input_len);return input[input_pos++];}
  assert(0);
 }
 assert(p>=wdt&&p<wdt+16);return *p;
}
static void writereg(volatile ULONG *p,ULONG v){
 if(p>=uart&&p<uart+16){assert(p==uart);assert(ntx<sizeof(tx));tx[ntx++]=(char)v;return;}
 assert(p>=wdt&&p<wdt+16);if(p==wdt+5){assert(v==0x1209u);resets++;}*p=v;
}
#define READ_REGISTER_ULONG readreg
#define WRITE_REGISTER_ULONG writereg
static void setup(void){memset(&c,0,sizeof(c));memset(wdt,0,sizeof(wdt));memset(uart,0,sizeof(uart));c.wdt=wdt;c.uart=uart;c.running=1;c.ownedFour=1;c.backupStarted=1;c.ticks=200;wdt[0]=1;nrbr=nlsr=nlcr=resets=ntx=regcount=input_len=input_pos=oe=0;thre=32;now=10000000;active=4;
#if A04_TOPRGU_RX_OBSERVE
 a04_rx_d0(&c.rx);
#endif
}
#if A04_TOPRGU_UART_SCREEN
static void *WdfIoQueueGetDevice(void *p){return p;}
static TOPRGU_CONTEXT *ToprguGetContext(void *p){return p;}
struct request {const char *data;size_t length;int status;};
static int WdfRequestRetrieveInputBuffer(void *p,size_t n,void **b,size_t *len){struct request *r=p;if(n>r->length)return -1;*b=(void*)r->data;*len=r->length;return 0;}
static void WdfRequestComplete(void *p,int s){((struct request*)p)->status=s;}
static void WdfTimerStart(void *p,unsigned n){(void)p;(void)n;}
#endif
'''
main=r'''
#if A04_TOPRGU_RX_OBSERVE
static void sent_all(struct a04_rx_observe *s){while(s->length)a04_rx_sent(s);}
static void helper_tests(void){
 struct {uint32_t before;struct a04_rx_observe s;uint32_t after;} guard={0xdeadbeef,{0},0xaabbccdd};struct a04_rx_observe *s=&guard.s;
 a04_rx_d0(s);a04_rx_begin(s);a04_rx_lcr(s,3);a04_rx_lsr(s,32);a04_rx_queue(s,0,0,1);
 assert(s->length==205&&s->records==1&&s->idle_records==1&&!memcmp(s->line,"A04RX1 ",7)&&s->line[204]=='\n');sent_all(s);
 for(unsigned i=1;i<4;i++){a04_rx_queue(s,(uint64_t)i*A04_RX_HEARTBEAT_INTERVAL,0,1);assert(s->idle_records==i+1);sent_all(s);}
 a04_rx_queue(s,UINT64_C(360000000000),0,1);assert(!s->length&&s->records==4);
 /* Late input retains all twenty event records after idle output stops. */
 for(unsigned i=0;i<20;i++){
  a04_rx_consumed(s,0,0,0);uint64_t t=UINT64_C(370000000000)+(uint64_t)i*A04_RX_CHANGE_INTERVAL;
  a04_rx_queue(s,t,0,1);assert(s->event_records==i+1);sent_all(s);
  a04_rx_consumed(s,0,0,0);a04_rx_queue(s,t+1,0,1);assert(!s->length);
 }
 assert(s->records==24);a04_rx_d0(s);a04_rx_queue(s,UINT64_C(900000000000),1,1);assert(!s->length&&s->records==24);
 assert(guard.before==0xdeadbeef&&guard.after==0xaabbccdd);
 memset(s,0,sizeof(*s));s->d0=s->polls=s->lcr_reads=s->lsr_reads=s->dr=s->overrun=s->bytes=s->parser_calls=s->advance=s->mismatch=s->hits=s->deferred=s->dlab=s->lcr_changes=UINT32_MAX;
 a04_rx_d0(s);a04_rx_begin(s);a04_rx_lcr(s,3);a04_rx_lsr(s,35);a04_rx_consumed(s,0,1,0);a04_rx_consumed(s,0,0,0);a04_rx_consumed(s,8,0,1);a04_rx_queue(s,0,0,0);
 assert(s->d0==UINT32_MAX&&s->polls==UINT32_MAX&&s->bytes==UINT32_MAX&&s->advance==UINT32_MAX&&s->mismatch==UINT32_MAX&&s->hits==UINT32_MAX&&s->deferred==UINT32_MAX&&(s->saturated&4095u)==4095u);
 assert(a04_rx_can_tx(s));a04_rx_lcr(s,128);assert(!s->lsr_valid&&!a04_rx_can_tx(s)&&s->dlab==UINT32_MAX&&s->lcr_changes==UINT32_MAX&&s->saturated==16383u);
}
#endif
int main(void){
 setup();ToprguPoll(&c,FALSE);assert(nrbr==0&&nlcr==1&&nlsr==1&&regcount==2&&reglog[0]==3&&reglog[1]==5);
#if A04_TOPRGU_RX_OBSERVE
 assert(c.rx.bytes==0&&c.rx.parser_calls==0&&c.rx.lsr_valid&&c.rx.lcr==0&&c.rx.lsr==32&&ntx==1);
 /* Trace must not create hidden clear-on-read accesses during an RX line. */
 unsigned n=regcount;Trace(&c,"forbidden");assert(regcount==n&&ntx==1);
#endif
 setup();uart[3]=128;ToprguPoll(&c,FALSE);assert(nlcr==1&&nlsr==0&&nrbr==0);
#if A04_TOPRGU_RX_OBSERVE
 assert(!c.rx.lsr_valid&&c.rx.lcr==128&&!ntx);
#endif
 setup();oe=2;thre=0;input[0]=0xe7;input_len=1;ToprguPoll(&c,FALSE);assert(nrbr==1&&nlsr==2&&nlcr==2&&regcount==5&&!resets);
#if A04_TOPRGU_RX_OBSERVE
 assert(c.rx.bytes==1&&c.rx.parser_calls==1&&c.rx.mismatch==1&&c.rx.overrun==1&&!ntx);
#endif
 setup();const char *bad="noiseA04BR3?A04BR3!RST";input_len=(unsigned)strlen(bad);memcpy(input,bad,input_len);
 while(input_pos<input_len){regcount=0;ToprguPoll(&c,FALSE);now+=200000;}
 assert(resets==1&&c.parser.matched==0&&nrbr==input_len);
#if A04_TOPRGU_RX_OBSERVE
 assert(c.rx.bytes==input_len&&c.rx.parser_calls==input_len&&c.rx.hits==1&&c.rx.mismatch>0&&c.rx.advance>0);
#endif
 setup();memset(input,'x',sizeof(input));input_len=40;ToprguPoll(&c,FALSE);assert(nrbr==16&&nlsr==16&&nlcr==16);
#if A04_TOPRGU_RX_OBSERVE
 assert(c.rx.bytes==16&&c.rx.parser_calls==16);
#endif
#if A04_TOPRGU_UART_SCREEN
 setup();c.screenLen=5;ToprguPoll(&c,FALSE);assert(!ntx&&nrbr==0&&nlsr==1);unsigned nscreen=regcount;Trace(&c,"forbidden");assert(regcount==nscreen);
#if A04_TOPRGU_RX_OBSERVE
 assert(!c.rx.length&&c.rx.deferred==1);c.screenLen=0;regcount=0;ToprguPoll(&c,FALSE);assert(c.rx.length&&ntx==1);
 struct request req={"PB TEST\n",8,0};ToprguScreenIoctl(&c,&req,0,8,A04_UART_SCREEN_SEND);assert(req.status==STATUS_DEVICE_BUSY&&!c.screenLen);
 while(c.rx.length){regcount=0;ToprguPoll(&c,FALSE);now+=200000;}
 ToprguScreenIoctl(&c,&req,0,8,A04_UART_SCREEN_SEND);assert(req.status==0&&c.screenLen==8);
#endif
#endif
#if A04_TOPRGU_RX_OBSERVE
 helper_tests();
#endif
 puts("TOPRGU_RX_OBSERVE_POLL_PARSER_BOUNDS_PASS");return 0;
}
'''
with (out/'host.log').open('w') as log:
 for handoff,screen in ((1,1),(0,0)):
  for enabled in (0,1):
   case=f'cpu{handoff}-screen{screen}-rx{enabled}'
   code=prefix+fn('Trace')+fn('TraceTimer')
   if enabled:code+=fn('ToprguRxObserve')
   code+=fn('ToprguPoll',0 if handoff else 1)
   if screen:code+=fn('ToprguScreenIoctl')
   code+=main
   path=out/(case+'.c');path.write_text(code);exe=out/case
   cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-fsanitize=address,undefined','-fno-omit-frame-pointer',f'-DA04_TOPRGU_RX_OBSERVE={enabled}',f'-DA04_TOPRGU_UART_SCREEN={screen}','-I'+str(driver),str(path),str(driver/'reset_command.c'),'-o',str(exe)]
   log.write(case+'\n');log.flush();subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True);subprocess.run([str(exe)],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 # Whole source macro-OFF token equality, not a hand-copied RX model.
 stubs=out/'preprocess-stubs';stubs.mkdir()
 for name in ('ntddk.h','wdf.h','wdmsec.h'):(stubs/name).write_text('/* Preprocess only: no WDK ABI claim. */\n')
 old=subprocess.check_output(['git','show','0821853f798407858a1e2995b8f4e7d9d13c3e36:drivers/system/mt8167-toprgu/wdf_toprgu.c'],cwd=repo)
 before=out/'baseline-wdf_toprgu.c';before.write_bytes(old)
 def tokens(s):return re.findall(r'"(?:\\.|[^"\\])*"|\w+|[^\s]',s)
 comparisons=[]
 for handoff,screen,thermal in ((0,0,0),(1,0,0),(1,1,0),(0,0,1),(1,0,1)):
  flags=[f'-DA04_TOPRGU_CPU1_HANDOFF={handoff}',f'-DA04_TOPRGU_UART_SCREEN={screen}',f'-DA04_TOPRGU_THERMAL_STOP={thermal}','-DA04_TOPRGU_RX_OBSERVE=0','-DA04_TOPRGU_DEADLINE_SECONDS=0']
  cmd=['cc','-E','-P','-I'+str(stubs),'-I'+str(driver),*flags]
  b=subprocess.check_output(cmd+[str(before)],env=env,text=True);n=subprocess.check_output(cmd+[str(driver/'wdf_toprgu.c')],env=env,text=True)
  assert tokens(b)==tokens(n),'macro OFF differs from original source'
  comparisons.append(dict(handoff=handoff,screen=screen,thermal=thermal,identical=True))
(out/'result.json').write_text(json.dumps(dict(actual_poll_and_parser_executed=True,actual_formatter_executed=True,ASan_UBSan=True,variants=4,macro_off_whole_source_token_comparisons=comparisons,rx_no_extra_LSR_LCR_RBR_reads=True,counts_saturate=True,parser_progress_mismatch_match_checked=True,one_byte_per_poll_from_existing_THRE=True,screen_pending_no_trace=True,observation_pending_screen_BUSY=True,max_records=24,idle_records_max=4,reserved_state_records=20,bytes_per_record=205,max_observation_UART_bytes=4920,minimum_record_spacing_seconds=10,idle_spacing_seconds=60,budget_survives_D0=True,private_context_only=True,source_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (driver/'wdf_toprgu.c',driver/'rx_observe.h',driver/'reset_command.c')},Windows_WDK_compiled=False,device_verified=False),indent=2)+'\n')
print('TOPRGU_RX_OBSERVE_HOST_BATCH_PASS')
