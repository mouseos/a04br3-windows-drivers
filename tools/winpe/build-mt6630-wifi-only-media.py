#!/usr/bin/env python3
"""Existing maintenance base + final signed WiFi1029 triple; host-only builder."""
import argparse,hashlib,json,os,re,shutil,subprocess
from pathlib import Path
BASE_WIM='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'
BASE_ESP='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce'
BASE_BCD='65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'
SIGNER='BCBF6C456C6F0DD0818434763B189488FBF13570'
NAMES=tuple('mt6630-combo-runtime.'+ext for ext in ('inf','sys','cat'))
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def require(ok,msg):
 if not ok:raise ValueError(msg)
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def rooted(root,p):
 p=p.resolve(strict=True);require(p.is_relative_to(root) and not any(c in str(p) for c in '\r\n"'),'root外/不正path');return p
def backup_check(root,path,digest,boot_id):
 require(path.parent.name=='private-windows-rt-prebackup-run206','配備基点は実fresh206のみ。fresh205流用禁止')
 require(sha(path)==digest,'fresh206 manifest pin不一致');b=read(path)
 require(b.get('schema_version')==1 and b.get('status')=='captured-unmounted' and b.get('unmounted') is True and b.get('read_only') is True and b.get('errors')==[],'完了した実Recovery backup必須')
 require(re.fullmatch('[0-9a-f-]{36}',boot_id) and b.get('recovery_boot_id')==boot_id,'same Recovery boot ID不一致')
 require(b.get('serial')=='0C0U265083','対象serialは0C0U265083のみ')
 seen=set()
 for row in b['files']:
  name=row['source_path'];require(name not in seen and row['backup_path']=='files/'+name,'backup path重複/不正');seen.add(name)
  p=rooted(root,path.parent/row['backup_path']);require(p.is_relative_to(path.parent) and p.is_file() and p.stat().st_size==row['bytes'] and sha(p)==row['sha256'],'backup実体不一致')
 required={'Windows/System32/config/'+h+s for h in ('SYSTEM','SOFTWARE','DRIVERS')for s in ('','.LOG1','.LOG2')}
 required.add('Windows/System32/Drivers/mt6630-combo-runtime.sys')
 require(required<=seen,'rollback原本/hive/log欠落');return b

