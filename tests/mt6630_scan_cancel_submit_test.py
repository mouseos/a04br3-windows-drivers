#!/usr/bin/env python3
"""Host-only actual submit function; mocked HIF, no firmware/driver build."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--output-dir', required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / 'drivers/network/mt6630/wmt_bridge.c').read_text()


def function(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define A04_WIFI_LIFE_IO 1
#define A04_WLAN_STATE 87
struct owner {unsigned active,poisoned,generation;};
struct life {unsigned state,generation;struct owner *owner;};
struct host {unsigned runtime;int failed;};
struct a04_wmt_bridge {struct life *life;unsigned wlan_ready;int failed;
 struct host wlan;struct {int failed;}transport;
 struct {unsigned pending,sequence,bss,complete;int failed;}scan;};
static unsigned calls;static int result;static uint8_t sent[4];
static int a04_wlan_set(struct host *h,unsigned cid,const uint8_t *p,unsigned n,void *response)
{assert(h&&cid==0x1b&&n==4&&!response);memcpy(sent,p,4);calls++;return result;}
''' + function('static int alive(') + '\n' + function('int a04_wmt_bridge_scan_cancel_submit(') + r'''
int main(void){
 struct owner owner={1,0,7};struct life life={1,7,&owner};
 struct a04_wmt_bridge b={.life=&life,.wlan_ready=1,.wlan={1,0},.scan={1,255,3,0,0}};
 struct a04_wmt_bridge before=b;
 assert(a04_wmt_bridge_scan_cancel_submit(&b,255,3)==0&&calls==1);
 assert(sent[0]==255&&!sent[1]&&!sent[2]&&!sent[3]);
 assert(!memcmp(&b,&before,sizeof(b))); /* no fabricated scan done / state retirement */
 result=-11;assert(a04_wmt_bridge_scan_cancel_submit(&b,255,3)==-11&&calls==2);
 assert(!memcmp(&b,&before,sizeof(b))); /* sender error, no success conversion */
 assert(a04_wmt_bridge_scan_cancel_submit(NULL,255,3)!=0);
 assert(a04_wmt_bridge_scan_cancel_submit(&b,254,3)!=0);
 assert(a04_wmt_bridge_scan_cancel_submit(&b,255,2)!=0);
 assert(a04_wmt_bridge_scan_cancel_submit(&b,511,3)!=0);
 assert(a04_wmt_bridge_scan_cancel_submit(&b,255,7)!=0);
 b.scan.pending=0;assert(a04_wmt_bridge_scan_cancel_submit(&b,255,3)!=0);b=before;
 b.scan.complete=1;assert(a04_wmt_bridge_scan_cancel_submit(&b,255,3)!=0);b=before;
 b.scan.failed=1;assert(a04_wmt_bridge_scan_cancel_submit(&b,255,3)!=0);b=before;
 b.wlan.runtime=0;assert(a04_wmt_bridge_scan_cancel_submit(&b,255,3)!=0);b=before;
 b.wlan.failed=1;assert(a04_wmt_bridge_scan_cancel_submit(&b,255,3)!=0);b=before;
 b.transport.failed=1;assert(a04_wmt_bridge_scan_cancel_submit(&b,255,3)!=0);b=before;
 owner.poisoned=1;assert(a04_wmt_bridge_scan_cancel_submit(&b,255,3)!=0);owner.poisoned=0;
 life.generation=8;assert(a04_wmt_bridge_scan_cancel_submit(&b,255,3)!=0);
 assert(calls==2);return 0;
}
'''
args.output_dir.mkdir(parents=True, exist_ok=True)
c_file = args.output_dir / 'scan-cancel-submit-host.c'
binary = args.output_dir / 'scan-cancel-submit-host'
c_file.write_text(harness)
subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(c_file), '-o', str(binary)], check=True)
subprocess.run([str(binary.resolve())], check=True)
print('PASS actual scan cancel submit: CID/body/identity/admission/error/no-completion; mocked HIF only')
