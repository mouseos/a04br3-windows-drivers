"""Fixed INF-only 3.12 -> 3.13 transaction; SYS/UMD/FW bytes never change."""
import csv,hashlib,json,re
from pathlib import Path
from private_hardware_probe_input import sha
SOURCE='b27cd0b37701607e82d87342f185e9a154e41f87'
MANIFEST='9ef6d89b900c089e754fa57f4a8ceaa557f4c7d22b69e4e0bbea9666b3c0cb11'
BINARY_SOURCE='9f31ddcf89fbe6d5c3a7b5abcceadb223212d378'
CERT='BCBF6C456C6F0DD0818434763B189488FBF13570'
def load_render313(entry,root,retained_files):
 from codec_msdc_transaction import version,datevalue
 if set(entry)!={'directory','manifest_sha256','source_commit'} or entry['source_commit']!=SOURCE or entry['manifest_sha256']!=MANIFEST:raise ValueError('main正式render3.13 source/manifest pinsが必要です')
 p=Path(entry['directory']).resolve(strict=True)
 if not p.is_relative_to(root.resolve()):raise ValueError('root外render package')
 mf=p/'signed-manifest.json';m=json.loads(mf.read_text(encoding='utf-8-sig'))
 if sha(mf)!=MANIFEST or m.get('package_source_commit')!=SOURCE or m.get('signer_thumbprint')!=CERT or m.get('signature_status')!='Valid':raise ValueError('render3.13署名出自不一致')
 registration=m.get('inf_only_registration',{})
 if registration.get('baseline_source_commit')!=BINARY_SOURCE or registration.get('compiled') is not False or registration.get('resigned_images') is not False or registration.get('d3d_name_slots')!=3:raise ValueError('INF-only/MULTI_SZ 3slots契約不一致')
 names=['ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','ge8300-mips.fw','ge8300-umd.dll']
 outputs=m['outputs']
 if {x['name'] for x in outputs}!=set(names+['mt8167-development.cer']) or len(outputs)!=6:raise ValueError('render3.13 artifacts不一致')
 for f in outputs:
  if sha(p/f['name'])!=f['sha256'].lower():raise ValueError('render3.13 artifact hash不一致')
 for name in names[3:]+[names[1]]:
  if sha(p/name)!=retained_files[name]:raise ValueError('render SYS/DLL/FW byte変更禁止')
 if sha(p/'linked-manifest.json')!=m['linked_manifest_sha256'].lower():raise ValueError('baseline linked出自不一致')
 der=(p/'mt8167-development.cer').read_bytes()
 if hashlib.sha1(der).hexdigest().upper()!=CERT:raise ValueError('render cert不一致')
 data=(p/names[0]).read_bytes();text=data.decode('utf-16') if data.startswith(b'\xff\xfe') else data.decode('utf-8-sig')
 if not re.search(r'(?im)^DriverVer\s*=\s*11/07/2026,0\.0\.3\.13\s*$',text) or not re.search(r'(?im)^AddService\s*=\s*GE8300Render\s*,',text) or 'ACPI\\MOU0006' not in text:raise ValueError('render3.13 INF tuple不一致')
 for key,expected in [('UserModeDriverName','ge8300-umd.dll'),('InstalledDisplayDrivers','ge8300-umd')]:
  lines=[x for x in text.splitlines() if re.match(r'(?i)^HKR,,\s*'+key+r'\s*,',x)]
  if len(lines)!=1:raise ValueError('一意のrender MULTI_SZが必要です')
  row=next(csv.reader(lines))[3:]
  if int(row[0],0)!=0x10000 or row[1:]!=[expected]*3:raise ValueError('同DLLのD3D11 third MULTI_SZ slot不一致')
 files={'render/'+n:sha(p/n) for n in names}
 component={'source_commit':SOURCE,'wrapper_source_commit':SOURCE,'binary_source_commit':BINARY_SOURCE,'driver_ver':'11/07/2026,0.0.3.13','version_u64':version('0.0.3.13'),'date_filetime':datevalue('11/07/2026'),'certificate_thumbprint':CERT,'signed_manifest_sha256':MANIFEST,'registration_only':True,'files':files}
 return {'component':component,'selected':{**{'render/'+n:p/n for n in names},'render313-development.cer':p/'mt8167-development.cer'},'certificate':der}
