#!/usr/bin/env python3
"""拒否済run222だけのhost-side実依存照合。device/hive APIは使用しない。"""
import argparse, hashlib, importlib.util, json
from pathlib import Path
SOURCE='c922efe5227b582801acd2ddba60c6fc823cd0b3'
OLD_SEAL='f3b184fc81b56be6cb5dea7a7167c9f7187a3e740ff0d99541e2c5ba25cda1ef'
OLD_MF='ae47f20ac1c5b1e96352e6834697f6df16bfa0897e8f248096b4efd1ba9f6f99'
NEW_MF='99f90f6871dc66d1fa8fd25b9129d0e1c038041d7aed3bd1176624cc9725d5fc'
SERVICE_MANIFEST='f34b195a2aa6f4b4995d8c6d50b3c834153fbfb75e3445fe0feeae4569cd280b'
NATIVE_RESULT='aae7956bcd8865386302c5b246bd0cdc9eb3e138f277630d950bae60cea470b5'
BASE='port/gsi/logs/ios-feasibility/'
MF='port/xnu/out/private-windows-rt-prebackup-run%d/manifest.json'
RUNTIME=BASE+'windows-rt-combined-run222-runtime/'
SCHEMA='GPU375_RUN222_REJECTED_REBIND223_V1'
def require(ok,message):
 if not ok:raise ValueError(message)
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def path(root,name):
 p=(root/name).resolve(strict=True);require(p.is_relative_to(root),'root外入力');return p

