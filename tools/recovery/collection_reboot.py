"""Explicit new-profile collection reboot contract and no-reset host boundary."""
import hashlib,importlib.util,json,re,subprocess,time
from pathlib import Path
CONFIG=dict(schema='MT8167_COLLECTION_REBOOT_OPT_IN_V1',enabled=True,receipt_schema='MT8167_COLLECTION_REBOOT_PROOF_V1',fastboot_wait_seconds=180,maximum_shutdown_requests=1,require_both_clients_completed=True,preserve_diagnostic_result=True,uart_reset_allowed=False)
def require(v,msg):
 if not v:raise ValueError(msg)
SNAPSHOT_CONFIG=dict(schema='MT8167_COLLECTION_VIDPN_UART_V1',enabled=True,deadline_ms=30000,submit_reserve_ms=4000,max_chunk_bytes=94,current_boot_proven=False,writer_source_unknown=True)
def snapshot_config(p):
 s=p.get('vidpn_uart_snapshot')
 if s is None:return None
 require(json.dumps(s,sort_keys=True)==json.dumps(SNAPSHOT_CONFIG,sort_keys=True) and p.get('collection_reboot')==CONFIG,'explicit collection snapshot contract')
 return dict(s)
def policy_config(p):
 snapshot_config(p)
 c=p.get('collection_reboot')
 if c is None:return None
 require(json.dumps(c,sort_keys=True)==json.dumps(CONFIG,sort_keys=True) and p['profile'].endswith('-collection-reboot-v1'),'explicit new collection-reboot profile required')
 require(p['service']==p['data_directory'] and p['first_install_mask']==p['deferred_unbound_mask']==0,'no new install mode')
 require(len(p['diagnostic_clients'])==2 and [x['name'] for x in p['diagnostic_clients']]==['gpu_present','gpu_globaldiag'] and all(x['phase']==1 for x in p['diagnostic_clients']),'exact two phase1 clients')
 return dict(c)
def macros(p):
 result=['#define A04_COLLECTION_REBOOT 1','#define A04_COLLECTION_REBOOT_PROFILE 1'] if policy_config(p) else []
 if snapshot_config(p):result+=['#define A04_VIDPN_UART_SNAPSHOT 1']
 return result
def compile_macros(p):return [s.removeprefix('#define ').replace(' ','=') for s in macros(p)]
def admit(c,p,explicit):
 cfg=policy_config(p)
 require(type(explicit) is bool and bool(cfg)==explicit,'collection reboot requires explicit host flag plus new profile')
 require(c.get('collection_reboot')==cfg if cfg else c.get('collection_reboot') in (None,False),'seal/profile collection opt-in mismatch')
 require(c.get('vidpn_uart_snapshot')==snapshot_config(p),'seal/profile snapshot opt-in mismatch')
 return cfg
def proof_events(raw,terminal,policy):
 clients=terminal.get('clients',{})
 require(terminal.get('phase')==1 and terminal.get('flush')==0 and all(type(terminal.get(k)) is int for k in ('run','PID','code')) and isinstance(clients,dict) and all(isinstance(clients.get(i),dict) and clients[i].get('pid',0)>0 and 'result' in clients[i] for i in (0,1)),'both clients must finish in the same terminal')
 events=[]
 for line in raw.splitlines():
  hit=re.fullmatch(rb'PB 00000000 (000002[D-E][0-9A-F]) ([0-9A-F]{8})\r?',line)
  if hit:events.append((int(hit[1],16),int(hit[2],16)))
 require(not any(e==0x2d2 for e,v in events),'collection waiter failed; no reset fallback')
 relevant=[(e,v) for e,v in events if e in (0x2d3,0x2d4,0x2d0,0x2d1) or 0x2e0<=e<=0x2e7]
 expected=[(0x2d3,terminal['run']),(0x2d4,terminal['PID'])]+[(0x2e0+i,int(policy[i*8:i*8+8],16)) for i in range(8)]+[(0x2d0,terminal['code']),(0x2d1,0)]
 require(len(relevant)<=len(expected) and relevant==expected[:len(relevant)],'collection receipt run/PID/policy/result/API sequence mismatch')
 return len(relevant)==len(expected)
