#!/usr/bin/env python3
"""既知profile切替とseal入力をhost fixtureで検査。実機/runner/VM操作なし。"""
import ast,copy,hashlib,importlib.util,json,subprocess
from pathlib import Path
repo=Path(__file__).resolve().parents[1]
def load(p):
 s=importlib.util.spec_from_file_location('gpu_test',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
m=load(repo/'tools/recovery/run-gpu368-fresh200.py')
wrapper=load(repo/'tools/recovery/run-gpu368-fresh202.py')
def observation(s):
 t=ast.parse(s);return ast.dump(next(n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Observation'),include_attributes=False)
prior=subprocess.check_output(['git','show','b3c10a8b1c0b08c75b2f77d3776b91c2cf791586:tools/recovery/run-gpu368-fresh200.py'],cwd=repo,text=True)
assert observation(prior)==observation((repo/'tools/recovery/run-gpu368-fresh200.py').read_text())
assert wrapper.Observation.__name__==m.Observation.__name__
k=m.contract('run203');assert (k['run'],k['fresh'],k['capture'],k['service'])==(203,202,204,'MT8167CombinedFollowup202')
try:m.contract('run205');raise AssertionError('unknown profile')
except ValueError:pass
root=repo/'build/gpu368-run203-seal-fixture';root.mkdir(parents=True,exist_ok=True)
def write(name,data):
 p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);return p
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
c=json.loads((repo/'tools/recovery/gpu368-run203-seal.pending.json').read_text())
c.update(status='validated-main-seal',source_tree='source',profile_source_commit='a'*40,serial='HOST_FIXTURE',recovery_boot_id='host-only',controller_path='controller/tools/recovery/run-gpu368-fresh202.py',controller_engine='controller/tools/recovery/run-gpu368-fresh200.py',collector_present_helper='controller/tools/recovery/collect-present356.py',diagnostics_collector='controller/tools/recovery/collect-combined200.sh',capture_tool='source/capture.py',crc_decoder='source/crc.py',normal_esp='normal.img',media_directory='media')
(root/'source').mkdir(exist_ok=True)
for key in ('controller_path','controller_engine','collector_present_helper','diagnostics_collector'):
 write(c[key],(repo/'tools/recovery'/Path(c[key]).name).read_bytes())
for key in ('reset_script','target_tools','ntfs3g','capture_tool','crc_decoder','normal_esp'):write(c[key],b'host-fixture-not-executable')
fm=dict(status='captured-unmounted',read_only=True,unmounted=True,errors=[],recovery_boot_id='host-only',serial='HOST_FIXTURE')
write(c['fresh_manifest'],json.dumps(fm).encode());c['fresh_manifest_sha256']=sha(root/c['fresh_manifest'])
p=dict(profile=k['profile'],service=k['service'],data_directory=k['service'],install_targets=['render'],planned_reboots=1,backup_manifest_sha256=c['fresh_manifest_sha256'],capture_selection=dict(capture_number=202),boot_transition=dict(boot_write_required=False,gpt_written=False),diagnostic_clients=[dict(name='gpu_present',phase=1,timeout_ms=900000),dict(name='gpu_globaldiag',phase=1,timeout_ms=60000)],observer_contract=dict(absolute_batch_timeout_seconds=1140))
c['policy_sha256']=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest();c['prepare_token']=c['policy_sha256'][:16].upper()
c['wim_sha256']=sha(write('media/update.wim',b'not-a-WIM'));c['esp_sha256']=sha(write('media/update-esp.img',b'not-an-ESP'));c['normal_esp_sha256']=sha(root/c['normal_esp'])
write('media/manifest.json',json.dumps(dict(policy=p,wim_sha256=c['wim_sha256'],esp_sha256=c['esp_sha256'])).encode());c['media_manifest_sha256']=sha(root/'media/manifest.json')
keys=('controller_path','controller_engine','collector_present_helper','diagnostics_collector','reset_script','target_tools','ntfs3g','capture_tool','crc_decoder','normal_esp','fresh_manifest')
c['pinned_files']=[dict(path=c[x],sha256=sha(root/c[x])) for x in keys]+[dict(path='media/manifest.json',sha256=c['media_manifest_sha256'])]
# Only git HEAD lookup is stubbed. No subprocess API used by execution can run.
original=m.subprocess.check_output
m.__file__=str(root/c['controller_engine'])
def head_only(args,**kwargs):
 assert args==['git','-C',str(root/'source'),'rev-parse','HEAD'];return 'a'*40+'\n'
m.subprocess.check_output=head_only
seal=root/'seal.json'
def validate(v):
 seal.write_text(json.dumps(v));return m.validate_seal(root,seal,sha(seal),'run203',root/c['controller_path'])
try:
 assert validate(c)[0]['transaction']=='combined-run203'
 for key,value in [('schema','GPU368_RUN201_MAIN_SEAL_V1'),('transaction','combined-run201'),('service_name','MT8167CombinedFollowup200'),('fresh_manifest','port/xnu/out/private-windows-rt-prebackup-run200/manifest.json'),('capture_output','port/xnu/out/private-windows-rt-prebackup-run202'),('diagnostics_output','port/xnu/out/private-windows-rt-gpu368-run201-diagnostics'),('planned_windows_reboots',2),('controller_engine',c['controller_path'])]:
  bad=copy.deepcopy(c);bad[key]=value
  try:validate(bad);raise AssertionError('mixed seal accepted: '+key)
  except (ValueError,FileNotFoundError):pass
 validate(c)
finally:m.subprocess.check_output=original
# Execute only collector's configuration heredoc, never its shell/device code.
body=(repo/'tools/recovery/collect-combined200.sh').read_text().split("<<'CONFIG'\n",1)[1].split('\nCONFIG',1)[0]
state=write('result.json',json.dumps(dict(transaction='combined-run203',status='install-failed-await-disk-verification')).encode())
r=subprocess.run(['python3','-c',body,str(seal),str(state)],cwd=root,text=True,capture_output=True);assert r.returncode==0,r.stderr
assert r.stdout.splitlines()==['MT8167CombinedFollowup202','MT8167CombinedFollowup202','MT8167D3DClear']
state.write_text(json.dumps(dict(transaction='combined-run201',status='install-failed-await-disk-verification')))
r=subprocess.run(['python3','-c',body,str(seal),str(state)],cwd=root,text=True,capture_output=True);assert r.returncode!=0
print('PASS run203: same Observation, sealed fresh202/service202/capture204, mixed inputs rejected, collector selection; no device/VM')
