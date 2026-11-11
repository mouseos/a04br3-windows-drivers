"""Explicit new-profile collection reboot contract and no-reset host boundary."""
import json,re,subprocess,time
from pathlib import Path
CONFIG=dict(schema='MT8167_COLLECTION_REBOOT_OPT_IN_V1',enabled=True,receipt_schema='MT8167_COLLECTION_REBOOT_PROOF_V1',fastboot_wait_seconds=180,maximum_shutdown_requests=1,require_both_clients_completed=True,preserve_diagnostic_result=True,uart_reset_allowed=False)
def require(v,msg):
 if not v:raise ValueError(msg)
def policy_config(p):
 c=p.get('collection_reboot')
 if c is None:return None
 require(json.dumps(c,sort_keys=True)==json.dumps(CONFIG,sort_keys=True) and p['profile'].endswith('-collection-reboot-v1'),'explicit new collection-reboot profile required')
 require(p['service']==p['data_directory'] and p['first_install_mask']==p['deferred_unbound_mask']==0,'no new install mode')
 require(len(p['diagnostic_clients'])==2 and [x['name'] for x in p['diagnostic_clients']]==['gpu_present','gpu_globaldiag'] and all(x['phase']==1 for x in p['diagnostic_clients']),'exact two phase1 clients')
 return dict(c)
def macros(p):
 return ['#define A04_COLLECTION_REBOOT 1','#define A04_COLLECTION_REBOOT_PROFILE 1'] if policy_config(p) else []
def admit(c,p,explicit):
 cfg=policy_config(p)
 require(type(explicit) is bool and bool(cfg)==explicit,'collection reboot requires explicit host flag plus new profile')
 require(c.get('collection_reboot')==cfg if cfg else c.get('collection_reboot') in (None,False),'seal/profile collection opt-in mismatch')
 return cfg
def proof_events(raw,terminal,policy):
 require(terminal['phase']==1 and terminal['flush']==0 and all(terminal['clients'][i].get('pid',0)>0 and 'result' in terminal['clients'][i] for i in (0,1)),'both clients must finish in the same terminal')
 events=[]
 for line in raw.splitlines():
  hit=re.fullmatch(rb'PB 00000000 (000002[D-E][0-9A-F]) ([0-9A-F]{8})\r?',line)
  if hit:events.append((int(hit[1],16),int(hit[2],16)))
 require(not any(e==0x2d2 for e,v in events),'collection waiter failed; no reset fallback')
 relevant=[(e,v) for e,v in events if e in (0x2d3,0x2d4,0x2d0,0x2d1) or 0x2e0<=e<=0x2e7]
 expected=[(0x2d3,terminal['run']),(0x2d4,terminal['PID'])]+[(0x2e0+i,int(policy[i*8:i*8+8],16)) for i in range(8)]+[(0x2d0,terminal['code']),(0x2d1,0)]
 require(len(relevant)<=len(expected) and relevant==expected[:len(relevant)],'collection receipt run/PID/policy/result/API sequence mismatch')
 return len(relevant)==len(expected)
def wait_fastboot(root,c,out,terminal,uart,offset,clean,run,bounded_shell,clock=time.monotonic,sleep=time.sleep):
 require(c.get('collection_reboot')==CONFIG,'explicit sealed collection mode')
 deadline=clock()+CONFIG['fastboot_wait_seconds'];observed=False
 while clock()<deadline:
  with Path(uart).open('rb') as f:f.seek(offset);raw=clean(f.read())
  (out/'collection-reboot-uart.log').write_bytes(raw)
  require(not re.search(rb'A04(?:GPU|WIN) BUGCHECK|Unhandled Exception',raw),'runtime fault during collection shutdown')
  observed=proof_events(raw,terminal,c['policy_sha256'])
  if observed:
   # Read-only fastboot identity only. No UART/RST, reboot or device writes here.
   shell='set -euo pipefail\nsource "$1"\n'+bounded_shell+'[ "$rt_target_serial" = "$2" ]\n[ -n "$(fastboot devices)" ]\ninfo=$(fastboot getvar product 2>&1)\n[[ "$info" =~ product:[[:space:]]*A04BR3 ]]\n'
   try:
    with (out/'collection-fastboot-probe.log').open('ab') as f:r=run(['bash','-c',shell,'collection-fastboot',str(root/c['target_tools']),c['serial']],cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=min(25,max(.01,deadline-clock())))
   except (subprocess.TimeoutExpired,subprocess.CalledProcessError):
    r=None # Probe unavailable; group already reaped, same absolute deadline.
   if r is not None and r.returncode==0:
    result=dict(schema='MT8167_COLLECTION_REBOOT_HOST_PROOF_V1',terminal=terminal,policy_sha256=c['policy_sha256'],Windows_shutdown_API_return0=True,fastboot_identity_verified=True,UART_reset_requested=False,hardware_success=False)
    (out/'collection-fastboot-proof.json').write_text(json.dumps(result,indent=2)+'\n');return result
  sleep(.5)
 raise RuntimeError('collection fastboot timeout; no UART reset/retry; preserve Windows result')