def evaluate(root):
 refs={}
 def pin(name,want=None):
  p=path(root,name);h=sha(p);require(want is None or h==want,'固定証拠hash不一致: '+name);refs[name]=h;return p
 old=read(pin(BASE+'windows-rt-explicit-source375/main-deployment-contract.json',OLD_SEAL))
 latch=read(pin(BASE+'gpu375-run222-executions/combined-run222.json'))
 result=read(pin(RUNTIME+'result.json'))
 require(old['schema']=='GPU375_COMBINED_RUN222_MAIN_SEAL_V1' and old['service_name']=='MT8167CombinedFollowup221','元combined222のみ')
 require(latch['seal_sha256']==result['seal_sha256']==OLD_SEAL and latch['candidate']==hashlib.sha256((old['fresh_manifest_sha256']+old['policy_sha256']+old['esp_sha256']).encode()).hexdigest(),'旧latch/seal結合')
 require(Path(latch['output']).resolve()==path(root,RUNTIME),'旧runtimeのみ')
 require(result['status']=='pipeline-stopped' and result['prior_status']=='prepare-observing' and result['planned_runtime_restarts']==0 and result['events']==[] and result['boot_written'] is False and result['gpt_written'] is False and not result.get('prepare_crc_committed_released') and not result.get('terminal'),'normal/service開始済みは再実行不可')
 decoder_path=pin(old['crc_decoder']);oldpins={r['path']:r['sha256'] for r in old['pinned_files']};require(sha(decoder_path)==oldpins[old['crc_decoder']],'元CRC decoder pin')
 sp=importlib.util.spec_from_file_location('rebind_crc',decoder_path);dm=importlib.util.module_from_spec(sp);sp.loader.exec_module(dm)
 report=pin(RUNTIME+'prepare-report.txt').read_bytes();require(dm.decode(pin(RUNTIME+'prepare-uart.log').read_bytes(),old['prepare_token'])==report,'元UART CRC/report不一致')
 lines=report.decode('ascii').splitlines()
 for line in ('UPDATE_STATUS=BASELINE_REJECTED_NO_CHANGE','UPDATE_RELEASE_EXIT=0','UPDATE_EXIT=6','BASELINE_FAILURE_KIND=PRESERVED_FILE','BASELINE_FAILURE_RELATIVEPATH=Windows/System32/mt8167-dxgi-console-launch363.exe','BASELINE_FAILURE_COMPARISON=32'):
  require(lines.count(line)==1,'既知pre-copy拒否境界のみ: '+line)
 require(not any('COMMITTED' in v for v in lines),'COMMITTED後は再実行不可')
 media=read(pin(old['media_directory']+'/manifest.json',old['media_manifest_sha256']));policy=media['policy']
 require(hashlib.sha256(json.dumps(policy,sort_keys=True,separators=(',',':')).encode()).hexdigest()==old['policy_sha256'],'元policy不一致')
 manifests=[];tables=[]
 for number,want in ((221,OLD_MF),(223,NEW_MF)):
  fp=pin(MF%number,want);fm=read(fp)
  require(fm['status']=='captured-unmounted' and fm['read_only'] is True and fm['unmounted'] is True and fm['errors']==[],'実ro/unmounted capture必須')
  table={r['source_path']:r for r in fm['files']};require(len(table)==len(fm['files'])==42,'固定42files必須')
  for row in table.values():
   leaf=pin((fp.parent/row['backup_path']).relative_to(root).as_posix(),row['sha256']);require(leaf.stat().st_size==row['bytes'],'実file size不一致')
  manifests.append(fm);tables.append(table)
 a,b=manifests;x,y=tables
 require(set(x)==set(y) and all(a[k]==b[k] for k in ('serial','source_partition','source_partition_start_sectors','source_partition_size_sectors','source_partition_major_minor','missing_files')),'別端末/partition/file集合不可')
 diff=[k for k in x if (x[k]['sha256'],x[k]['bytes'])!=(y[k]['sha256'],y[k]['bytes'])]
 require(diff==['Windows/System32/config/SOFTWARE.LOG1'],'既観測のSOFTWARE.LOG1差のみ許可')
 require(b['recovery_boot_id']=='162ac6b0-839c-4cce-9b5a-4350e758bfda','実Recovery223固定')
 trios={'Windows/System32/config/'+h+s for h in ('SYSTEM','DRIVERS') for s in ('','.LOG1','.LOG2')}
 require(all(x[k]['sha256']==y[k]['sha256'] for k in trios),'未arm証拠trio差異')
 package='port/xnu/out/windows-rt-gpu375-combined221-inputs/recovered-service-native221/'
 sm=read(pin(package+'service221/peripheral-service-manifest.json',SERVICE_MANIFEST));nr=read(pin(package+'native221/result.json',NATIVE_RESULT))
 require(sm['source_commit'].strip()==SOURCE and sm['signature_status']=='Valid' and sm['policy']==policy,'既署名C922/policy bindingのみ')
 pin(package+'service221/MT8167CombinedFollowup221.exe',sm['exe_sha256']);pin(package+'native221/candidate/SYSTEM',nr['candidate']['sha256'])
 require(nr['status']=='verified-clean-system-services-only' and nr['peripheral_old_services_preserved'] and not nr['errors'],'既native検証済候補のみ')
 for name in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2'):require(nr['source_files'][name]['sha256']==y['Windows/System32/config/'+name]['sha256'],'native source trio不一致')
 replacements=policy['baseline221_client_transition_reference_only']['client_replacements']
 guards={k:dict(path=v,sha256=policy['preserved_files'][k]) for k,v in policy['preserved_sources'].items() if k in policy['preserved_files'] and k not in replacements};guards.update(policy['baseline_witnesses'])
 live=[]
 for name,row in guards.items():
  pin(row['path'],row['sha256'])
  if name in y:require(row['sha256']==y[name]['sha256'],'pre-copy依存差異: '+name)
  else:live.append(name)
 require(len(guards)==30 and len(live)==6,'固定guard依存集合')
 return dict(schema=SCHEMA,status='dependency-verified-rejected-before-arm-only',original_seal_sha256=OLD_SEAL,original_service_source_commit=SOURCE,policy_sha256=old['policy_sha256'],fresh_manifest=MF%223,fresh_manifest_sha256=NEW_MF,recovery_boot_id=b['recovery_boot_id'],same_files=41,all42_identical=False,differences=diff,precopy_trios=sorted(trios),mandatory_WinPE_old_guard_files=sorted(live),software_log1_write_allowed=False,refs=[dict(path=k,sha256=v) for k,v in sorted(refs.items())])

def validate(root,ref,c):
 sidepath=path(root,ref['path']);require(sha(sidepath)==ref['sha256'],'rebind sidecar pin');side=read(sidepath);require(side==evaluate(root),'拒否/現依存証拠が変更された')
 require(c['policy_sha256']==side['policy_sha256'] and c['fresh_manifest_sha256']==OLD_MF and c['boot_write_required'] is False,'元policy/fresh221/boot0保持')
 media=read(path(root,c['media_directory']+'/manifest.json'))
 require(media['service_source_commit']==SOURCE and media['tooling_only_changes_since_signed_service'] is True,'media/controller sourceと実C922署名sourceを分離')
 require(c['esp_sha256']!=read(path(root,BASE+'windows-rt-explicit-source375/main-deployment-contract.json'))['esp_sha256'],'拒否媒体そのものの再実行禁止')
 return side

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();root=a.workspace.resolve(strict=True);out=a.out.resolve();require(out.is_relative_to(root) and not out.exists(),'root内新規sidecarのみ');value=evaluate(root)
 with out.open('x') as f:json.dump(value,f,indent=2);f.write('\n')
 print(json.dumps(dict(path=out.relative_to(root).as_posix(),sha256=sha(out))))
if __name__=='__main__':main()