def package_check(root,directory,manifest,digest):
 require(manifest.is_relative_to(directory) and sha(manifest)==digest,'署名receipt pin不一致');m=read(manifest)
 require(m.get('signer_thumbprint',m.get('signing_thumbprint'))==SIGNER and m.get('signed') is not False,'既BCBF最終署名のみ')
 require(m.get('signature_status')=='Valid','Singer正式native署名receiptのValid必須')
 require(re.fullmatch('[0-9a-f]{40}',m.get('source_commit','')),'package固定source欠落')
 rows={}
 for row in m['files']:
  name=row['path'];require(name not in rows,'package receipt重複');rows[name]=row
 hashes={}
 for name in NAMES:
  require(name in rows,'署名三file欠落');p=rooted(root,directory/name)
  require(p.parent==directory and p.stat().st_size>0 and sha(p)==rows[name]['sha256'],'署名三file hash不一致');hashes[name]=sha(p)
 raw_inf=(directory/NAMES[0]).read_bytes();inf=raw_inf.decode('utf-16' if raw_inf[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig')
 require(re.search(r'(?im)^Class\s*=\s*Net\s*$',inf) and re.search(r'(?im)^CatalogFile\s*=\s*mt6630-combo-runtime\.cat\s*$',inf),'Net/CAT不一致')
 version=re.search(r'(?im)^DriverVer\s*=\s*([^\r\n]+)',inf)
 require(version and re.fullmatch(r'\d{2}/\d{2}/\d{4},\s*\d+\.\d+\.\d+\.1029',version[1].strip()),'WiFi1029 INF以外')
 require('A04BR3\\MT6630SDIO' in inf and 'mt6630-combo-runtime.sys' in inf and re.search(r'(?im)^AddService\s*=\s*Mt6630Runtime,',inf),'WiFi hardware/service不一致')
 return m,hashes

def render(here,token,backup_sha,boot_id):
 text=(here/'run-mt6630-wifi-only-update.cmd.in').read_text()
 for key,value in {'TOKEN':token,'HOST_BACKUP_SHA':backup_sha,'RECOVERY_BOOT_ID':boot_id}.items():text=text.replace('@'+key+'@',value)
 require(not re.search('@[A-Z_]+@',text),'未置換cmd');return text.replace('\r','').replace('\n','\r\n').encode('ascii')

def main(argv=None):
 ap=argparse.ArgumentParser(description=__doc__)
 for n in ('workspace','base','package','package-manifest','backup','tools','out'):ap.add_argument('--'+n,type=Path,required=True)
 for n in ('source-commit','package-manifest-sha256','backup-manifest-sha256','recovery-boot-id'):ap.add_argument('--'+n,required=True)
 ap.add_argument('--check-only',action='store_true');a=ap.parse_args(argv)
 root=a.workspace.resolve(strict=True);here=Path(__file__).resolve().parent;repo=here.parents[1]
 require(repo.is_relative_to(root),'source root外')
 require(subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()==a.source_commit and not subprocess.check_output(['git','-C',str(repo),'status','--porcelain'],text=True).strip(),'固定clean source必須')
 base,package,pm,backup,tools=(rooted(root,p)for p in (a.base,a.package,a.package_manifest,a.backup,a.tools))
 out=a.out.resolve();require(out.is_relative_to(root) and not out.exists(),'root内の新規出力のみ')
 require(sha(base/'maintenance-staged.wim')==BASE_WIM and sha(base/'maintenance-staged-esp.img')==BASE_ESP,'既maintenance base identity不一致')
 b=backup_check(root,backup,a.backup_manifest_sha256,a.recovery_boot_id);p,hashes=package_check(root,package,pm,a.package_manifest_sha256)
 token=hashlib.sha256(json.dumps({'files':hashes,'backup':sha(backup),'template':sha(here/'run-mt6630-wifi-only-update.cmd.in')},sort_keys=True).encode()).hexdigest()[:16].upper()
 if a.check_only:print('WIFI1029_INPUTS_VERIFIED_NO_MEDIA_NO_DEVICE');return
 out.mkdir(mode=0o700);stage=out/'stage';stage.mkdir();dest=stage/'wifi-only';dest.mkdir()
 for name in NAMES:shutil.copyfile(package/name,dest/name);require(sha(dest/name)==hashes[name],'staging中のpackage変更')
 (stage/'run-update-once.cmd').write_bytes(render(here,token,sha(backup),a.recovery_boot_id))
 (stage/'startnet.cmd').write_bytes(b'@echo off\r\nmountvol /N\r\nwpeinit\r\nif errorlevel 1 exit /b 2\r\nmountvol /N\r\ncall X:\\a04br3\\run-update-once.cmd\r\n')
 image=out/'update.wim';shutil.copyfile(base/'maintenance-staged.wim',image)
 env=os.environ.copy();env.update(TMPDIR=str(stage),LD_LIBRARY_PATH=str(tools/'wimtools'),PYTHONPATH=str(tools/'python-fat'),PYTHONDONTWRITEBYTECODE='1')
 pairs=[(stage/'startnet.cmd','/Windows/System32/startnet.cmd'),(stage/'run-update-once.cmd','/a04br3/run-update-once.cmd'),(dest,'/a04br3/wifi-only')]
 commands=''.join('add "'+str(src)+'" '+dst+'\n' for src,dst in pairs)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'update',str(image),'1'],input=commands,text=True,env=env,check=True)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'verify',str(image)],env=env,check=True)
 subprocess.run(['python3','-B',str(here/'replace-esp-wim.py'),'--template',str(base/'maintenance-staged-esp.img'),'--wim',str(image),'--output',str(out/'update-esp.img'),'--template-bcd-sha256',BASE_BCD,'--template-wim-sha256',BASE_WIM],env=env,check=True)
 require(sha(backup)==a.backup_manifest_sha256 and sha(pm)==a.package_manifest_sha256,'媒体生成中に入力manifestが変更された')
 files={'wifi-only/'+n:sha(dest/n) for n in NAMES};files.update({n:sha(stage/n)for n in ('startnet.cmd','run-update-once.cmd')})
 # Controller must perform this existing read-only preflight in the SAME
 # Recovery immediately before deployment. Builder does not operate a device.
 preflight=['python3','-B',str(repo/'tools/recovery/capture-windows-prebackup.py'),'verify','--serial',b['serial'],'--manifest',str(backup),'--manifest-sha256',sha(backup)]
 manifest=dict(schema='MT6630_WIFI1029_OFFLINE_ONLY_V1',status='generated-not-device-verified',source_commit=a.source_commit,package_source_commit=p['source_commit'],package_manifest_sha256=sha(pm),signer_thumbprint=SIGNER,signature_status=p['signature_status'],catalog_membership_evidence='main独立照合のSinger実verify recipeログ; native manifestに未定義のboolは要求/合成しない',files=files,token=token,esp_sha256=sha(out/'update-esp.img'),wim_sha256=sha(image),host_backup_manifest_sha256=sha(backup),recovery_boot_id=a.recovery_boot_id,serial=b['serial'],required_same_recovery_preflight=preflight,base_identity=dict(wim_sha256=BASE_WIM,esp_sha256=BASE_ESP,bcd_sha256=BASE_BCD),install_targets=['wifi'],diagnostic_clients=[],trust_added=False,force_unsigned_PE_servicing=True,runtime_CI_changed=False,firmware_changed=False,parent_changed=False,BT_changed=False,boot_written=False,gpt_written=False,automatic_reboot=False,post_update='release/CRC gate; main restores normal ESP then normal Windows hold',hardware_verified=False,builder_sha256=sha(Path(__file__)),command_sha256=files['run-update-once.cmd'])
 (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps({'esp_sha256':manifest['esp_sha256'],'token':token,'host_backup_manifest_sha256':sha(backup)},indent=2))
if __name__=='__main__':main()