def save_vu_snapshot(c,out,terminal,raw,*,normal_offset=0,full_raw=None,storage_trace=None):
 if not c.get('vidpn_uart_snapshot'):return None
 require(c['vidpn_uart_snapshot']==SNAPSHOT_CONFIG,'exact sealed VU snapshot config')
 spec=importlib.util.spec_from_file_location('collection_vidpn_vu',Path(__file__).with_name('decode-vidpn-vu.py'))
 module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 original=raw if full_raw is None else full_raw
 removed=list(re.finditer(storage_trace,original)) if storage_trace else []
 require((re.sub(storage_trace,b'',original) if storage_trace else original)==raw,'VU raw/deinterleaved provenance mismatch')
 def original_boundary(position,right=False):
  deleted=0;answer=position
  for match in removed:
   boundary=match.start()-deleted;length=match.end()-match.start()
   if boundary<position or (right and boundary==position):answer+=length
   deleted+=length
  return answer
 def span(begin,end):
  return dict(clean_start=begin,clean_end=end,original_start=normal_offset+original_boundary(begin,True),original_end=normal_offset+original_boundary(end,begin==end))
 # Match the decoder's candidate rule, including malformed/contaminated VU.
 # Never skip a bad first candidate to select a later apparently valid stream.
 candidates=[];at=0
 for line in raw.split(b'\n'):
  if line.startswith(b'VU') or b'VU ' in line:candidates.append(at)
  at+=len(line)+1
 begin=candidates[0] if candidates else len(raw)
 markers=list(module.BOOT.finditer(raw));following=[m for m in markers if m.start()>=begin]
 boundary=following[0] if following else None;end=boundary.start() if boundary else len(raw)
 window=raw[begin:end]
 original_window=original[original_boundary(begin,True):original_boundary(end,begin==end)]
 require((re.sub(storage_trace,b'',original_window) if storage_trace else original_window)==window,'VU selected original/clean window mismatch')
 (out/'collection-reboot-uart-raw.log').write_bytes(original)
 (out/'collection-reboot-uart.log').write_bytes(raw)
 (out/'vidpn-vu-window.uart').write_bytes(window)
 try:shutdown=proof_events(raw,terminal,c['policy_sha256'])
 except (ValueError,KeyError):shutdown=False
 fault=lambda data:bool(re.search(rb'A04(?:GPU|WIN) BUGCHECK|Unhandled Exception',data))
 later=[pos for pos in candidates if pos>=end] if boundary else []
 side=dict(schema='MT8167_VIDPN_VU_WINDOW_V1',coordinate_system='clean ranges relative to normal window; original ranges absolute in source UART',
  normal_offset=normal_offset,normal_end=normal_offset+len(original),full_raw_file='collection-reboot-uart-raw.log',full_raw_sha256=hashlib.sha256(original).hexdigest(),full_raw_bytes=len(original),
  cleaned_file='collection-reboot-uart.log',cleaned_sha256=hashlib.sha256(raw).hexdigest(),window_file='vidpn-vu-window.uart',window_sha256=hashlib.sha256(window).hexdigest(),original_window_sha256=hashlib.sha256(original_window).hexdigest(),window=span(begin,end),
  window_rule='first VU candidate through immediately before next BOOT_REASON; no latest/resync/END-based trimming',
  boot_markers=[span(m.start(),m.end()) for m in markers],following_boot_marker=span(boundary.start(),boundary.end()) if boundary else None,
  removed_storage_ranges=[dict(original_start=normal_offset+m.start(),original_end=normal_offset+m.end()) for m in removed],
  later_vu_candidates=[span(pos,min(len(raw),raw.find(b'\n',pos)+1 if b'\n' in raw[pos:] else len(raw))) for pos in later],
  reboot_observed=bool(boundary),normal_shutdown_API_return0_recorded=shutdown,exception_before_boundary_observed=fault(raw[:end]),exception_in_full_window_observed=fault(raw),
  reboot_classification=('exception-evidence-and-reboot-observed' if fault(raw[:end]) else 'shutdown-API0-and-reboot-observed' if shutdown else 'reboot-observed-cause-unproven') if boundary else 'no-following-boot-marker-observed',
  post_boundary_VU_present=bool(later),boot_cause_proven=False,current_boot_unproven=True,hardware_success=False)
 try:result=module.decode(window,terminal['run'],terminal['PID'])
 except module.Rejected as error:result=dict(schema='GE8300_VIDPN_VU_HOST_V1',transport_complete=False,current_boot_unproven=True,hardware_success=False,error=str(error))
 result['window_sidecar']='vidpn-vu-window.json';side['decoder_transport_complete']=result['transport_complete'];side['decoder_error']=result.get('error')
 (out/'vidpn-vu-window.json').write_text(json.dumps(side,indent=2)+'\n')
 (out/'vidpn-vu-snapshot.json').write_text(json.dumps(result,indent=2)+'\n');return result
