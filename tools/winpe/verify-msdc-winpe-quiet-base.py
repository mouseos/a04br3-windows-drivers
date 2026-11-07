#!/usr/bin/env python3
"""生成済みquiet WinPEとoriginalの変更範囲を再照合する。実機アクセス無し。"""
import argparse,hashlib,importlib.util,json,os,struct,subprocess,sys
from pathlib import Path
from preparation_base_input import select_preparation_base,QUIET_MANIFEST,RECEIPT

def main():
 ap=argparse.ArgumentParser()
 for n in ('workspace','original','quiet','package','membership-receipt','tools','output'):ap.add_argument('--'+n,type=Path,required=True)
 a=ap.parse_args();root=a.workspace.resolve(strict=True);out=a.output.resolve()
 source=Path(__file__).resolve().parents[2]
 if subprocess.check_output(['git','-C',str(source),'status','--porcelain']):raise ValueError('Committed clean verifier source required')
 for n in ('original','quiet','package','membership_receipt','tools'):
  p=getattr(a,n).resolve(strict=True)
  if not p.is_relative_to(root) or p==root:raise ValueError('root内入力が必要')
  setattr(a,n,p)
 if not out.is_relative_to(root) or out.exists():raise ValueError('root内新出力が必要')
 os.umask(0o077);out.mkdir();os.environ['TMPDIR']=str(out)
 spec=importlib.util.spec_from_file_location('quiet_builder',Path(__file__).with_name('build-msdc-winpe-quiet-base.py'));b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
 sys.path[:0]=[str(a.tools/'python-fat'),str(a.tools/'python-hive')]
 from pyfatfs.PyFatFS import PyFatFS
 from regipy.registry import RegistryHive
 original=select_preparation_base(a.original,root)
 selected=select_preparation_base(a.quiet,root,a.quiet/'manifest.json',QUIET_MANIFEST,a.membership_receipt,True)
 m=b.read(a.quiet/'manifest.json');package=b.read(a.package/'signed-manifest.json')
 if b.sha(a.package/'signed-manifest.json')!=m['package_manifest_sha256']:raise ValueError('package manifest pin mismatch')
 if package.get('driver_recompiled') is not False or package.get('driver_resigned') is not False or package.get('error_init_checkpoint_trace_retained') is not True:raise ValueError('Logs-only signed package contract mismatch')
 for item in package['files']:
  if b.sha(a.package/item['path'])!=item['sha256']:raise ValueError('Signed package file mismatch')
 tool=a.tools/'wimtools/wimlib-imagex';old=a.original/'maintenance-staged.wim';new=a.quiet/'maintenance-staged.wim'
 env=dict(os.environ,LD_LIBRARY_PATH=str(a.tools/'wimtools'),PYTHONDONTWRITEBYTECODE='1')
 def run(cmd,**kw):return subprocess.run(list(map(str,cmd)),env=env,check=True,**kw)
 # inventory() uses subprocess inheritance; keep library lookup within task tools.
 os.environ['LD_LIBRARY_PATH']=env['LD_LIBRARY_PATH']
 run([tool,'verify',new])
 before=b.inventory(tool,old);after=b.inventory(tool,new)
 changes=set(m['replaced_wim_paths'])
 if set(before)!=set(after) or any(after[n]!=v for n,v in before.items() if n not in changes):raise ValueError('Unrelated WIM metadata/security/stream changed')
 for i,name in enumerate(sorted(changes)):
  d=out/('extract-'+str(i));d.mkdir();run([tool,'extract',new,'1',name,'--dest-dir='+str(d)],stdout=subprocess.DEVNULL)
  if name.startswith('/a04br3/') and b.sha(d/Path(name).name)!=m['flat_driver_files'][Path(name).name]:raise ValueError('flat WIM member hash mismatch')
  if name.endswith('/SOFTWARE'):final_hive=d/'SOFTWARE'
 d=out/'original-software';d.mkdir();run([tool,'extract',old,'1','/Windows/System32/config/SOFTWARE','--dest-dir='+str(d)],stdout=subprocess.DEVNULL)
 cert=(a.package/'msdc-development.cer').read_bytes()
 blob=struct.pack('<III',3,1,20)+hashlib.sha1(cert).digest()+struct.pack('<III',32,1,len(cert))+cert
 for store in ('Root','TrustedPublisher'):
  oldstore=b.store_snapshot(RegistryHive(str(d/'SOFTWARE')),store);newstore=b.store_snapshot(RegistryHive(str(final_hive)),store)
  entry=newstore.pop('\\'+b.THUMB,None)
  if entry!=(('Blob','REG_BINARY',repr(blob.hex())),) or newstore!=oldstore:raise ValueError('Certificate type/byte or prior store changed')
 with PyFatFS(str(a.original/'maintenance-staged-esp.img'),read_only=True,preserve_case=True) as fat:oldfat=b.fat_files(fat)
 with PyFatFS(str(a.quiet/'maintenance-staged-esp.img'),read_only=True,preserve_case=True) as fat:newfat=b.fat_files(fat)
 if set(oldfat)!=set(newfat) or any(newfat[n]!=(selected['wim_sha256'] if n=='/sources/boot.wim' else h) for n,h in oldfat.items()):raise ValueError('Unrelated ESP file changed')
 run(['fsck.vfat','-n',a.quiet/'maintenance-staged-esp.img'])
 # Old WinPE2371 and reused quiet1.8 have identical transfer algorithms/regs.
 source=Path(__file__).resolve().parents[2];algorithms={}
 for name in ('dma.c','dma.h','pio.c','pio.h','cdb.c','scsi.c','scsi_capacity.c','gpt_guard.c','io_gate.h'):
  path='drivers/storage/mt8167-msdc/'+name
  first=subprocess.check_output(['git','-C',str(source),'show','2371bd09aa6f90b60f4affeaef83553451713eb1:'+path])
  second=subprocess.check_output(['git','-C',str(source),'show','e2db926f1ddd12b4cae341a7df50f832e20c16e5:'+path])
  if first!=second:raise ValueError('DMA/PIO/control contract changed: '+path)
  algorithms[path]=hashlib.sha256(first).hexdigest()
 result={'verifier_sha256':b.sha(Path(__file__)),'source_dirty':False,'status':'verified-generated-private-base','source_commit':subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD']).decode().strip(),'original':original,'quiet':selected,'membership_receipt_sha256':RECEIPT,'changed_wim_paths':sorted(changes),'unrelated_wim_records_preserved':True,'all_other_esp_files_preserved':True,'prior_certificate_stores_preserved':True,'system_hive_preserved':True,'dma_pio_control_sources_unchanged':algorithms,'driver_recompiled':False,'driver_resigned':False,'hardware_operations':False,'performance_change_measured':False,'native_os_system_bound':False}
 (out/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 print('QUIET_WINPE_BASE_REAL_ARTIFACT_VERIFIED')
if __name__=='__main__':main()
