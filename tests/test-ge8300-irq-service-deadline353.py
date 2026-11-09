from pathlib import Path
import subprocess,json
import argparse
p=argparse.ArgumentParser();p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args()
S=Path(__file__).resolve().parents[1];O=a.output_dir;O.mkdir(parents=True,exist_ok=True)
path='drivers/gpu/mt8167/wddm-render/kmd_clear.c'
new=(S/path).read_text();old=subprocess.check_output(['git','-C',str(S),'show','65c89c68c05255f59f0a9604a9c7993fa3e77c3e:'+path],text=True)
prefix=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint64_t ULONGLONG;typedef uint32_t UINT,ULONG;typedef int NTSTATUS;typedef struct {long long QuadPart;} LARGE_INTEGER;
#define MAXULONG UINT32_MAX
#define STATUS_SUCCESS 0
#define STATUS_DEVICE_BUSY -1
#define STATUS_INVALID_DEVICE_STATE -2
#define STATUS_IO_TIMEOUT -3
#define STATUS_DEVICE_NOT_READY -4
#define GEO_SUBMIT_WAIT_IRQ 259
#define GEO_DRAW_MAGIC 7
struct prepare_call {int status,trace_submit_eligible;uint64_t token;};
struct ticket {unsigned slot,generation;};
static uint64_t now,cost;static int services,syncs,mode,syncfail,records,negative;
static uint64_t KeQueryInterruptTime(void){return now;}
static int KeDelayExecutionThread(int a,int b,LARGE_INTEGER*c){now+=10000;return 0;}
#define KernelMode 0
#define FALSE 0
static void InterlockedAnd(long*p,long n){*p&=n;}
static void GeoFactoryCheckpoint(int a,int b,int c,int d,int e,int f){records++;now+=500000;}
static int finish_prepare;
static int sync_call(void*ctx,int f,struct prepare_call*c){syncs++;assert(c->token==123);if(syncfail&&syncs==2)return 0;if(c->status<0){negative++;return 1;}c->status=(services && mode==0)?0:GEO_SUBMIT_WAIT_IRQ;return 1;}
static int service_call(void*p,uint64_t epoch){services++;now+=cost;return mode==2?-9:mode==3?STATUS_DEVICE_BUSY:0;}
struct session {struct {int(*service)(void*,uint64_t);}control;struct {void*cookie;uint64_t epoch;}lease;};
struct adapter {int(*synchronize)(void*,int,struct prepare_call*);void*sync_context;long service_pending;struct session*session;};
'''
def extract(s,name):
 body=s[s.index(' began=KeQueryInterruptTime();'):s.index('\nstruct claim_prepare_call')]
 return 'static int '+name+'(struct adapter*a){struct prepare_call call={0,0,123};struct ticket ticket={1,3},*t=&ticket;UINT kind=1,attempt=0;ULONGLONG began,service_started,service_elapsed;LARGE_INTEGER interval;NTSTATUS service=0;UINT trace_waited=0,trace_attempt=0,trace_service_ms=0,trace_sync_failed=0;\n'+body
main=r'''
static int run(int old,int m,uint64_t us,int fail){struct session s={{service_call},{0,1}};struct adapter a={sync_call,0,1,&s};now=0;cost=us*10;mode=m;syncfail=fail;services=syncs=records=negative=0;return old?baseline(&a):candidate(&a);}
int main(void){
 assert(run(1,0,110000,0)==STATUS_IO_TIMEOUT);assert(services==1&&negative==1);
 assert(run(0,0,110000,0)==0);assert(services==1&&syncs==2&&negative==0);
 assert(run(0,1,110000,0)==STATUS_IO_TIMEOUT);assert(services==1&&negative==1);
 assert(run(0,1,0,0)==STATUS_IO_TIMEOUT);assert(services==100&&negative==1);
 assert(run(0,2,0,0)==-9);assert(services==1&&negative==1);
 assert(run(0,3,1000,0)==STATUS_IO_TIMEOUT);assert(services<=100&&negative==1);
 assert(run(0,0,0,1)==STATUS_DEVICE_NOT_READY);assert(services==1);
 puts("production retry body: baseline reproduced; same-token resubmit, renewed IRQ, elapsed/count, busy, error, synchronization failure passed");return 0;}
'''
loop=new[new.index(' for(;;)',new.index('static NTSTATUS prepare_pending')):new.index('record_result:',new.index('static NTSTATUS prepare_pending'))]
assert 'GeoFactoryCheckpoint(' not in loop
c=O/'retry353.c';c.write_text(prefix+extract(old,'baseline')+extract(new,'candidate')+main)
subprocess.run(['clang','-fsanitize=address,undefined','-g','-Wno-unused-value',str(c),'-o',str(O/'retry353')],check=True)
r=subprocess.run([str(O/'retry353')],capture_output=True,text=True,check=True);print(r.stdout,end='');(O/'host-regression.json').write_text(json.dumps({'production_body_extracted':True,'baseline_timeout_reproduced':True,'cases':7,'asan_ubsan_exit':r.returncode,'hardware_verified':False,'stdout':r.stdout},indent=2)+'\n')
