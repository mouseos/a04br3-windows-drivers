#!/usr/bin/env python3
"""拒否済run222だけのhost-side実依存照合。device/hive APIは使用しない。"""
import argparse, hashlib, importlib.util, json, re
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

def evaluate223(root):
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

JOURNAL_MF='bf5e2f6aaf0b7bd91f46aec57d73342d1bb6647e155394c891252de46875fad3'
JOURNAL_SHA='08c8a9e3fd92a5c04ba40a73d5131491af4f34ac3f1379e8df154d362104c888'
JOURNAL_SCHEMA='GPU375_RUN222_JOURNAL_REJECTED_REBIND224_V1'
def evaluate(root,journal224=False):
 side=evaluate223(root)
 if not journal224:return side
 refs={row['path']:row['sha256'] for row in side['refs']}
 def pin(name,want=None):
  p=path(root,name);h=sha(p);require(want is None or h==want,'固定224証拠hash不一致: '+name);refs[name]=h;return p
 sealsha='dbb85a321c2c18ebc10e7b7335008841b7327b221d690480f98df2967e247119'
 seal=read(pin(BASE+'windows-rt-explicit-source375/main-deployment-contract-resume223.json',sealsha))
 previous=RUNTIME.rstrip('/')+'-resume223/'
 latch=read(pin(BASE+'gpu375-run222-executions/combined-run222-resume223.json'));st=read(pin(previous+'result.json'))
 require(latch['seal_sha256']==st['seal_sha256']==sealsha and latch['candidate']==hashlib.sha256((seal['fresh_manifest_sha256']+seal['policy_sha256']+seal['esp_sha256']).encode()).hexdigest() and Path(latch['output']).resolve()==path(root,previous),'消費済resume223結合')
 require(st['status']=='pipeline-stopped' and st['prior_status']=='prepare-observing' and st['events']==[] and st['planned_runtime_restarts']==0 and st['boot_written'] is False and st['gpt_written'] is False and not st.get('terminal') and not st.get('prepare_crc_committed_released'),'resume223でnormal/arm未開始必須')
 raw=pin(previous+'prepare-uart.log','eba7f22c83e94eb8b26cd9b50d945f080332f65fee1dbb3284e708ef5845f2d4').read_bytes()
 sp=importlib.util.spec_from_file_location('rebind_engine',Path(__file__).with_name('run-gpu368-fresh200.py'));engine=importlib.util.module_from_spec(sp);sp.loader.exec_module(engine)
 decoder_path=pin(seal['crc_decoder']);require(sha(decoder_path)==next(x['sha256'] for x in seal['pinned_files'] if x['path']==seal['crc_decoder']),'元resume CRCdecoder pin')
 sp=importlib.util.spec_from_file_location('rebind_journal_crc',decoder_path);dm=importlib.util.module_from_spec(sp);sp.loader.exec_module(dm)
 report=dm.decode(re.sub(engine.STORAGE_TRACE,b'',raw),seal['prepare_token'])
 require(report==pin(previous+'prepare-report-storage-deinterleaved.txt','a17149ed04686f18fcc9bff744299dfe7b32f69b197266e3c0dcd6c03fa6ac05').read_bytes(),'resume223 CRC report不一致')
 lines=report.decode('ascii').splitlines()
 for line in ('STATE=BASELINE_REJECTED_NO_CHANGE','UPDATE_STATUS=JOURNAL_EXISTS_NO_CHANGE','UPDATE_RELEASE_EXIT=0','UPDATE_EXIT=4'):require(lines.count(line)==1,'既知journal拒否境界のみ')
 require(not any('COMMITTED' in v for v in lines),'COMMITTED後の再試行不可')
 fm=read(pin(MF%224,JOURNAL_MF));prior=read(path(root,MF%223))
 require(fm['status']=='captured-unmounted' and fm['read_only'] is True and fm['unmounted'] is True and fm['errors']==[] and fm['recovery_boot_id']=='21115fbe-6271-4497-bd1b-b14632aaf176','実Recovery224固定')
 for k in ('serial','source_partition','source_partition_start_sectors','source_partition_size_sectors','source_partition_major_minor','missing_files'):require(fm[k]==prior[k],'別partition/端末/欠落集合不可')
 table={r['source_path']:r for r in fm['files']};before={r['source_path']:r for r in prior['files']};require(set(table)==set(before) and len(table)==len(fm['files'])==42,'224全42files必須')
 for name,row in table.items():
  leaf=pin((Path(MF%224).parent/row['backup_path']).as_posix(),row['sha256']);require(leaf.stat().st_size==row['bytes'] and row['sha256']==before[name]['sha256'] and row['bytes']==before[name]['bytes'],'223→224依存差異: '+name)
 jp='port/xnu/out/private-windows-rt-journal224/'
 jm=read(pin(jp+'manifest.json'))
 require(jm['status']=='captured-unmounted' and jm['read_only'] is True and jm['unmounted'] is True and not jm.get('errors',[]) and jm['fresh_manifest_sha256']==JOURNAL_MF and jm['recovery_boot_id']==fm['recovery_boot_id'],'journal同Recovery224結合')
 require({r['path'] for r in jm['files']}=={'journal/JOURNAL.txt','journal/prepare-report.txt'} and len(jm['files'])==2,'journal224固定2files')
 for row in jm['files']:
  leaf=pin(jp+row['path'],row['sha256']);require(leaf.stat().st_size==row['bytes'],'journal size不一致')
 journal=pin(jp+'journal/JOURNAL.txt',JOURNAL_SHA);require(journal.read_bytes()==b'STATE=BASELINE_REJECTED_NO_CHANGE\r\n','実journal strict拒否state')
 journal_report=pin(jp+'journal/prepare-report.txt','ecc732ca665fc1f9be04c3380339209beba9a7719e8bd3bfeab359815b3278b7')
 require(report.startswith(journal_report.read_bytes()) and journal_report.read_bytes().endswith(b'UPDATE_STATUS=JOURNAL_EXISTS_NO_CHANGE\r\n'),'journal保存reportとUART CRC結合')
 token=side['policy_sha256'][:16]
 require(jm['remote_sha_listing']==JOURNAL_SHA+'  a04backup/peripheral-batch-'+token+'/JOURNAL.txt\n'+'ecc732ca665fc1f9be04c3380339209beba9a7719e8bd3bfeab359815b3278b7  a04backup/peripheral-batch-'+token+'/prepare-report.txt','journal当該token出自')
 side.update(schema=JOURNAL_SCHEMA,resume_capture_number=224,fresh_manifest=MF%224,fresh_manifest_sha256=JOURNAL_MF,recovery_boot_id=fm['recovery_boot_id'],previous_resume_seal_sha256=sealsha,identical_to223_files=42,journal=dict(path=journal.relative_to(root).as_posix(),sha256=JOURNAL_SHA),journal_token=token,refs=[dict(path=k,sha256=v) for k,v in sorted(refs.items())])
 return side

