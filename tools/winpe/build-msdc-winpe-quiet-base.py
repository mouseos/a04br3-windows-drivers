#!/usr/bin/env python3
"""固定WinPE baseのflat MSDC packageだけを差し替えるprivate派生。実機アクセスなし。"""
import argparse,hashlib,json,os,re,shutil,ssl,struct,subprocess,sys
from pathlib import Path
BASE_WIM='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'
BASE_ESP='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce'
BCD='65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'
PACKAGE_MANIFEST='11c68361ebc405d81a1b32820813964f6471880787fd863a2327a777b6a73fcd'
PACKAGE_SOURCE='91ee3ce6f773f97d8c6964df1a73e2283ec610b4'
THUMB='BCBF6C456C6F0DD0818434763B189488FBF13570'
CERT_SHA='33680a69f647193e7f7adcecf137db525262019c3a8f971ab3095d40f894d72a'
OLD={'a04-msdc-probe.inf':'9bbf73d73c6dcad9f392e294a2cee7ca23209f80e4d36801e6b6a5781acb7192','mt8167-msdc.sys':'28115a3ba588d3354e30faca4b37a7de28dcc0b288c7354e2dd3b1ddf3ef416f','mt8167-msdc.cat':'e88b0514f64cdca47f5b12d6351df51518fbbf47c6253ff469609e96ef3f021c'}
NAMES=(*OLD,'observe-msdc-storage.exe','msdc-development.cer')
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def run(cmd,**kw):return subprocess.run(list(map(str,cmd)),check=True,**kw)
def inventory(tool,wim):
 text=run([tool,'dir',wim,'1','--detailed'],capture_output=True,text=True).stdout;records={}
 for block in text.split('----------------------------------------------------------------------------'):
  path=re.search(r'Full Path\s*= "([^"]+)"',block)
  if not path:continue
  name=path.group(1);attrs=re.search(r'Attributes\s*= (0x[0-9a-fA-F]+)',block);sd=re.search(r'Security Descriptor\s*= ([^\r\n]+)',block)
  if name in records or not attrs:raise ValueError('ambiguous/incomplete WIM inventory')
  records[name]=(int(attrs[1],16),sd[1] if sd else None,tuple(re.findall(r'Hash\s*= (0x[0-9a-fA-F]+)',block)))
 if not records:raise ValueError('Empty WIM inventory')
 return records
def store_snapshot(hive,store):
 out={}
 def visit(key,path):
  out[path]=tuple(sorted([(v.name,v.value_type,repr(v.value)) for v in key.iter_values(max_len=8*1024*1024)],key=lambda x:str(x[0])))
  for sub in key.iter_subkeys():visit(sub,path+'\\'+sub.name)
 visit(hive.get_key('\\Microsoft\\SystemCertificates\\'+store+'\\Certificates'),'');return out
def fat_files(fat):
 out={}
 for directory,_,files in fat.walk():
  for entry in files:
   name=directory.rstrip('/')+'/'+entry.name
   with fat.openbin(name,'r') as f:out[name.lower()]=hashlib.file_digest(f,'sha256').hexdigest()
 return out
