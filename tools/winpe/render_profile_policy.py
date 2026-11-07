#!/usr/bin/env python3
"""明示opt-inの固定Render/CPU-DOD/SPM profile。旧System GPU policyは不変。"""
import argparse, datetime, hashlib, json, re, subprocess
from pathlib import Path
from engine_io_package import require_armnt

SIGNER='BCBF6C456C6F0DD0818434763B189488FBF13570'
FRESH54='521124fdab3b7c3759ee790b76afca18e4988123e6dadc0f5c11e9958d59a1d2'
FW='5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1'
PARENT='287241c37fa0eae538e5e2d136dea5df42de7e43'
PARENT1015='13ae9f85c37331a3dd7324278e932956b9a840ef'
BOOT='6e852b1ff13e16609957afc7a2e03da3d371dfe8'
DOD='6a99ec7d444180e150208d508dfaf203eb734d77'
BASE_SYS='a8f226b33fa11915e88b3424c5ad3ed3aaa62ff35deb7f91902b04e7df4bf090'
TARGETS={
 'parent':('ACPI\\BEN0001','MT8167Shared','{4D36E97D-E325-11CE-BFC1-08002BE10318}',('shared-parent.inf','mt8167-shared.sys','shared-parent.cat'), 'drivers/bus/mt8167-shared/prototype/shared-parent-engine-pcm-gpu-sd.inf'),
 'display':('ACPI\\MOU0007','MT8167Display','{4D36E968-E325-11CE-BFC1-08002BE10318}',('mt8167-display.inf','mt8167-display.sys','mt8167-display.cat'),'drivers/gpu/mt8167/wddm-display/mt8167-display.inf'),
 'render':('ACPI\\MOU0006','GE8300Render','{4D36E968-E325-11CE-BFC1-08002BE10318}',('ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','ge8300-mips.fw','ge8300-umd.dll'),'drivers/gpu/mt8167/wddm-render/ge8300-render.inf'),
}
def require(v,msg):
 if not v:raise ValueError(msg)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def hexpin(v,n=64):
 require(isinstance(v,str) and re.fullmatch('[0-9a-fA-F]{'+str(n)+'}',v),'不正hex pin');return v.lower()
def rooted(root,p):
 p=Path(p);require(not p.is_symlink(),'symlink入力禁止');p=p.resolve(strict=True);require(p.is_relative_to(root),'root外入力禁止');return p