SOURCE_PATHS=('tools/recovery/collection_reboot.py','tools/recovery/decode-collection-reboot.py','tools/winpe/collection-reboot.h','tools/winpe/collection-reboot-contract.h')
def source_closure(repo):
 import hashlib
 return [dict(path=p,sha256=hashlib.sha256((repo/p).read_bytes()).hexdigest()) for p in SOURCE_PATHS]
def enable_next_profile(p):
 """Next actual-profile adapter calls this before final policy hashing. No capture invention."""
 import copy
 require(p.get('draft') is False and p.get('main_execution_approved') is True,'actual finalized next profile required')
 require('pending' not in p['profile'] and not p.get('collection_reboot'),'no draft/duplicate opt-in')
 # All currently deployed generations remain ineligible for retrofitting.
 require(p.get('capture_number',0)>231,'new actual capture after231 required; run232 frozen')
 q=copy.deepcopy(p);q['profile']+='-collection-reboot-v1';q['collection_reboot']=dict(CONFIG);policy_config(q);return q

def verify_source_pins(repo,c,p,service_manifest,workspace=None):
 cfg=policy_config(p)
 require(bool(service_manifest.get('collection_reboot'))==bool(cfg),'service/profile collection opt-in mismatch')
 if not cfg:return
 expected=dict(schema='MT8167_COLLECTION_REBOOT_COMPILE_V1',config=cfg,headers=source_closure(repo),macros=['A04_COLLECTION_REBOOT=1','A04_COLLECTION_REBOOT_PROFILE=1'])
 require(service_manifest['collection_reboot']==expected,'collection service source/compile closure mismatch')
 workspace=(workspace or repo).resolve(strict=True);repo=repo.resolve(strict=True)
 require(repo.is_relative_to(workspace),'collection source checkout outside workspace')
 pins={}
 for r in c['pinned_files']:
  actual=(workspace/r['path']).resolve(strict=True)
  require(actual.is_relative_to(workspace) and actual not in pins,'out-of-root/duplicate normalized collection pin')
  pins[actual]=r['sha256']
 for ref in expected['headers']:
  actual=(repo/ref['path']).resolve(strict=True)
  require(actual.is_relative_to(repo) and actual.is_relative_to(workspace) and pins.get(actual)==ref['sha256'],'missing/mismatched collection source seal pin '+ref['path'])

if __name__=='__main__':
 import argparse
 ap=argparse.ArgumentParser();ap.add_argument('--policy',type=Path,required=True);ap.add_argument('--repo',type=Path,required=True);ap.add_argument('--explicit',action='store_true');a=ap.parse_args()
 p=json.loads(a.policy.read_text(encoding='utf-8-sig'));cfg=policy_config(p);require(bool(cfg)==a.explicit,'explicit recipe/profile opt-in mismatch')
 print(json.dumps(None if not cfg else dict(schema='MT8167_COLLECTION_REBOOT_COMPILE_V1',config=cfg,headers=source_closure(a.repo),macros=['A04_COLLECTION_REBOOT=1','A04_COLLECTION_REBOOT_PROFILE=1'])))
