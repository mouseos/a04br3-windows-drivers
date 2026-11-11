"""Exact379 package adapter; no signing, output generation or hive operation.
Admission still requires the actual accepted lock, not source expectations alone.
"""
import hashlib,json,re
from pathlib import Path
SCHEMA='GE8300_VIDPN379_SINGLE_TU_V1'
MANIFEST='2046d42b7da6c5c3401a87786ff45b3ff4f02ae49804ecf9f3c5fb62cada6aa9'
SYS='5a7066d4b1b8664d3015d02ea242cf6886683047d509656e1c3c51e9e28c3237'
ADAPTER_ID='vidpn379-single-tu-client-settle-v1'
TRACE='b9fb91571b587ab98c01ed1a118c847b634b21ca'
RECIPE='2f210c9f05e784a1245170a1ae525b0b9507d8b0'
BASE='e215c7015f8040ffe8731bb97adc05519d3e1028'
CLIENT='bdc6e061ddbaf22121286c81528159ccb55293c0'
EVALUATOR='c6fb3f43c530288473fc243d986d8c843a49468c425d48d090dd44d518b244b4'
BASE_MANIFEST='6ff34226b405bf876055038d8634d72647d3db13033c100a41d269cf7820c99e'
BASE_ARCHIVE='7cfbcb5bdbc351f257f2f5c2a92843ff7e72b8697f3d8370bb59e8062bc71074'
NATIVE='c7dcbffb1d3de151e22796c0494c033502570950d0afebb4162a6a2f598fa836'
SIGNER='BCBF6C456C6F0DD0818434763B189488FBF13570'
CHANGED={'ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','mt8167-dxgi-present356.exe'}

def require(ok,msg):
 if not ok:raise ValueError(msg)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def path(root,name):
 p=(root/name).resolve(strict=True);require(p.is_relative_to(root),'root-local379 input only');return p

def validate(root,manifest,lock):
 root=root.resolve(strict=True)
 require(lock['schema']=='FOLLOWUP231_ACCEPTED_CANDIDATES_V1' and lock['status']=='actual-signed-candidates-main-reviewed' and lock['adapter_id']==ADAPTER_ID and lock['mode']=='render-update','actual379 lock pending')
 ref=lock['GPU_candidate'];require(set(ref)=={'path','sha256'} and manifest.resolve()==path(root,ref['path']) and sha(manifest)==ref['sha256'],'exact379 manifest lock')
 # Both independent review and finalized client/launcher pair are mandatory.
 for key in ('main_review','client_pair'):
  r=lock[key];require(set(r)=={'path','sha256'} and sha(path(root,r['path']))==r['sha256'],'actual379 review/client pair ref')
 return inspect_signed_manifest(root,manifest)

def inspect_signed_manifest(root,manifest):
 # Read-only artifact inspection does not admit finalize or substitute main review.
 root=root.resolve(strict=True);require(sha(manifest)==MANIFEST,'exact actual379 manifest')
 m=read(manifest)
 require(m['schema']==SCHEMA and m['source_commit']==RECIPE and m['trace_source_commit']==TRACE and m['baseline_source_commit']==BASE and m['client_source_commit']==CLIENT,'exact379 source family')
 require(m['success'] is True and m['signed'] is True and m['signer_thumbprint']==SIGNER and m['candidate_version']==379 and m['runtime_build_id_retained_from378'] is True and m['hardware_verified'] is False,'actual379 signed result only')
 require(all(m[k] is False for k in ('SDK_rebuilt','PCO_rebuilt','UMD_rebuilt','native_owner_rebuilt')) and m['native_owner_sha256']==NATIVE,'retained378 inputs only')
 tus=m['compiled_TUs'];require(len(tus)==1 and tus[0]['source'].replace('\\','/')=='drivers/gpu/mt8167/wddm-display/unified_display.cxx' and tus[0]['object']=='display_unified_display.obj' and re.fullmatch('[0-9a-f]{64}',tus[0]['sha256']),'one bridge TU only')
 closure=tus[0]['include_closure'];require(len(closure)==92 and all(isinstance(n,str) and re.fullmatch('[0-9a-f]{64}',h) for n,h in closure.items()),'actual379 recorded 92-header closure')
 for suffix,digest in (
  ('/drivers/gpu/mt8167/wddm-display/unified_display.cxx','4393a26b5b5faaa328ca8a5b3c2c3e8c6bcdcebfa36a7f367d22641a609b6ba4'),
  ('/drivers/gpu/mt8167/wddm-display/vidpn_callback_trace.h','0fba97d0ff2ac4dcdf0b3c769d9f2ede4d6967b09c7395945e05b088ea2f24a6')):
  found=[h for n,h in closure.items() if n.replace('\\','/').endswith(suffix)]
  require(found==[digest],'exact b9fb trace source/header in real compile closure')
 required={'compile-bridge','archive','kernel-link','pe','sign-sys','catalog','sign-cat','certificates'}|{'trust-'+str(i) for i in range(6)}|{'catalog-'+n for n in ('ge8300-render.sys','ge8300-render.inf','ge8300-umd.dll','ge8300-mips.fw')}
 require(set(m['steps'])==required and all(s['exit_code']==0 and re.fullmatch('[0-9a-f]{64}',s['log_sha256']) for s in m['steps'].values()),'all exact379 steps must complete once')
 archive=m['archive'];require(archive['baseline_sha256']==BASE_ARCHIVE and archive['replacement']=='display_unified_display.obj' and archive['members']['display_unified_display.obj']==tus[0]['sha256'],'retained378 archive replacement')
 baseline=path(root,'port/xnu/out/windows-rt-wire378/recovered/package378/signed-manifest.json');require(sha(baseline)==BASE_MANIFEST,'exact signed378 baseline')
 prior={r['name']:r['sha256'] for r in read(baseline)['outputs']};files={}
 require(len(m['outputs'])==len(prior)==15,'full379 fifteen outputs')
 for r in m['outputs']:
  n=r['name'];require(re.fullmatch(r'(?:display-transport/)?[A-Za-z0-9_.-]+',n) and n not in files and n in prior,'fixed379 output names')
  p=path(root,manifest.parent/n);require(p.stat().st_size==r['bytes'] and sha(p)==r['sha256'],'actual379 output hash/size '+n);files[n]=r['sha256']
 require(set(files)==set(prior) and {n for n in files if files[n]!=prior[n]}==CHANGED and files['mt8167-dxgi-present356.exe']==EVALUATOR and files['ge8300-render.sys']==SYS,'only SYS/INF/CAT/settle evaluator changed; UMD/FW/transport retained')
 return m,files

