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
if origins['schema']!='ge8300-fullsource327-origins-v1':raise ValueError('Source origins schema')
for row in origins['files']+origins['legacy_files_preserved']:
 if sha(file(row['path']))!=row['sha256']:raise ValueError('Source changed: '+row['path'])
native=json.loads(file('tools/ci/ge8300-native-source-map.json'))
if native['native_owner_source_commit']!=origins['native_source_commit']:raise ValueError('Native provenance mismatch')
# Exactly reviewed325 overlays, with immutable baseline origin retained.
# This does not admit arbitrary native edits or a new code-generation recipe.
reviewed={'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_d3d_viewport.c': {'source_commit': '1e4ca5d11701a809653c518e07b75aa6c8cfb409', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_d3d_viewport.c', 'sha256': '53cd50bcf05bebeeae496509b9a99c3c2d94a1899b3770e9f355a16b79906982', 'baseline_destination_sha256': 'fa8c0660378e1b5a5d352c8e39102c4cf6a46ff0fb072536132a66b2eb98dae1', 'role': 'PPP offset-scale order325'}, 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_d3d_viewport.h': {'source_commit': '1e4ca5d11701a809653c518e07b75aa6c8cfb409', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_d3d_viewport.h', 'sha256': '8b5b68f0c9aa4fd338ff6eb801a18d52df7498b638183aaa25cedc6e98467c3a', 'baseline_destination_sha256': '12efe5e335074bd6debf70e75afe319afc794bf9252da5bb926dcbad6146c3b7', 'role': 'PPP offset-scale order325'}, 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_draw_job.c': {'source_commit': 'f118dfdce3a58f85b22230526e0154c044270813', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_draw_job.c', 'sha256': '04843a5ac72c3ba668cf570663ce55ef1dad1c8b9fbc147572d2a003dbd4fda3', 'baseline_destination_sha256': 'e51bc1bfde053d806cf9af1652e08197f2067b30d0b6696501efa7d3bf4c6251', 'role': 'Texture326 binding + source filter327'}, 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_draw_job.h': {'source_commit': 'f118dfdce3a58f85b22230526e0154c044270813', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_draw_job.h', 'sha256': 'fa64d2171297c0631615291b111f5320a95ad8489123409d1b6d8d944b3ae91a', 'baseline_destination_sha256': 'd9c11d66989d5b755d1ee53fc1319b1c3ffa0318308b9eb0f9a76afd41c1f8fa', 'role': 'Texture326 binding + source filter327'}, 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_owner_factory.c': {'source_commit': 'f118dfdce3a58f85b22230526e0154c044270813', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_owner_factory.c', 'sha256': 'def61c5ab2e387e2db0f688f8c2c4716e460ada6f2bc491d6ebec0c4579935b7', 'baseline_destination_sha256': '783cfdacadd26b622df834e52b7e4464287dc898e1832dea849647eafdd8ba7c', 'role': 'Texture326 binding + source filter327'}, 'drivers/gpu/mt8167/vendor_m1_11/render_owner/windows_native_cold.h': {'source_commit': 'f118dfdce3a58f85b22230526e0154c044270813', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/windows_native_cold.h', 'sha256': '06566d36982c69e76fa770c9af7494d15064830988f13c8fe7caa4bdeb114fea', 'baseline_destination_sha256': 'e6a7687f177e320ebcfe7f712dc263fef0192bcdb861a33dfc04789e4a79eb7b', 'role': 'Texture326 binding + source filter327'}, 'drivers/gpu/mt8167/vendor_m1_11/render_owner/windows_native_job.c': {'source_commit': 'f118dfdce3a58f85b22230526e0154c044270813', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/windows_native_job.c', 'sha256': 'e91d8765c38c19961b8522b7e1bd0d721f19d54b02614d95dd50a9e01f7bfb9d', 'baseline_destination_sha256': '88412ddcd5dcdfe7a2f76e5b73bee1fe495b65aa02f4097275fd2531470ed94e', 'role': 'Texture326 binding + source filter327'}, 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_state.c': {'source_commit': 'f118dfdce3a58f85b22230526e0154c044270813', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_state.c', 'sha256': '23628eb4b7542f6ba65f672ff039ddee0552774bee1f0abfbf42ef91ff08d246', 'baseline_destination_sha256': None, 'role': 'New source texture implementation326/327', 'new_source': True}, 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_state.h': {'source_commit': 'f118dfdce3a58f85b22230526e0154c044270813', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_state.h', 'sha256': '6537d24a5a9f8af464cf8f6571cd781d5b38569f5f46f3fadb6afd96e80d4b9f', 'baseline_destination_sha256': None, 'role': 'New source texture implementation326/327', 'new_source': True}, 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_program.c': {'source_commit': 'f118dfdce3a58f85b22230526e0154c044270813', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_program.c', 'sha256': '5d8c52b7058ad726876ab72555911f8ab505fc3f494f166ea6392bbaddd4f611', 'baseline_destination_sha256': None, 'role': 'New source texture implementation326/327', 'new_source': True}, 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_program.h': {'source_commit': 'f118dfdce3a58f85b22230526e0154c044270813', 'source_path': 'drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_program.h', 'sha256': '00f51f0ebcd346448c00522b17271f841e8e15f667411fa4353e0baa8f59bfcc', 'baseline_destination_sha256': None, 'role': 'New source texture implementation326/327', 'new_source': True}}
if origins.get('reviewed_native_deltas')!=list(reviewed.values()):raise ValueError('Reviewed native overlay set changed')
for row in native['files']:
 data=file(row['destination'])
 if sha(data)!=row['sha256']:raise ValueError('Scoped native source changed: '+row['destination'])
 if 'reviewed_delta' in row:
  d=reviewed.get(row['destination'])
  if row['reviewed_delta']!=d or row['sha256']!=d['sha256']:raise ValueError('Unreviewed native overlay')
  if row['source_commit']!=(d['source_commit'] if d.get('new_source') else origins['native_source_commit']):raise ValueError('Native origin lost')
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
if plan['prebuilt_objects_or_libraries_included'] or len(plan['units'])!=372:raise ValueError('Compiler must be the source-built372TU closure')
for row in plan['files']:
 if sha(file(row['path']))!=row['sha256']:raise ValueError('Compiler snapshot changed: '+row['path'])
if sha(file('tests/fixtures/tokenized_shader_before323.c'))!='c838cb69ea91369084b1d8b4a8ea313d981f7b35925c9b8bc12a97dc6a16890e':raise ValueError('Pre323 fixture changed')
print(json.dumps({'schema':'ge8300-fullsource327-source-verification-v1','reviewed_native_overlays':len(reviewed),'viewport_source_commit':origins['viewport_source_commit'],'origin_files':len(origins['files']),'legacy_files':len(origins['legacy_files_preserved']),'native_scoped_files':len(native['files']),'native_units':len(units),'compiler_snapshot_files':len(plan['files']),'compiler_units':len(plan['units']),'old_git_history_used':False,'driver_cache_used':False,'hardware_verified':False}))