def wait_fastboot(root,c,out,terminal,uart,offset,clean,run,bounded_shell,clock=time.monotonic,sleep=time.sleep,storage_trace=None):
 require(c.get('collection_reboot')==CONFIG,'explicit sealed collection mode')
 deadline=clock()+CONFIG['fastboot_wait_seconds'];observed=False
 while clock()<deadline:
  with Path(uart).open('rb') as f:f.seek(offset);full_raw=f.read();raw=clean(full_raw)
  (out/'collection-reboot-uart.log').write_bytes(raw)
  fault=bool(re.search(rb'A04(?:GPU|WIN) BUGCHECK|Unhandled Exception',raw));proof_error=None
  try:observed=proof_events(raw,terminal,c['policy_sha256'])
  except ValueError as error:observed=False;proof_error=str(error)
  # Read-only discovery is independent of UART delivery or boot-cause proof.
  # Read-only fastboot identity only. No UART/RST, reboot or device writes here.
  shell='set -euo pipefail\nsource "$1"\n'+bounded_shell+'[ "$rt_target_serial" = "$2" ]\n[ -n "$(fastboot devices)" ]\ninfo=$(fastboot getvar product 2>&1)\n[[ "$info" =~ product:[[:space:]]*A04BR3 ]]\n'
  try:
   with (out/'collection-fastboot-probe.log').open('ab') as f:r=run(['bash','-c',shell,'collection-fastboot',str(root/c['target_tools']),c['serial']],cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=min(25,max(.01,deadline-clock())))
  except (subprocess.TimeoutExpired,subprocess.CalledProcessError):
   r=None # Probe unavailable; group already reaped, same absolute deadline.
  if r is not None and r.returncode==0:
   snapshot=save_vu_snapshot(c,out,terminal,raw,normal_offset=offset,full_raw=full_raw,storage_trace=storage_trace)
   result=dict(schema='MT8167_COLLECTION_REBOOT_HOST_PROOF_V1',terminal=terminal,policy_sha256=c['policy_sha256'],collection_transport_status='fastboot-observed-api-return0-recorded' if observed else 'fastboot-observed-shutdown-unproven',Windows_shutdown_API_return0=True if observed else None,UART_shutdown_proof_complete=observed,UART_proof_validation_error=proof_error,runtime_fault_observed=fault,fastboot_identity_verified=True,UART_reset_requested=False,hardware_success=False)
   if snapshot is not None:result['vidpn_vu_snapshot_output']='vidpn-vu-snapshot.json'
   (out/'collection-fastboot-proof.json').write_text(json.dumps(result,indent=2)+'\n');return result
  sleep(.5)
 save_vu_snapshot(c,out,terminal,raw,normal_offset=offset,full_raw=full_raw,storage_trace=storage_trace)
 raise RuntimeError('collection fastboot timeout; no UART reset/retry; preserve Windows result')

SOURCE_PATHS=('tools/recovery/collection_reboot.py','tools/recovery/decode-collection-reboot.py','tools/winpe/collection-reboot.h','tools/winpe/collection-reboot-contract.h','tools/winpe/vidpn-uart-snapshot.h','tools/winpe/collection-vidpn-uart.h','tools/recovery/decode-vidpn-vu.py')
def source_closure(repo):
 import hashlib
 return [dict(path=p,sha256=hashlib.sha256((repo/p).read_bytes()).hexdigest()) for p in SOURCE_PATHS]
def enable_next_profile(p,vidpn_uart_snapshot=False):
 """Next actual-profile adapter calls this before final policy hashing. No capture invention."""
 import copy
 require(p.get('draft') is False and p.get('main_execution_approved') is True,'actual finalized next profile required')
 require('pending' not in p['profile'] and not p.get('collection_reboot'),'no draft/duplicate opt-in')
 # All currently deployed generations remain ineligible for retrofitting.
 require(p.get('capture_number',0)>231,'new actual capture after231 required; run232 frozen')
 q=copy.deepcopy(p);q['profile']+='-collection-reboot-v1';q['collection_reboot']=dict(CONFIG)
 if vidpn_uart_snapshot:q['vidpn_uart_snapshot']=dict(SNAPSHOT_CONFIG)
 policy_config(q);return q

def verify_source_pins(repo,c,p,service_manifest,workspace=None):
 cfg=policy_config(p)
 require(bool(service_manifest.get('collection_reboot'))==bool(cfg),'service/profile collection opt-in mismatch')
 if not cfg:return
 expected=dict(schema='MT8167_COLLECTION_REBOOT_COMPILE_V1',config=cfg,headers=source_closure(repo),macros=compile_macros(p))
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
 print(json.dumps(None if not cfg else dict(schema='MT8167_COLLECTION_REBOOT_COMPILE_V1',config=cfg,headers=source_closure(a.repo),macros=compile_macros(p))))