def jsonfile(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def textinf(p):
 b=p.read_bytes();return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig').replace('\r\n','\n')
def version(v):
 require(isinstance(v,str) and re.fullmatch(r'\d{2}/\d{2}/\d{4},\d+\.\d+\.\d+\.\d+',v),'固定DriverVer形式不一致')
 date,parts=v.split(',');nums=tuple(map(int,parts.split('.')));require(max(nums)<=65535,'版範囲不正')
 dt=datetime.datetime.strptime(date,'%m/%d/%Y');ticks=int((dt-datetime.datetime(1601,1,1)).total_seconds())*10000000
 number=0
 for n in nums:number=(number<<16)|n
 return number,ticks

def package(root,entry,role,repo,render_color=False):
 require(set(entry)=={'directory','manifest_sha256','source_commit'},'component field不一致: '+role)
 d=rooted(root,entry['directory']);p=d/'signed-manifest.json';require(sha(p)==hexpin(entry['manifest_sha256']),'署名manifest pin不一致')
 m=jsonfile(p);source=hexpin(entry['source_commit'],40)
 require(m.get('source_commit',m.get('package_source_commit'))==source,'source pin不一致')
 require(m.get('signed') is not False and m.get('source_dirty') is not True,'未署名/dirty package拒否')
 entries=m.get('files',m.get('outputs'));require(isinstance(entries,list),'固定file manifestが必要')
 files={}
 for e in entries:
  n=e.get('path',e.get('name'));require(isinstance(n,str) and re.fullmatch(r'[A-Za-z0-9_.-]+',n) and n not in files,'flat固定fileのみ')
  f=rooted(root,d/n);require(f.parent==d and f.is_file() and 0<f.stat().st_size<=16777216,'package file範囲不正')
  require(sha(f)==hexpin(e['sha256']),'package hash不一致: '+n);files[n]=sha(f)
 names=TARGETS[role][3];require(set(names).issubset(files) and 'mt8167-development.cer' in files,'固定package不足')
 require(hashlib.sha1((d/'mt8167-development.cer').read_bytes()).hexdigest().upper()==SIGNER,'固定private入力cert不一致')
 for n in names:
  if n.endswith(('.sys','.dll')):require_armnt(d/n)
 inf=textinf(d/names[0]);inf_path='drivers/bus/mt8167-shared/prototype/shared-parent-engine-pcm-gpu-sd-rtc1015.inf' if role=='parent' and source==PARENT1015 else TARGETS[role][4];raw=subprocess.check_output(['git','-C',str(repo),'show',source+':'+inf_path])
 committed=raw.decode('utf-16' if raw[:2]==b'\xff\xfe' else 'utf-8-sig').replace('\r\n','\n');require(inf==committed,'実INFとsource不一致')
 v=re.search(r'(?im)^DriverVer\s*=\s*([^\n;]+)',inf);require(v,'DriverVer欠落');v=v[1].strip();num,date=version(v)
 for field,wanted in [('ClassGuid',TARGETS[role][2]),('ServiceBinary','%12%\\'+names[1])]:
  found=re.search(r'(?im)^'+field+r'\s*=\s*([^\n;]+)',inf);require(found and found[1].strip().lower()==wanted.lower(),'固定INF '+field+'不一致')
 require(TARGETS[role][0].lower() in inf.lower() and re.search(r'(?im)^AddService\s*=\s*'+TARGETS[role][1]+r'\s*,',inf),'固定ID/service不一致')
 require(re.search(r'(?im)^\[Install\.NT(?:ARM)?\]',inf),'ARM DDInstall装飾不足')
 require(not re.search(r'(?i)\b(CoInstallers32|RegisterDlls|UnregisterDlls|RunPreSetupCommands|RunPostSetupCommands)\b',inf),'実行性INF拡張拒否')
 if role=='parent':
  require((source,v) in ((PARENT,'11/07/2026,2026.11.7.1014'),(PARENT1015,'11/07/2026,2026.11.7.1015')),'固定SD1/SPM親1014/1015以外拒否')
  cap=jsonfile(d/'parent-manifest.json');require('parent-manifest.json' in files,'親capability非hash')
  flags=cap['flags'];require(flags.get('A04_SHARED_SD_POWER')=='1' and flags.get('A04_SHARED_GPU_DOMAIN_SPM')=='1' and flags.get('A04_SHARED_GPU_PLATFORM')=='1','SPM/SD1必須')
  baseline=json.loads(subprocess.check_output(['git','-C',str(repo),'show',source+':drivers/bus/mt8167-shared/prototype/gpu-parent1014-spm-profile.json']))
  if source==PARENT1015:
   require(files[names[1]]=='e7df942265a8216ba393152f1fe781e47d48ebe09b6da02f235dd8c2ba7d9752' and flags==dict(baseline['flags'],A04_SHARED_WIFI_RTC_BORROW='1'),'Fixed parent1015 bytes and unchanged GPU profile required')
  # Exact flags are recorded, and the source profile pins the 1010 baseline.
  require(all(flags.get(k)=='1' for k in ('A04_SHARED_TOUCH_D0_EXPERIMENT','A04_SHARED_BUTTON_D0_EXPERIMENT','A04_SHARED_ENGINE_PCM','A04_SHARED_WIFI_RUNTIME','A04_AUDIO_CHILD_RESOURCES')) and flags.get('A04_SHARED_THERMAL_DIAGNOSTIC')=='0','通常OS保持flags不一致')
 elif role=='display':
  require(source==DOD and v=='11/07/2026,0.0.1.6' and files[names[1]]=='8a2cf298b782acad669cf95b6dd81ced062761e1e9ecd5d095316dedfff3a4b0','CPU DOD1.6固定pin不一致')
  flags=m.get('effective_flags',{});require(flags and flags.get('A04_MT8167_DISPLAY_BRIGHTNESS')==1 and all(value==0 for key,value in flags.items() if key!='A04_MT8167_DISPLAY_BRIGHTNESS'),'CPU present専用flags不一致')
 elif render_color:
  require(role=='render' and (source,v) in (('4f55794471a4008c1ceaa3a1da2679c0f3fefd2a','11/07/2026,0.0.3.3'),('0406393650f68c6c4d704f4e6e40e58225d5b8d3','11/07/2026,0.0.3.4'),('f06f5398b55791505946f1640a1c90c7e5b1ed1f','11/07/2026,0.0.3.5'),('c63ec476e9bbcef3ec04576a58317df40de93564','11/07/2026,0.0.3.6')),'固定COLOR3.3 source/version不一致')
  require(files['ge8300-mips.fw']==FW and m.get('default_off') is True and m.get('signature_status')=='Valid','COLOR署名/FW不一致')
  require(m.get('required_parent_commit')==PARENT and m.get('required_acpi_commit')==BOOT,'COLOR matched親/UEFI不一致')
  linked=jsonfile(d/'linked-manifest.json');require(sha(d/'linked-manifest.json')==hexpin(m['linked_manifest_sha256']) and linked==m.get('source_runtime'),'COLOR linked provenance不一致')
  require(linked.get('native_source_commit')=='e83792d88485883130e137309b0ef027b3c7bc5e' and linked.get('limited_development_profile') is True and linked.get('standard_profile_qualified') is False,'COLOR native/profile不一致')
  if v in ('11/07/2026,0.0.3.4','11/07/2026,0.0.3.5','11/07/2026,0.0.3.6'):
   require(files['ge8300-umd.dll']=='f8a711915e878e7aeb58ef996db8abbeee15351bd10bf8e0fdcabe12009f922b' and m.get('umd_resigned') is False,'UMD signed bytes must remain unchanged')
   require('LoadOrderGroup=Video' in inf and linked.get('initialization_checkpoint'),'Real initialization status evidence and Video group required')
  if v=='11/07/2026,0.0.3.5':
   require(linked.get('linker',{}).get('name')=='Microsoft LINK' and linked.get('kernel_source_commit')=='0406393650f68c6c4d704f4e6e40e58225d5b8d3' and linked.get('compilation_performed') is False,'MSVC linker and unchanged kernel archive required')
  if v=='11/07/2026,0.0.3.6':
   require(linked.get('linker',{}).get('name')=='Microsoft LINK' and linked.get('additional_compile_units')==['miniport'] and files['ge8300-render.sys']=='4caec0e7ef7714db918745cd43de41dfc83920128fd96ccf9de491b53433b116','Fixed zero-output base callback miniport required')
  flags={}
 else:
  require(v=='11/07/2026,0.0.3.2' and files['ge8300-mips.fw']==FW and m.get('default_off') is True,'render3.2/FW/default-off不一致')
  require(m.get('required_parent_commit')==PARENT and m.get('required_acpi_commit')==BOOT and m.get('signature_status')=='Valid','render matched依存/署名不一致')
  baseline=jsonfile(d/'baseline-signed-manifest.json')
  require(sha(d/'baseline-signed-manifest.json')==hexpin(m['baseline_manifest_sha256']),'0f8署名baseline provenance不一致')
  require(baseline.get('package_source_commit')=='0f8f68d252820443b988b93ae5c2448fdc9a7d00' and m.get('compiled') is False and m.get('resigned_images') is False,'SYS/DLL再生成拒否')
  require(sha(d/'linked-manifest.json')==hexpin(baseline['linked_manifest_sha256']) and jsonfile(d/'linked-manifest.json')==m.get('source_runtime'),'linked provenance不一致')
  original={e['name']:e['sha256'].lower() for e in baseline['outputs']}
  require(all(files[n]==original[n] for n in ('ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')),'0f8実署名bytes変更拒否')
  flags={}
 return {'directory':d.relative_to(root).as_posix(),'manifest_sha256':sha(p),'source_commit':source,'driver_ver':v,'version_u64':num,'date_filetime':date,'files':{n:files[n] for n in names},'effective_flags':flags,'certificate_sha256':files['mt8167-development.cer']}

def load_map(path,root):
 root=root.resolve(strict=True);spec=jsonfile(rooted(root,path));repo=Path(__file__).resolve().parents[2]
 require(set(spec)=={'schema','profile','backup','packages','boot','client'} and spec['schema']==1 and spec['profile']=='offline-render-diagnostic-v1','固定Render profile必須')
 backup=rooted(root,spec['backup']);require(backup.name=='manifest.json' and backup.parent.name=='private-windows-rt-prebackup-run54' and sha(backup)==FRESH54,'fresh54唯一gate不一致')
 b=jsonfile(backup);require(b.get('status')=='captured-unmounted','fresh54未完了')
 source={x['source_path'].replace('\\','/').lower():x for x in b['files']};base=source['windows/system32/drivers/mt8167-ge8300.sys'];require(base['sha256']==BASE_SYS,'旧2.5実SYS不一致')
 for e in b['files']:
  p=rooted(root,backup.parent/e['backup_path']);require(sha(p)==e['sha256'] and p.stat().st_size==e['bytes'],'fresh54file不一致')
 require(set(spec['packages'])==set(TARGETS),'固定3package必須');packages={k:package(root,spec['packages'][k],k,repo) for k in TARGETS}
 boot=rooted(root,spec['boot']);bm=jsonfile(boot);require(bm.get('uefi_source_commit')==BOOT,'matched boot6e852b1必須')
 image=rooted(root,boot.parent/'boot.img');require(sha(image)==bm.get('boot_img_sha256'),'boot image hash不一致')
 c=spec['client'];require(set(c)=={'directory','manifest_sha256','source_commit'},'固定client入力fields不一致');cd=rooted(root,c['directory']);cm=cd/'signed-client-manifest.json';require(sha(cm)==hexpin(c['manifest_sha256']),'client署名manifest pin不一致');cr=jsonfile(cm)
 require(cr.get('signature_status')=='Valid' and cr.get('signer_thumbprint')==SIGNER and cr.get('standard_api_only') is True and cr.get('private_payload') is False,'標準署名client必須')
 build=cd/'build-manifest.json';require(sha(build)==hexpin(cr['build_manifest_sha256']),'client build manifest不一致');cb=jsonfile(build);cs=hexpin(c['source_commit'],40);require(cb.get('source_commit')==cs,'client fullsource不一致')
 committed=subprocess.check_output(['git','-C',str(repo),'show',cs+':drivers/gpu/mt8167/wddm-render/testclient/d3d-clear.c']);require(b'PowerVR' in committed and b'hardware=' in committed,'新固定auto選択clientが必要（旧0f8付属clientは不可）')
 exe=cd/'ge8300-d3d-clear.exe';require(sha(exe)==hexpin(cr['signed_sha256']),'client EXE不一致');require_armnt(exe)
 policy={'schema':1,'profile':spec['profile'],'default_off':True,'backup_manifest_sha256':FRESH54,'backup_path':backup.relative_to(root).as_posix(),'packages':packages,'client':{'directory':cd.relative_to(root).as_posix(),'source_commit':cs,'sha256':sha(exe),'manifest_sha256':sha(cm),'arguments':'--hardware=PowerVR','timeout_ms':120000},'boot':{'source_commit':BOOT,'manifest_sha256':sha(boot),'sha256':sha(image),'path':image.relative_to(root).as_posix()},'sequence':['WinPE copy native clean SYSTEM before three DISM AddDriver operations','release report0 exit0','main Recovery writes matched boot6e852b1 and normal ESP','normal service validates exact selected versions and SYS/FW/UMD then fixed client once'],'normal_install_allowed':False,'retained_gpu_baseline_sha256':BASE_SYS}
 return policy

def cbytes(h):return '{'+','.join('0x'+h[i:i+2] for i in range(0,64,2))+'}'
def generate(p):
 token=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()
 lines=['/* Fixed Render-only signed-service policy. */','#define DEFERRED_TARGET_COUNT 3','#define DEFERRED_GPU_UPDATE 0','#define DEFERRED_SD_UPDATE 0','#define DEFERRED_BRIGHTNESS_SCOPED_LOG 0','#define RENDER_OFFLINE_DIAGNOSTIC 1','static const struct deferred_pin deferred_pins[3]={']
 for k in TARGETS:
  v=p['packages'][k];hashes=list(v['files'].values())+['0'*64]*(5-len(v['files']));lines.append('{'+str(v['version_u64'])+'ULL,'+str(v['date_filetime'])+'ULL,{'+','.join(cbytes(h) for h in hashes)+'}},')
 num,date=version('11/07/2026,0.0.2.5');lines+=['};','static const struct deferred_pin deferred_render_baseline={'+str(num)+'ULL,'+str(date)+'ULL,{{0},'+cbytes(BASE_SYS)+'}};','static const char deferred_token[]="'+token+'";','static const BYTE deferred_client_hashes[1][32]={'+cbytes(p['client']['sha256'])+'};']
 return '\n'.join(lines)+'\n'
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--enable-render-profile',action='store_true');ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--package-map',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();require(a.enable_render_profile,'明示opt-in必須');p=load_map(a.package_map,a.workspace);out=a.output.resolve();require(out.is_relative_to(a.workspace.resolve()) and not out.exists(),'新規root内output必須');out.mkdir(parents=True);(out/'render-deferred-policy.h').write_text(generate(p));(out/'render-policy-manifest.json').write_text(json.dumps(p,indent=2)+'\n')
if __name__=='__main__':main()