def validate(root,ref,c):
 sidepath=path(root,ref['path']);require(sha(sidepath)==ref['sha256'],'rebind sidecar pin');side=read(sidepath);require(side['schema'] in (SCHEMA,JOURNAL_SCHEMA),'既知223/224 sidecarのみ');is224=side['schema']==JOURNAL_SCHEMA;require(side==evaluate(root,is224),'拒否/現依存証拠が変更された');require(c.get('baseline_reject_resume_capture',224 if is224 else 223)==(224 if is224 else 223),'seal/sidecar suffix結合')
 require(c['policy_sha256']==side['policy_sha256'] and c['fresh_manifest_sha256']==OLD_MF and c['boot_write_required'] is False,'元policy/fresh221/boot0保持')
 media=read(path(root,c['media_directory']+'/manifest.json'))
 require(media['service_source_commit']==SOURCE and media['tooling_only_changes_since_signed_service'] is True,'media/controller sourceと実C922署名sourceを分離')
 require(c['esp_sha256']!=read(path(root,BASE+'windows-rt-explicit-source375/main-deployment-contract.json'))['esp_sha256'],'拒否媒体そのものの再実行禁止')
 if is224:
  journal=media['rejected_journal_archive']
  require(journal is not None and journal['captured_journal']==side['journal'] and journal['token']==side['journal_token'] and journal['delete_allowed'] is False and journal['replace_existing_archive_allowed'] is False,'実224 journal archive opt-in必須')
  previous=read(path(root,BASE+'windows-rt-explicit-source375/main-deployment-contract-resume223.json'))
  require(c['esp_sha256']!=previous['esp_sha256'],'journal拒否済み媒体そのものの再実行禁止')
 return side

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--after-journal224',action='store_true');ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();root=a.workspace.resolve(strict=True);out=a.out.resolve();require(out.is_relative_to(root) and not out.exists(),'root内新規sidecarのみ');value=evaluate(root,a.after_journal224)
 with out.open('x') as f:json.dump(value,f,indent=2);f.write('\n')
 print(json.dumps(dict(path=out.relative_to(root).as_posix(),sha256=sha(out))))
if __name__=='__main__':main()
