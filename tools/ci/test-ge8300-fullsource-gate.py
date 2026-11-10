#!/usr/bin/env python3
"""Exercise wrong-source build-map rejection in an isolated source-only tree."""
import argparse,json,os,shutil,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];out=a.output.resolve()
if not out.is_relative_to(root) or out.exists():raise ValueError('New checkout-local output required')
out.mkdir(parents=True);fixture=out/'fixture'
origins=json.loads((root/'tools/ci/ge8300-fullsource-origins.json').read_text())
native=json.loads((root/'tools/ci/ge8300-native-source-map.json').read_text())
plan=json.loads((root/'third_party/mesa-pco/source-plan.json').read_text())
paths={'tools/ci/build-ge8300-from-source.ps1','tools/ci/build-ge8300-native-owner-core.ps1','tools/ci/ge8300-copy-usc-asset.json','tools/ci/ge8300-fullsource-origins.json','tools/ci/ge8300-native-source-map.json','tools/ci/verify-ge8300-fullsource.py','third_party/mesa-pco/source-plan.json','tests/fixtures/tokenized_shader_before323.c'}
paths.update(r['path'] for r in origins['files']+origins['legacy_files_preserved']+plan['files'])
paths.update(r['destination'] for r in native['files'])
for name in sorted(paths):
 source=root/name;dest=fixture/name;dest.parent.mkdir(parents=True,exist_ok=True)
 # Mutable metadata must never share an inode with production source.
 if name.endswith('.json'):shutil.copyfile(source,dest)
 else:
  try:os.link(source,dest)
  except OSError:shutil.copyfile(source,dest)
command=[sys.executable,'-B',str(fixture/'tools/ci/verify-ge8300-fullsource.py')]
records=[]
def run(name,expected):
 result=subprocess.run(command,text=True,capture_output=True)
 if (result.returncode==0)!=expected:raise AssertionError((name,result.returncode,result.stdout,result.stderr))
 records.append({'case':name,'accepted':result.returncode==0,'expected_accepted':expected})
run('canonical source-only closure',True)
mapfile=fixture/'tools/ci/ge8300-native-source-map.json'
changed=json.loads(json.dumps(native));unit='vendor_m1_11\\native_owner_factory_runtime'
legacy_unit='vendor_m1_11\\windows_stop'
legacy_path='drivers/gpu/mt8167/vendor_m1_11/windows_stop.c'
if not (fixture/legacy_path).is_file():raise AssertionError('Legacy source must exist: exercise redirection, not a missing-file rejection')
changed['native_unit_sources'][legacy_unit]=legacy_path
mapfile.write_text(json.dumps(changed))
run('redirect a native TU to the old legacy vendor file while every source hash stays valid',False)
changed=json.loads(json.dumps(native));del changed['native_unit_sources']['vendor_m1_11\\windows_stop']
mapfile.write_text(json.dumps(changed));run('drop reset helper from compilation map',False)
changed=json.loads(json.dumps(native));changed['native_unit_sources'][unit]=changed['native_unit_sources']['vendor_m1_11\\windows_stop']
mapfile.write_text(json.dumps(changed));run('alias one source unit to another reviewed but wrong native source',False)
mapfile.write_text(json.dumps(native));run('restored canonical mapping',True)
# Overlay origin is exact, not a generic hash-refresh bypass.
origfile=fixture/'tools/ci/ge8300-fullsource-origins.json'
changed_orig=json.loads(json.dumps(origins));changed_orig['reviewed_native_deltas'][0]['source_commit']='f96948533601f9fdcdcc22b9700aacd96dbf7204'
origfile.write_text(json.dumps(changed_orig));run('misattribute viewport325 to old native322 origin',False)
origfile.write_text(json.dumps(origins))
changed=json.loads(json.dumps(native))
for row in changed['files']:
 if row['destination'].endswith('/native_d3d_viewport.c'):
  del row['reviewed_delta']
mapfile.write_text(json.dumps(changed));run('remove reviewed viewport overlay while retaining changed source',False)
mapfile.write_text(json.dumps(native));run('restored exact325 origin and map',True)
changed=json.loads(json.dumps(native))
for row in changed['files']:
 if row['destination'].endswith('/native_texture2d_state.c'):del row['reviewed_delta']
mapfile.write_text(json.dumps(changed));run('remove explicit texture overlay provenance',False)
mapfile.write_text(json.dumps(native));run('restore exact327 overlay set',True)

# Recipe omission must be caught even when declared source hashes remain valid.
recipefile=fixture/'tools/ci/build-ge8300-native-owner-core.ps1'
original_recipe=recipefile.read_text();recipefile.unlink()
recipefile.write_text(original_recipe.replace("'vendor_m1_11\\windows_native_copy_transfer',",''))
run('omit copy transfer from normal native recipe while map remains complete',False)
recipefile.write_text(original_recipe)
assetfile=fixture/'tools/ci/ge8300-copy-usc-asset.json';asset=json.loads(assetfile.read_text());asset['bytes'][0]^=1;assetfile.write_text(json.dumps(asset))
run('change fixed copy USC while preserving old claimed hash',False)
assetfile.write_bytes((root/'tools/ci/ge8300-copy-usc-asset.json').read_bytes())
run('restored complete native67 recipe and exact copy USC asset',True)

# New shared integration preserves the complete old origin row, not just a SHA.
changed_orig=json.loads(json.dumps(origins))
shared_path='drivers/gpu/mt8167/wddm-render/clear_engine.c'
for row in changed_orig['files']:
 if row['path']==shared_path:row['previous_origins'][0]['role']='history silently relabelled'
origfile.write_text(json.dumps(changed_orig));run('alter preserved shared-source old origin history',False)
origfile.write_text(json.dumps(origins))
fullrecipefile=fixture/'tools/ci/build-ge8300-from-source.ps1'
original_full=fullrecipefile.read_text();fullrecipefile.unlink()
fullrecipefile.write_text(original_full.replace('-WithMdlTarget -WithOwnerAllocation','-ReuseVerifiedDirectory old-layout-cache -WithMdlTarget -WithOwnerAllocation'))
run('reuse old internal-layout native objects in fullsource recipe',False)
fullrecipefile.write_text(original_full);run('restored fresh shared layout recipe and old history',True)

receipt={'schema':'ge8300-fullsource-map-regression-v1','production_source_modified':False,'driver_cache_used':False,'hardware_operations':False,'cases':records}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt))
