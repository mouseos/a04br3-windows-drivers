#!/usr/bin/env python3
"""main用の固定USB1121 SCM入力を生成。Windows/端末操作なし。"""
import argparse,hashlib,importlib.util,json,shutil,zipfile
from pathlib import Path
from types import SimpleNamespace
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('fresh198',HERE/'prepare-usb1121-fresh198-bundle.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
ZIP='e37b36fc550b170b3d233cd7585121f72df309f7e29f7325cd67c2566829e6b1'
MANIFEST='656ab72fda2cd79518aa47ddabdcd008db86673c675e83ccbb7875073b501064'
DLL='b0c3585525ab406218e03eaa06a3e453db4c92796898baa72bd7bc54786474ba'
SERVICE='MT8167Usb1121Install198'
def prepare(root,out):
 root=root.resolve(strict=True);out=out.resolve();b.require(out.is_relative_to(root) and not out.exists(),'root内新規出力必須')
 zpath=b.below(root,'port/xnu/out/windows-rt-usb1121-helper42796448681/usb1121-helper-signed.zip');b.require(b.sha(zpath)==ZIP,'signed ZIP drift')
 with zipfile.ZipFile(zpath) as z:
  b.require(z.testzip() is None and set(z.namelist())=={'MT8167UsbAbi3Pnp.dll','helper-manifest.json','build-plan.json','build-receipt.json'},'signed ZIP members/CRC')
  mh=hashlib.sha256(z.read('helper-manifest.json')).hexdigest();dh=hashlib.sha256(z.read('MT8167UsbAbi3Pnp.dll')).hexdigest()
  b.require(mh==MANIFEST and dh==DLL,'signed helper drift')
  m=json.loads(z.read('helper-manifest.json'));b.require(m['frozen_helper_source_commit']=='90e4132b7d7e62e9629d1bd768e5c09ad063c517' and m['fixed_recipe_source_commit']=='1a753dccd604641e767e03748418c8a5c5c9d352' and m['certificate_status']=='Valid','helper provenance')
  plan=b.prepare(root,out/'payload/bundle')
  hp=out/'payload/helper';hp.mkdir()
  for name in z.namelist():(hp/name).write_bytes(z.read(name))
 b.require(m['source_sha256']==plan['files']['recipe/UsbAbi3Pnp.cs'],'unchanged C# source')
 payload=out/'payload';launch=HERE.parents[1]/'tools/usb/run-usb1121-service.ps1';shutil.copyfile(launch,payload/launch.name)
 fm_path=b.below(root,'port/xnu/out/private-windows-rt-prebackup-run198/manifest.json');fm=b.read(fm_path)
 fresh={x['source_path']:x for x in fm['files']}
 # All captured originals are checked offline; only SYSTEM will be replaced in media.
 for f in fm['files']:b.require(b.sha(b.below(root,fm_path.parent/f['backup_path']))==f['sha256'],'fresh original drift')
 # Exact actual GPU367 package, UMD/FW and old transport witnesses from main197 proof.
 prior=b.read(b.below(root,'port/xnu/out/windows-rt-combined197-fresh196/service/peripheral-policy.json'))
 retained={k:dict(path=(fm_path.parent/f['backup_path']).relative_to(root).as_posix(),sha256=f['sha256']) for k,f in fresh.items() if k.startswith('Windows/System32/') and '/config/' not in k and not k.endswith('.log')}
 for e in prior['packages']['render']['extra_members']:
  name='Windows/System32/'+e['installed'].replace('\\','/');q=b.below(root,Path(prior['packages']['render']['directory'])/e['name']);b.require(b.sha(q)==e['sha256'],'actual367 UMD/FW source drift');retained[name]=dict(path=q.relative_to(root).as_posix(),sha256=e['sha256'])
 b.require(fresh['Windows/System32/Drivers/ge8300-render.sys']['sha256']==prior['packages']['render']['hashes']['sys'],'actual SYS367 mismatch')
 files={p.relative_to(payload).as_posix():b.sha(p) for p in payload.rglob('*') if p.is_file()}
 profile=dict(schema='MT8167_USB1121_FRESH198_SERVICE_V1',service=SERVICE,fresh_sha256=b.FRESH,recovery_boot_id=b.BOOT,native_receipt_sha256=b.NATIVE,plan_sha256=b.sha(payload/'bundle/plan.json'),helper_manifest_sha256=MANIFEST,helper_build_plan_sha256=m['install_plan_sha256'],helper_build_plan_is_historical_only=True,files=files,retained_files={k:v['sha256'] for k,v in retained.items() if Path(k).name != Path(plan['rollback']['parent']['names']['sys']).name},retained_sources=retained,source_ON=False,force_host=False,root_PDO_creation=False,driver_rebuilt=False,helper_rebuilt=False,SDK_rebuilt=False,boot_GPT_changes=0,old_service_changes=0,POST_value=0,hardware_applied=False,power_verified=False)
 (payload/'profile.json').write_text(json.dumps(profile,indent=2)+'\n')
 header='/* Generated exact input pins, no runtime role override. */\n'
 for name,h in [('launcher',b.sha(payload/launch.name)),('profile',b.sha(payload/'profile.json'))]:header+='static const BYTE '+name+'_hash[32]={'+','.join('0x'+h[i:i+2] for i in range(0,64,2))+'};\n'
 (out/'usb1121-service-pins.h').write_text(header)
 for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2'):
  f=fresh['Windows/System32/config/'+n];d=out/'fresh-system'/n;d.parent.mkdir(exist_ok=True);shutil.copyfile(b.below(root,fm_path.parent/f['backup_path']),d)
 closure={p.relative_to(out).as_posix():b.sha(p) for p in out.rglob('*') if p.is_file()}
 (out/'input-closure.json').write_text(json.dumps(dict(schema='MT8167_USB1121_SERVICE_INPUTS_V1',files=closure,profile_sha256=b.sha(payload/'profile.json'),fresh_sha256=b.FRESH,native_sha256=b.NATIVE,signed_helper_zip_sha256=ZIP),indent=2)+'\n')
 return profile
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();prepare(a.workspace,a.out);print('USB1121_FRESH198_INPUTS_PREPARED_NO_HARDWARE')
