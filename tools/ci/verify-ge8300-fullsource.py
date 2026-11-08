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
if origins['schema']!='ge8300-fullsource323-origins-v1':raise ValueError('Source origins schema')
for row in origins['files']+origins['legacy_files_preserved']:
 if sha(file(row['path']))!=row['sha256']:raise ValueError('Source changed: '+row['path'])
native=json.loads(file('tools/ci/ge8300-native-source-map.json'))
if native['native_owner_source_commit']!=origins['native_source_commit']:raise ValueError('Native provenance mismatch')
for row in native['files']:
 data=file(row['destination'])
 if sha(data)!=row['sha256']:raise ValueError('Scoped native source changed: '+row['destination'])
 replacements=iter(row['include_rewrites'])
 def inverse(match):
  old,new=next(replacements)
  if match.group(1).decode()!=new:raise ValueError('Unreviewed include rewrite')
  return match.group(0).replace(match.group(1),old.encode(),1)
 original=re.sub(rb'^\s*#include\s+"([^"\r\n]+)"',inverse,data,flags=re.M)
 if list(replacements) or sha(original)!=row['source_sha256']:raise ValueError('Non-include native change')
 if row['source_commit']!=origins['native_source_commit']:raise ValueError('Mixed native source')
for unit,path in native['native_unit_sources'].items():file(path)
plan_bytes=file('third_party/mesa-pco/source-plan.json')
if sha(plan_bytes)!=origins['compiler_source_plan_sha256']:raise ValueError('Compiler plan changed')
plan=json.loads(plan_bytes)
if plan['prebuilt_objects_or_libraries_included'] or len(plan['units'])!=370:raise ValueError('Compiler must be the source-built370TU closure')
for row in plan['files']:
 if sha(file(row['path']))!=row['sha256']:raise ValueError('Compiler snapshot changed: '+row['path'])
if sha(file('tests/fixtures/tokenized_shader_before323.c'))!='c838cb69ea91369084b1d8b4a8ea313d981f7b35925c9b8bc12a97dc6a16890e':raise ValueError('Pre323 fixture changed')
print(json.dumps({'schema':'ge8300-fullsource323-source-verification-v1','origin_files':len(origins['files']),'legacy_files':len(origins['legacy_files_preserved']),'native_scoped_files':len(native['files']),'compiler_snapshot_files':len(plan['files']),'compiler_units':len(plan['units']),'old_git_history_used':False,'driver_cache_used':False,'hardware_verified':False}))
