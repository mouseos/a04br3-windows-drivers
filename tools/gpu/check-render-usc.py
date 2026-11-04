#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare private PCO fill-pattern traces; never export shader bytes."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import sys

def load_module(name,file):
    spec=importlib.util.spec_from_file_location(name,pathlib.Path(__file__).with_name(file))
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def links(events):
    compilers=[];uploads={};out=[];last=None
    for e in events:
        op=e['event']
        if op=='pco_binary':compilers.append(e)
        elif op in ('upload','pds_construct'):
            addr=e['gpu_va'];size=e['size']
            uploads={va:u for va,u in uploads.items() if not (addr<va+u[0] and va<addr+size)}
        elif op=='usc_upload_hash':
            if not last or last['event']!='upload' or last['kind']!='usc_code' or last['gpu_va']!=e['gpu_va'] or last['size']!=e['size']:
                raise ValueError('upload hash not adjacent to upload')
            # Do not associate a prior compiler result with future uploads.
            matches={(c['stage'],c['internal']) for c in compilers if c['size']==e['size'] and c['fnv64']==e['fnv64']}
            uploads[e['gpu_va']]=(e['size'],e['fnv64'],matches)
        elif op in ('pds_shader','fragment_shader_link'):
            addr=e['shader_gpu'];entry=e['entry_gpu'];stage=0 if op=='pds_shader' else 4
            if addr not in uploads:raise ValueError('shader upload not observed')
            size,digest,matches=uploads[addr]
            if (stage,0) not in matches or not addr<=entry<addr+size:
                raise ValueError('compiler or entry mismatch')
            if size != (312 if stage==0 else 360):raise ValueError('unsupported fixed shader size')
            out.append((stage,size,digest,entry-addr))
        elif op in ('vm_unmap','vm_destroy'):
            # Ambiguous lifetime: no stale upload can satisfy a later link.
            uploads={}
        last=e
    if [x[0] for x in out] != [0,4,0,4]:raise ValueError('unexpected fixed probe shader order')
    return out

def inspect(root, fill):
    root=pathlib.Path(root);m=json.loads((root/'run-manifest.json').read_text());d=m.get('diagnostic_manifest') or {}
    data=(root/'events.jsonl').read_bytes()
    if m.get('pco_fill')!=fill or not d.get('usc_compiler_metadata_only') or hashlib.sha256(data).hexdigest()!=m['events_sha256']:
        raise ValueError('invalid PCO capture')
    report=load_module('trace','check-render-trace.py').check(root/'events.jsonl')
    if report['pds_shader_unmapped'] or report['fragment_unmapped'] or report['fragment_missing_uploads'] or report['uploads_ambiguous_vm']:
        raise ValueError('unmapped shader dependency')
    load_module('fragment','check-render-fragment.py').check(root)
    return m,links(map(json.loads,data.splitlines()))

def compare(zero,ones):
    a,aa=inspect(zero,'00');b,bb=inspect(ones,'ff')
    for k in ('probe_commit','probe_source_sha256','shader_header_sha256','harness_sha256'):
        if a[k]!=b[k]:raise ValueError('probe provenance mismatch')
    for k in ('mesa_commit','library_sha256','pco_source_sha256','pco_object_sha256'):
        if a['diagnostic_manifest'][k]!=b['diagnostic_manifest'][k]:raise ValueError('compiler provenance mismatch')
    if aa!=bb:raise ValueError('PCO output depends on fill pattern')
    return {'vertex_links':2,'fragment_links':2,'vertex_bytes':312,'fragment_bytes':360,
            'gpu_execution_allowed':False,'limitations':['FNV is not collision-resistant',
            'matching two fill values does not prove every emitted bit or immediate is safe',
            'indirect shader references, runtime writes and complete job lifetime remain unverified']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('zero');p.add_argument('ones');a=p.parse_args()
    try: print(json.dumps(compare(a.zero,a.ones),indent=2))
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):
        print('invalid or unsupported USC comparison',file=sys.stderr);return 2
    return 1
if __name__=='__main__':sys.exit(main())
