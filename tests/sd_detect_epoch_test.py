#!/usr/bin/env python3
"""Exercise actual CD state/live/ensure functions with deterministic interleaving.
No hardware access; OS atomic primitives and card initialization are mocked.
"""
import argparse, json, os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--out',required=True);out=Path(p.parse_args().out).resolve()
assert root.parent.parent in out.parents
out.mkdir(parents=True,exist_ok=True)
rel='drivers/storage/mt8167-sd/windows.c'
old=subprocess.check_output(['git','-C',str(root),'show','9e52cc1c:'+rel],text=True)
new=(root/rel).read_text();scsi=(root/'drivers/storage/mt8167-sd/scsi.c').read_text()
def take(s,a,b):return s[s.index(a):s.index(b,s.index(a))]
common=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "sd.h"
typedef int32_t LONG;
struct irp {int Cancel;};
struct sd_host {void *registers; int quiescing; volatile LONG closing,child_online,present,generation,attention,ejected,blocked,absent_seen;
 LONG run_generation,card_generation;struct irp *active_irp;struct a04_sd_card card;struct a04_sd_ops ops;};
static LONG InterlockedExchange(volatile LONG*p,LONG v){return __atomic_exchange_n(p,v,__ATOMIC_SEQ_CST);}
static LONG InterlockedIncrement(volatile LONG*p){return __atomic_add_fetch(p,1,__ATOMIC_SEQ_CST);}
static LONG InterlockedCompareExchange(volatile LONG*p,LONG v,LONG expected){__atomic_compare_exchange_n(p,&expected,v,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);return expected;}
#define RtlSecureZeroMemory(p,n) memset(p,0,n)
static unsigned card_commands;
enum a04_sd_result a04_sd_initialize(const struct a04_sd_ops*o,struct a04_sd_card*c,uint64_t deadline){(void)o;(void)deadline;card_commands++;c->ready=1;return SD_OK;}
'''
body=common+take(new,'static int live(','\nNTSTATUS SdAcpi')+take(old,'static void set_presence(','\nVOID SdTimer').replace('set_presence','baseline_presence')+take(new,'static void set_presence(','\nVOID SdTimer')+take(scsi,'static enum a04_sd_result ensure(','\nNTSTATUS SdProcess')+r'''
static struct sd_host present_host(void){struct sd_host h={0};h.registers=&card_commands;h.child_online=1;h.present=1;h.generation=h.run_generation=h.card_generation=7;h.card.ready=1;return h;}
int main(void){
 struct sd_host base=present_host(),h=present_host();
 /* Both callbacks occur between worker live() checks: baseline reuses old card. */
 assert(live(&base));baseline_presence(&base,-1);assert(!live(&base));baseline_presence(&base,1);
 assert(live(&base)&&base.generation==7&&ensure(&base,100)==SD_OK&&card_commands==0);
 set_presence(&h,-1);assert(!live(&h)&&h.generation==8&&h.blocked&&h.attention&&!h.absent_seen);
 set_presence(&h,1);assert(!live(&h)&&h.generation==8&&h.blocked&&!h.absent_seen);
 h.run_generation=h.generation;assert(ensure(&h,100)==SD_POISONED&&card_commands==0);
 /* No repeated invalidation or quarantine clearing on unchanged/recovered reads. */
 set_presence(&h,-1);LONG generation=h.generation;set_presence(&h,-1);assert(h.generation==generation&&h.blocked);
 set_presence(&h,1);assert(h.blocked&&ensure(&h,100)==SD_POISONED&&card_commands==0);
 /* An actual observed empty socket then insertion uses fresh card init once. */
 set_presence(&h,0);assert(h.absent_seen&&!h.blocked&&!live(&h));
 set_presence(&h,1);h.run_generation=h.generation;assert(!h.absent_seen);
 assert(ensure(&h,100)==SD_OK&&card_commands==1&&h.card_generation==h.generation);
 assert(ensure(&h,100)==SD_OK&&card_commands==1);
 /* Unknown never cancels logical eject or fabricates absence. */
 h.ejected=1;set_presence(&h,-1);set_presence(&h,1);assert(h.ejected&&h.blocked&&!h.absent_seen);
 assert(ensure(&h,100)==SD_REMOVED&&card_commands==1);
 /* Empty socket and read failures do not initialize/reset a card. */
 h=present_host();set_presence(&h,0);generation=h.generation;
 set_presence(&h,-1);assert(!h.blocked&&h.absent_seen);assert(ensure(&h,100)==SD_REMOVED);
 set_presence(&h,0);assert(h.present==0&&h.absent_seen&&!h.blocked);
 assert(ensure(&h,100)==SD_REMOVED&&card_commands==1);
 /* Ordinary removal/insertion still invalidates all previous work. */
 set_presence(&h,1);assert(h.generation>generation&&h.run_generation!=h.generation);
 puts("{\"baseline_stale_request_reproduced\":true,\"candidate_epoch_invalidated\":true,\"quarantine_preserved\":true,\"no_card_io_on_unknown_or_empty\":true,\"fresh_init_only_after_observed_absence\":true}");
}
'''
(out/'detect.c').write_text(body)
env=dict(os.environ,TMPDIR=str(out))
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'drivers/storage/mt8167-sd'),str(out/'detect.c'),'-o',str(out/'detect')],env=env,check=True)
result=subprocess.check_output([str(out/'detect')],env=env,text=True)
(out/'result.json').write_text(json.dumps(json.loads(result),indent=2)+'\n');print(result,end='')