def main():
 ap=argparse.ArgumentParser()
 for n in ('workspace','base','package','membership-receipt','tools','output'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--membership-receipt-sha256',required=True);a=ap.parse_args();root=a.workspace.resolve(strict=True)
 source=Path(__file__).resolve().parents[2]
 if run(['git','-C',source,'status','--porcelain'],capture_output=True,text=True).stdout:raise ValueError('Committed clean builder source required')
 base,pkg,receipt,tools=[getattr(a,n).resolve(strict=True) for n in ('base','package','membership_receipt','tools')];out=a.output.resolve()
 if any(not x.is_relative_to(root) or x==root for x in (base,pkg,receipt,tools,out)) or out.exists():raise ValueError('Root内入力と新private出力が必要')
 for p in (base,pkg,receipt,tools,out):
  if any(c in str(p) for c in ('"','\n','\r')):raise ValueError('Unsupported path')
 wim=base/'maintenance-staged.wim';esp=base/'maintenance-staged-esp.img'
 if sha(wim)!=BASE_WIM or sha(esp)!=BASE_ESP:raise ValueError('固定base不一致')
 if sha(pkg/'signed-manifest.json')!=PACKAGE_MANIFEST:raise ValueError('固定package manifest不一致')
 pm=read(pkg/'signed-manifest.json');vr=read(receipt)
 if (sha(receipt)!=a.membership_receipt_sha256 or vr['schema']!=1 or vr['status']!='verified' or vr['package_source_commit']!=PACKAGE_SOURCE or vr['package_manifest_sha256']!=PACKAGE_MANIFEST or vr['signer']!=THUMB or set(vr['catalog_members'])!={'mt8167-msdc.sys','a04-msdc-probe.inf'} or set(vr['signatures'])!={'mt8167-msdc.sys','mt8167-msdc.cat','observe-msdc-storage.exe'} or vr['verification_recipe_sha256']!=sha(Path(__file__).parents[1]/'ci/verify-msdc-winpe-quiet.ps1') or vr['files']!=pm['files']):raise ValueError('WDK署名/member receipt不一致')
 if pm['source_commit']!=PACKAGE_SOURCE or pm['normal_io_trace_enabled'] or pm['dma_poll_trace_enabled'] or pm['dma_logic_modified'] or pm['timeouts_modified']:raise ValueError('Logs-only fixed package required')
 pins={e['path']:e['sha256'] for e in pm['files']}
 if len(pins)!=len(pm['files']) or set(pins)!=set(NAMES):raise ValueError('固定5file package required')
 for n,h in pins.items():
  if sha(pkg/n)!=h:raise ValueError('package file pin mismatch: '+n)
 cert=(pkg/'msdc-development.cer').read_bytes()
 if sha(pkg/'msdc-development.cer')!=CERT_SHA or hashlib.sha1(cert).hexdigest().upper()!=THUMB:raise ValueError('公開certificate pin不一致')
 header=(pkg/'msdc-winpe-storage-pins.h').read_text();observer=(pkg/'observe-msdc-storage.exe').read_bytes()
 if sha(pkg/'msdc-winpe-storage-pins.h')!=pm['pin_header_sha256']:raise ValueError('header pin mismatch')
 for role,n in [('INF','a04-msdc-probe.inf'),('SYS','mt8167-msdc.sys'),('CAT','mt8167-msdc.cat')]:
  if f'#define A04_WINPE_MSDC_{role}_SHA256 "{pins[n]}"' not in header or pins[n].encode() not in observer:raise ValueError('observer embedded pin mismatch')
 os.umask(0o077);out.mkdir();scratch=out/'scratch';scratch.mkdir();os.environ['TMPDIR']=str(scratch)
 sys.path[:0]=[str(tools/'python-hive'),str(tools/'python-fat')]
 from regipy.registry import RegistryHive
 from pyfatfs.PyFatFS import PyFatFS
 tool=tools/'wimtools/wimlib-imagex';env=dict(os.environ,LD_LIBRARY_PATH=str(tools/'wimtools'),PYTHONPATH=os.pathsep.join([str(tools/'python-fat'),str(tools/'python-hive')]),PYTHONDONTWRITEBYTECODE='1')
 os.environ['LD_LIBRARY_PATH']=env['LD_LIBRARY_PATH']
 before=inventory(tool,wim)
 with PyFatFS(str(esp),read_only=True,preserve_case=True) as fat:oldfat=fat_files(fat)
 if oldfat.get('/sources/boot.wim')!=BASE_WIM or oldfat.get('/efi/microsoft/boot/bcd')!=BCD:raise ValueError('ESP embedded WIM/BCD mismatch')
 # Read only from immutable input; all registry edits are on a new host copy.
 for n,h in OLD.items():
  folder=scratch/('old-'+n);folder.mkdir();run([tool,'extract',wim,'1','/a04br3/'+n,'--dest-dir='+str(folder)],env=env,stdout=subprocess.DEVNULL)
  if sha(folder/n)!=h:raise ValueError('旧flat driver pin mismatch')
 run([tool,'extract',wim,'1','/Windows/System32/config/SOFTWARE','--dest-dir='+str(scratch)],env=env,stdout=subprocess.DEVNULL)
 original=scratch/'SOFTWARE';updated=scratch/'SOFTWARE.updated';shutil.copyfile(original,updated)
 stores=('Root','TrustedPublisher');oldstores={s:store_snapshot(RegistryHive(str(original)),s) for s in stores}
 blob=struct.pack('<III',3,1,20)+hashlib.sha1(cert).digest()+struct.pack('<III',32,1,len(cert))+cert
 commands=''
 for s in stores:
  prior=oldstores[s].get('\\'+THUMB)
  if prior is not None:raise ValueError('固定baseに予期しない既存署名者')
  commands+=f'cd \\Microsoft\\SystemCertificates\\{s}\\Certificates\nadd {THUMB}\ncd {THUMB}\nsetval 1\nBlob\nhex:3:{blob.hex()}\n'
 edited=run(['hivexsh','-w',updated],input=commands+'commit\nquit\n',text=True,capture_output=True,env=env)
 if edited.stderr:raise ValueError('Certificate hive edit diagnostics: '+edited.stderr)
 for s in stores:
  current=store_snapshot(RegistryHive(str(updated)),s);entry=current.pop('\\'+THUMB,None)
  if entry!=(('Blob','REG_BINARY',repr(blob.hex())),) or current!=oldstores[s]:raise ValueError('既存certificate store changed')
 dests={'/a04br3/'+n:pkg/n for n in NAMES};dests['/Windows/System32/config/SOFTWARE']=updated
 image=out/'maintenance-staged.wim';shutil.copyfile(wim,image)
 update=''.join(f'add "{p}" "{dst}"\n' for dst,p in dests.items())
 run([tool,'update',image,'1','--check'],input=update,text=True,env=env);run([tool,'verify',image],env=env)
 after=inventory(tool,image)
 if set(after)!=set(before) or any(after[n]!=v for n,v in before.items() if n not in dests):raise ValueError('Unrelated WIM record changed')
 for i,(dst,p) in enumerate(dests.items()):
  check=scratch/('check-'+str(i));check.mkdir();run([tool,'extract',image,'1',dst,'--dest-dir='+str(check)],env=env,stdout=subprocess.DEVNULL)
  if sha(check/Path(dst).name)!=sha(p):raise ValueError('WIM staged file mismatch')
 finalhive=scratch/'check-5/SOFTWARE'
 for s in stores:
  current=store_snapshot(RegistryHive(str(finalhive)),s);entry=current.pop('\\'+THUMB,None)
  if entry!=(('Blob','REG_BINARY',repr(blob.hex())),) or current!=oldstores[s]:raise ValueError('Final WIM certificate mismatch')
 run([sys.executable,Path(__file__).with_name('replace-esp-wim.py'),'--template',esp,'--wim',image,'--output',out/'maintenance-staged-esp.img','--template-bcd-sha256',BCD,'--template-wim-sha256',BASE_WIM],env=env)
 with PyFatFS(str(out/'maintenance-staged-esp.img'),read_only=True,preserve_case=True) as fat:newfat=fat_files(fat)
 if set(newfat)!=set(oldfat) or any(newfat[n]!=(sha(image) if n=='/sources/boot.wim' else h) for n,h in oldfat.items()):raise ValueError('Unrelated EFI/FAT file changed')
 if sha(wim)!=BASE_WIM or sha(esp)!=BASE_ESP or sha(pkg/'signed-manifest.json')!=PACKAGE_MANIFEST:raise ValueError('Frozen inputs changed')
 source=Path(__file__).resolve().parents[2];commit=run(['git','-C',source,'rev-parse','HEAD'],capture_output=True,text=True).stdout.strip()
 result={'schema':1,'private_do_not_distribute':True,'kind':'fixed-preparation-winpe-msdc-quiet-base','source_commit':commit,'source_dirty':False,'builder_sha256':sha(Path(__file__)),'parent_wim_sha256':BASE_WIM,'parent_esp_sha256':BASE_ESP,'wim_sha256':sha(image),'esp_sha256':sha(out/'maintenance-staged-esp.img'),'bcd_sha256':BCD,'package_source_commit':PACKAGE_SOURCE,'package_manifest_sha256':PACKAGE_MANIFEST,'membership_receipt_sha256':sha(receipt),'flat_driver_files':pins,'signer':THUMB,'certificate_stores':['Root','TrustedPublisher'],'existing_certificate_stores_preserved':True,'unrelated_wim_records_preserved':True,'all_other_esp_files_preserved':True,'frozen_inputs_preserved':True,'replaced_wim_paths':list(dests),'system_hive_modified':False,'driver_logic_modified':False,'dma_timeout_clock_acpi_modified':False,'hardware_verified':False,'performance_change_measured':False,'status':'generated-and-verified; hardware unverified','next_input_scope':'new base input only; existing GPU media builders retain their fixed original base gate'}
 (out/'manifest.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
