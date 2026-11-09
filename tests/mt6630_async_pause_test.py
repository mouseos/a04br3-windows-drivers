#!/usr/bin/env python3
"""実production Pause/claim/API退場関数の限定境界検証。NDIS実行・電源成功の代替ではない。"""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
source=(root/'drivers/network/mt6630/ndis_runtime.c').read_text()
def function(signature):
    start=source.index(signature)
    opening=source.index('{',start)
    depth=1;end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
body='\n'.join(function(x) for x in ['static int pause_complete_locked(', 'static void api_leave(', 'NDIS_STATUS RuntimePause(NDIS_HANDLE p,PNDIS_MINIPORT_PAUSE_PARAMETERS params)\n{'])
assert 'NdisWaitEvent' not in body
worker=source[source.index('static void worker(PVOID p,NDIS_HANDLE work)\n{'):source.index('static void poll_dpc')]
assert worker.index('NdisFreeIoWorkItem(work);a->busy=0')<worker.index('pause_complete_locked(a)')<worker.index('if(complete)NdisMPauseComplete(adapter);')
assert worker.rfind('NdisReleaseSpinLock(&a->lock);')<worker.index('if(complete)NdisMPauseComplete(adapter);')
assert 'if(a->closing)status=NDIS_STATUS_REQUEST_ABORTED;else if(a->paused)status=NDIS_STATUS_PAUSED;' in source
fixture=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef void *NDIS_HANDLE;typedef int NDIS_STATUS;typedef void *PNDIS_MINIPORT_PAUSE_PARAMETERS;
#define NDIS_STATUS_PENDING 1
#define NDIS_STATUS_SUCCESS 0
#define MT6630_NDIS_PAUSED 1
#define MT6630_NDIS_PAUSING 2
#define UNREFERENCED_PARAMETER(x) (void)(x)
struct a04_ndis_runtime {int lock,paused,busy;unsigned api_refs;void *send;struct {unsigned count;}send_queue;struct {void *pending;}tx;int idle;NDIS_HANDLE adapter;};
static unsigned completions;
static void NdisAcquireSpinLock(int *v){assert(!*v);*v=1;}
static void NdisReleaseSpinLock(int *v){assert(*v);*v=0;}
static void NdisSetEvent(int *v){*v=1;}
static void NdisMPauseComplete(NDIS_HANDLE p){struct a04_ndis_runtime *a=p;assert(!a->lock&&a->paused==1&&!a->busy&&!a->api_refs&&!a->send&&!a->send_queue.count&&!a->tx.pending);++completions;}
'''+body+r'''
static void worker_drained(struct a04_ndis_runtime *a){int complete;NdisAcquireSpinLock(&a->lock);a->busy=0;complete=pause_complete_locked(a);NdisReleaseSpinLock(&a->lock);if(complete)NdisMPauseComplete(a);}
int main(void){
 struct a04_ndis_runtime a={0};a.adapter=&a;
 assert(RuntimePause(&a,0)==NDIS_STATUS_SUCCESS&&a.paused==1&&!completions);
 a.paused=0;a.busy=1;assert(RuntimePause(&a,0)==NDIS_STATUS_PENDING&&a.paused==2);
 a.api_refs=1;worker_drained(&a);assert(!completions&&a.paused==2);
 api_leave(&a);assert(completions==1&&a.paused==1);api_leave(&a);assert(completions==1);
 /* A new independent BT worker after drain does not create another owed
  * Pause completion. Paused remains truthy to the existing data guards. */
 a.busy=1;worker_drained(&a);assert(completions==1&&a.paused);
 a.paused=0;a.busy=1;a.send=&a;a.send_queue.count=1;
 assert(RuntimePause(&a,0)==NDIS_STATUS_PENDING);worker_drained(&a);assert(completions==1);
 a.send=0;a.send_queue.count=0;a.api_refs=1;api_leave(&a);assert(completions==2&&a.paused==1);
 a.paused=0;a.tx.pending=&a;assert(RuntimePause(&a,0)==NDIS_STATUS_PENDING);
 worker_drained(&a);assert(completions==2&&a.paused==2);
 a.tx.pending=0;a.api_refs=1;api_leave(&a);assert(completions==3&&a.paused==1);
 puts("PASS actual Pause/claim/api_leave: no synchronous wait, drain-before-complete, exactly once outside lock; no NDIS/hardware execution");
}
'''
out=root/'build/mt6630-async-pause';out.mkdir(parents=True,exist_ok=True)
(out/'production-boundary.c').write_text(fixture)
subprocess.run(['clang','-std=c11','-O1','-fsanitize=address,undefined','-Wall','-Wextra','-Werror',str(out/'production-boundary.c'),'-o',str(out/'boundary-test')],check=True)
subprocess.run([str(out/'boundary-test')],check=True)
