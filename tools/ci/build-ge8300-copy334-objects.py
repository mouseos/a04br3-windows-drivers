#!/usr/bin/env python3
"""Compile only new TQ/cold-storage closure with an existing pinned332 toolchain.
No active library/driver/package is replaced. No hardware claim or signing.
"""
import argparse,hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--baseline-build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
base=a.baseline_build.resolve();out=a.output.resolve()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
if not out.is_relative_to(root) or out.exists():raise ValueError('New checkout-local output required')
manifest=base/'package332/signed-manifest.json'
if sha(manifest)!='55a4d2e6c918275aeccb0364c8af823c1c4f25defd54cf1f3df9753748d753d8':raise ValueError('Pinned332 manifest required')
m=json.loads(manifest.read_text(encoding='utf-8-sig'));oldroot=base.parent.parent
f=next(x['arguments'] for x in m['compiled_units'] if x['role']=='native' and '/compiler/' not in x['source'])
f=[str(s).replace(str(oldroot),str(root)) for s in f];out.mkdir(parents=True)
units=['render_owner/native_copy_job.c','render_owner/native_copy_queue.c','render_owner/windows_native_copy_storage.c','render_owner/windows_native_copy_transfer.c','native_copy_state.c','native_copy_usc.c','native_ccb.c']
rows=[]
for unit in units:
 rel='drivers/gpu/mt8167/vendor_m1_11/'+unit;obj=out/(unit.replace('/','-')[:-2]+'.obj')
 subprocess.run(['clang',*f,'-c',str(root/rel),'-o',str(obj)],check=True)
 rows.append({'source':rel,'source_sha256':sha(root/rel),'object':obj.name,'object_sha256':sha(obj),'arguments':f})
 (out/'compiled-checkpoint.json').write_text(json.dumps(rows,indent=2)+'\n')
commit=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
(out/'manifest.json').write_text(json.dumps({'source_commit':commit,'compiled_units':rows,'baseline_manifest_sha256':sha(manifest),'runtime_connected':False,'signed':False,'hardware_verified':False,'SDK_rebuilt':False,'PCO_rebuilt':False},indent=2)+'\n')
print('COPY334_OBJECTS_COMPILED',commit,len(rows))