PAIR_MANIFEST='8682c4b9232643c7de5c85644687946f54d9096733f9806b4348b1d2bd502f05'
LAUNCHER_SOURCE='30cf7ea4ccaeb1ca7b2f742012e2e1c7c920c968'
LAUNCHER_SHA='7a5b402a6692bb2ce77cc69d2c4d020256f3fe9c99e08de4f2f02112f02f76f1'
PAIR_RECIPE='483176baa4cd5bfa41707f784b5a8bf5b4434f0f'
def inspect_signed_pair(root,manifest):
 # Read exact native handoff schema; normalization is in memory only.
 root=root.resolve(strict=True);require(sha(manifest)==PAIR_MANIFEST,'exact formal signed settle pair')
 p=read(manifest)
 require(p['schema']=='ge8300-client375-active-settle-signed-pair-v1' and p['success'] is True and p['evaluator_source_commit']==CLIENT and p['launcher_source_commit']==p['formal_launcher_expected_commit']==LAUNCHER_SOURCE and p['recipe_source_commit']==PAIR_RECIPE,'exact formal30cf/bdc6 pair schema')
 require(p['evaluator_sha256']==p['launcher_expected_sha256']==EVALUATOR and p['launcher_sha256']==LAUNCHER_SHA and p['launcher_guard_relaxed'] is False,'formal launcher expected evaluator')
 require(p['evaluator_recompiled_for_launcher']==p['memory_object_recompiled']==p['GPU_compiles']==p['kernel_compiles']==p['SDK_rebuilds']==p['hardware_operations']==0 and p['launcher_actual_compiles']==1 and p['evaluator_actual_compiles']==1 and p['driver_package_modified'] is False and p['hardware_verified'] is False and p['runtime_verified'] is False and p['non_signature_payload_unchanged'] is True,'no recompile/driver change/runtime-success inference')
 def pin(ref):
  f=path(root,ref['path']);require(f.stat().st_size==ref['bytes'] and sha(f)==ref['sha256'],'formal pair hash/size '+ref['path']);return dict(path=ref['path'],sha256=ref['sha256'])
 require(len(p['outputs'])==2 and len(p['import_proofs'])==2,'exact two leaves/proofs')
 outputs={Path(r['path']).name:r for r in p['outputs']};proofs={r['phase']:r for r in p['import_proofs']}
 require(set(outputs)=={'mt8167-dxgi-console-launch363.exe','mt8167-dxgi-present356.exe'} and set(proofs)=={'launcher','evaluator'},'fixed signed pair names')
 result=dict(launcher_source_commit=LAUNCHER_SOURCE,references=[])
 for role,name,digest,receipt_digest in (
  ('launcher','mt8167-dxgi-console-launch363.exe',LAUNCHER_SHA,'8085f1965697a416f48867dc8b356de24901386a16acfc4f68309e909d920210'),
  ('evaluator','mt8167-dxgi-present356.exe',EVALUATOR,'c25893737ebed2df6deb626b434522926c4eb21b8a603563ecc182c4ca3a9bf2')):
  require(outputs[name]['sha256']==digest and p[role+'_receipt']['sha256']==receipt_digest,'exact actual pair leaf/receipt')
  result[role]=dict(file=pin(outputs[name]),manifest=pin(p[role+'_receipt']),import_proof=pin(proofs[role]))
  signature=p['signatures'][role];require(signature['status']=='Valid' and signature['sha256']==digest and signature['thumbprint']==SIGNER,'pair signer receipt')
 for k in ('pair_archive','source_archive','source_refs'):result['references'].append(pin(p[k]))
 return result
