#!/usr/bin/env python3
"""Verify the public source closure without old Git history or driver caches."""
import hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[2]
def sha(data):return hashlib.sha256(data).hexdigest()
def file(relative):
 p=(root/relative).resolve(strict=True)
 if not p.is_relative_to(root):raise ValueError('Source escaped checkout: '+relative)
 return p.read_bytes()
origins=json.loads(file('tools/ci/ge8300-fullsource-origins.json'))
if origins['schema']!='ge8300-fullsource325-origins-v1':raise ValueError('Source origins schema')
for row in origins['files']+origins['legacy_files_preserved']:
 if sha(file(row['path']))!=row['sha256']:raise ValueError('Source changed: '+row['path'])
native=json.loads(file('tools/ci/ge8300-native-source-map.json'))
if native['native_owner_source_commit']!=origins['native_source_commit']:raise ValueError('Native provenance mismatch')
# Exactly reviewed325 overlays, with immutable baseline origin retained.
# This does not admit arbitrary native edits or a new code-generation recipe.
reviewed={
'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_d3d_viewport.c':{'source_commit': '1e4ca5d11701a809653c518e07b75aa6c8cfb409', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_d3d_viewport.c', 'sha256': '53cd50bcf05bebeeae496509b9a99c3c2d94a1899b3770e9f355a16b79906982', 'baseline_destination_sha256': 'fa8c0660378e1b5a5d352c8e39102c4cf6a46ff0fb072536132a66b2eb98dae1', 'role': 'PPP offset-scale order325'},
'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_d3d_viewport.h':{'source_commit': '1e4ca5d11701a809653c518e07b75aa6c8cfb409', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_d3d_viewport.h', 'sha256': '8b5b68f0c9aa4fd338ff6eb801a18d52df7498b638183aaa25cedc6e98467c3a', 'baseline_destination_sha256': '12efe5e335074bd6debf70e75afe319afc794bf9252da5bb926dcbad6146c3b7', 'role': 'PPP offset-scale order325'},
}
if origins.get('reviewed_native_deltas')!=list(reviewed.values()):raise ValueError('Reviewed native overlay set changed')
for row in native['files']:
 data=file(row['destination'])
 if sha(data)!=row['sha256']:raise ValueError('Scoped native source changed: '+row['destination'])
 if 'reviewed_delta' in row:
  d=reviewed.get(row['destination'])
  if row['reviewed_delta']!=d or row['sha256']!=d['sha256']:raise ValueError('Unreviewed native overlay')
  if row['source_commit']!=origins['native_source_commit']:raise ValueError('Baseline native origin lost')
  continue
 replacements=iter(row['include_rewrites'])
 def inverse(match):
  old,new=next(replacements)
  if match.group(1).decode()!=new:raise ValueError('Unreviewed include rewrite')
  return match.group(0).replace(match.group(1),old.encode(),1)
 original=re.sub(rb'^\s*#include\s+"([^"\r\n]+)"',inverse,data,flags=re.M)
 if list(replacements) or sha(original)!=row['source_sha256']:raise ValueError('Non-include native change')
 if row['source_commit']!=origins['native_source_commit']:raise ValueError('Mixed native source')
units=native['native_unit_sources']
if len(units)!=origins['native_unit_count']:raise ValueError('Native compilation unit count changed')
if sha(json.dumps(units,sort_keys=True,separators=(',',':')).encode())!=origins['native_unit_sources_sha256']:raise ValueError('Native compilation map changed')
native_sources={row['source']:row['destination'] for row in native['files']}
pinned_external={row['path'] for row in origins['files']}
for unit,path in units.items():
 source='drivers/gpu/mt8167/'+unit.replace('\\','/')+'.c'
 if unit.startswith('vendor_m1_11\\'):
  if native_sources.get(source)!=path:raise ValueError('Native unit escaped its scoped source: '+unit)
 elif path!=source or path not in pinned_external:raise ValueError('Unpinned native external unit: '+unit)
 file(path)
plan_bytes=file('third_party/mesa-pco/source-plan.json')
if sha(plan_bytes)!=origins['compiler_source_plan_sha256']:raise ValueError('Compiler plan changed')
plan=json.loads(plan_bytes)
if plan['prebuilt_objects_or_libraries_included'] or len(plan['units'])!=370:raise ValueError('Compiler must be the source-built370TU closure')
for row in plan['files']:
 if sha(file(row['path']))!=row['sha256']:raise ValueError('Compiler snapshot changed: '+row['path'])
if sha(file('tests/fixtures/tokenized_shader_before323.c'))!='c838cb69ea91369084b1d8b4a8ea313d981f7b35925c9b8bc12a97dc6a16890e':raise ValueError('Pre323 fixture changed')
print(json.dumps({'schema':'ge8300-fullsource325-source-verification-v1','reviewed_native_overlays':len(reviewed),'viewport_source_commit':origins['viewport_source_commit'],'origin_files':len(origins['files']),'legacy_files':len(origins['legacy_files_preserved']),'native_scoped_files':len(native['files']),'native_units':len(units),'compiler_snapshot_files':len(plan['files']),'compiler_units':len(plan['units']),'old_git_history_used':False,'driver_cache_used':False,'hardware_verified':False}))
