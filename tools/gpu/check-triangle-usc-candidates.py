#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare private Mesa compiler outputs; never export shader bytes or addresses."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import sys

EXPECTED=(('0-0.bin',0,312),('4-1.bin',4,360),('0-2.bin',0,312),('4-3.bin',4,360))
def fnv(data):
    h=14695981039346656037
    for b in data:h=((h^b)*1099511628211)&((1<<64)-1)
    return h

def compare(zero,ones):
    root=pathlib.Path(__file__).parent
    spec=importlib.util.spec_from_file_location('usc_check',root/'check-render-usc.py')
    checker=importlib.util.module_from_spec(spec);spec.loader.exec_module(checker)
    report=checker.compare(zero,ones)
    expected=tuple((name,stage,report['vertex_bytes'] if stage==0 else size) for name,stage,size in EXPECTED)
    captures=[]
    for path in (zero,ones):
        path=pathlib.Path(path);m=json.loads((path/'run-manifest.json').read_text())
        if not (m.get('diagnostic_manifest') or {}).get('private_compiler_candidates'):raise ValueError('not private compiler capture')
        recorded=m.get('private_compiler_sha256') or {}
        if set(recorded)!={x[0] for x in expected}:raise ValueError('unexpected candidate list')
        actual={p.name for p in (path/'private-compiler-candidates').iterdir()}
        if actual!=set(recorded):raise ValueError('unexpected candidate directory')
        events=[json.loads(x) for x in (path/'events.jsonl').read_bytes().splitlines()]
        pco=[x for x in events if x['event']=='pco_binary' and x['internal']==0 and x['stage'] in (0,4)]
        if len(pco)!=4:raise ValueError('unexpected compiler output count')
        files=[]
        for (name,stage,size),event in zip(expected,pco):
            p=path/'private-compiler-candidates'/name
            if p.is_symlink() or not p.is_file():raise ValueError('invalid candidate file')
            data=p.read_bytes();digest=hashlib.sha256(data).hexdigest()
            if len(data)!=size or digest!=recorded[name] or event['stage']!=stage or event['size']!=size or event['fnv64']!=fnv(data):
                raise ValueError('compiler output mismatch')
            files.append(data)
        captures.append(files)
    if captures[0]!=captures[1]:raise ValueError('compiler output differs across fill patterns')
    if captures[0][0]!=captures[0][2] or captures[0][1]!=captures[0][3]:raise ValueError('draw shaders differ')
    return {'compiler_outputs':4,'vertex_bytes':report['vertex_bytes'],'fragment_bytes':360,'gpu_execution_allowed':False,
            'limitations':['candidate files are private and may contain unverified immediates',
                           'dynamic scratch accesses and full shader semantics are not validated',
                           'matching output is not proof of safe execution or complete reference ownership']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('zero');p.add_argument('ones');a=p.parse_args()
    try:print(json.dumps(compare(a.zero,a.ones),indent=2));return 1
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):
        print('invalid private USC candidates',file=sys.stderr);return 2
if __name__=='__main__':sys.exit(main())
