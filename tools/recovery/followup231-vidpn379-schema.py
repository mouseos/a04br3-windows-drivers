"""Exact379 package adapter; no signing, output generation or hive operation.
Admission still requires the actual accepted lock, not source expectations alone.
"""
import hashlib,json,re
from pathlib import Path
SCHEMA='GE8300_VIDPN379_SINGLE_TU_V1'
ADAPTER_ID='vidpn379-single-tu-client-settle-v1'
TRACE='b9fb91571b587ab98c01ed1a118c847b634b21ca'
RECIPE='8d2d4b77d1f10f0a4a4f03043ed27a0fc0507cd5'
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
 m=read(manifest)
 require(m['schema']==SCHEMA and m['source_commit']==RECIPE and m['trace_source_commit']==TRACE and m['baseline_source_commit']==BASE and m['client_source_commit']==CLIENT,'exact379 source family')
 require(m['success'] is True and m['signed'] is True and m['signer_thumbprint']==SIGNER and m['candidate_version']==379 and m['runtime_build_id_retained_from378'] is True and m['hardware_verified'] is False,'actual379 signed result only')
 require(all(m[k] is False for k in ('SDK_rebuilt','PCO_rebuilt','UMD_rebuilt','native_owner_rebuilt')) and m['native_owner_sha256']==NATIVE,'retained378 inputs only')
 tus=m['compiled_TUs'];require(len(tus)==1 and tus[0]['source']=='drivers/gpu/mt8167/wddm-display/unified_display.cxx' and tus[0]['object']=='display_unified_display.obj' and re.fullmatch('[0-9a-f]{64}',tus[0]['sha256']),'one bridge TU only')
 closure=tus[0]['include_closure'];require(len(closure)>10 and any(n.replace('\\','/').endswith('/vidpn_callback_trace.h') for n in closure),'actual trace header closure')
 required={'compile-bridge','archive','kernel-link','pe','sign-sys','catalog','sign-cat','certificates'}|{'trust-'+str(i) for i in range(6)}|{'catalog-'+n for n in ('ge8300-render.sys','ge8300-render.inf','ge8300-umd.dll','ge8300-mips.fw')}
 require(set(m['steps'])==required and all(s['exit_code']==0 and re.fullmatch('[0-9a-f]{64}',s['log_sha256']) for s in m['steps'].values()),'all exact379 steps must complete once')
 archive=m['archive'];require(archive['baseline_sha256']==BASE_ARCHIVE and archive['replacement']=='display_unified_display.obj' and archive['members']['display_unified_display.obj']==tus[0]['sha256'],'retained378 archive replacement')
 baseline=path(root,'port/xnu/out/windows-rt-wire378/recovered/package378/signed-manifest.json');require(sha(baseline)==BASE_MANIFEST,'exact signed378 baseline')
 prior={r['name']:r['sha256'] for r in read(baseline)['outputs']};files={}
 require(len(m['outputs'])==len(prior)==15,'full379 fifteen outputs')
 for r in m['outputs']:
  n=r['name'];require(re.fullmatch(r'(?:display-transport/)?[A-Za-z0-9_.-]+',n) and n not in files and n in prior,'fixed379 output names')
  p=path(root,manifest.parent/n);require(p.stat().st_size==r['bytes'] and sha(p)==r['sha256'],'actual379 output hash/size '+n);files[n]=r['sha256']
 require(set(files)==set(prior) and {n for n in files if files[n]!=prior[n]}==CHANGED and files['mt8167-dxgi-present356.exe']==EVALUATOR,'only SYS/INF/CAT/settle evaluator changed; UMD/FW/transport retained')
 return m,files
