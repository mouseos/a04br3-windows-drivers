#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate private offline context USC candidates from Mesa PCO output."""
import argparse
import hashlib
import importlib.util
import json
import os
import pathlib
import re
import struct
import subprocess
import tempfile

PIN='ef11349e8e5d620a2227fac0eed17e7ed1f86428'
NAMES=('cs_store_sr_1024_common_0_common','cs_load_sr_256_common_0_common')
def digest(path):return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
def parse(path):
    text=pathlib.Path(path).read_text();result={}
    for name in NAMES:
        match=re.search(r'static const uint32_t '+name+r'\[(\d+)\] = \{(.*?)\};',text,re.S)
        if not match:raise ValueError('missing generated symbol')
        body=match.group(2)
        if re.sub(r'0x[0-9a-fA-F]+|[\s,]','',body):raise ValueError('unexpected initializer')
        words=[int(x,16) for x in re.findall(r'0x[0-9a-fA-F]+',body)]
        if len(words)!=int(match.group(1)) or len(words)<3:raise ValueError('invalid initializer')
        meta=words[:2];raw=struct.pack('<%dI'%(len(words)-2),*words[2:])
        if (meta[1]>>8)!=len(words)-2:raise ValueError('invalid metadata length')
        result[name]=(meta,raw)
    return result
def compare_capture(root,expected):
    root=pathlib.Path(root);m=json.loads((root/'run-manifest.json').read_text());events=(root/'events.jsonl').read_bytes()
    d=m.get('diagnostic_manifest') or {}
    if (not d.get('context_component_metadata_only') or m['mesa_build_manifest']['mesa_commit']!=PIN or
        hashlib.sha256(events).hexdigest()!=m['events_sha256']):raise ValueError('capture provenance')
    spec=importlib.util.spec_from_file_location('context',pathlib.Path(__file__).with_name('check-render-context.py'))
    ctx=importlib.util.module_from_spec(spec);spec.loader.exec_module(ctx);ctx.check(root)
    rows=[json.loads(x) for x in events.splitlines()]
    def fnv(raw):
        h=14695981039346656037
        for b in raw:h=((h^b)*1099511628211)&((1<<64)-1)
        return h
    for name,kind in zip(NAMES,('sr_store_usc','sr_load_usc')):
        raw=expected[name][1];components=[e for e in rows if e['event']=='ctx_component' and e['kind']==kind]
        if len(components)!=2:raise ValueError('unexpected context component count')
        for c in components:
            matches=[e for e in rows if e['event']=='usc_upload_hash' and e['gpu_va']==c['gpu_va'] and e['size']==len(raw)]
            if len(matches)!=1 or matches[0]['fnv64']!=fnv(raw):raise ValueError('upload mismatch')
    return m
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('source','build','capture-zero','capture-ones','output'):p.add_argument('--'+name,type=pathlib.Path,required=True)
    p.add_argument('--experimental-incomplete',action='store_true');a=p.parse_args()
    if not a.experimental_incomplete:raise SystemExit('explicit offline opt-in required')
    src=a.source.resolve();build=a.build.resolve()
    git=lambda *args:subprocess.check_output(['git','-C',str(src),*args],text=True).strip()
    if git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all'):raise SystemExit('Mesa checkout mismatch')
    binary=build/'src/imagination/pco/uscgen/pco_clc';spv=build/'src/imagination/pco/usclib/pco_usclib.spv';built=build/'src/imagination/pco/uscgen/pco_uscgen_programs.c'
    try:
        for cap in (a.capture_zero,a.capture_ones):
            m=json.loads((cap/'run-manifest.json').read_text())
            if digest(build/'build.ninja')!=m['mesa_build_manifest']['build_ninja_sha256']:
                raise ValueError('Mesa build does not match capture')
        with tempfile.TemporaryDirectory(prefix='a04-context-usc-') as td:
            generated=[]
            for n in range(2):
                h=pathlib.Path(td)/('out%d.h'%n);c=pathlib.Path(td)/('out%d.c'%n)
                env=os.environ.copy();env['PCO_DEBUG']='no_pred_cf'
                subprocess.run([str(binary),str(spv),str(h),str(c)],env=env,check=True,timeout=120,stdout=subprocess.DEVNULL)
                generated.append(parse(c))
            if generated[0]!=generated[1] or generated[0]!=parse(built):raise ValueError('PCO output differs')
            expected=generated[0];m0=compare_capture(a.capture_zero,expected);m1=compare_capture(a.capture_ones,expected)
            if (m0.get('pco_fill'),m1.get('pco_fill'))!=('00','ff'):
                raise ValueError('two independent fill captures required')
            for key in ('library_sha256','context_source_sha256','context_object_sha256','pco_object_sha256'):
                if m0['diagnostic_manifest'].get(key)!=m1['diagnostic_manifest'].get(key):raise ValueError('diagnostic mismatch')
            out=a.output.resolve();out.mkdir(mode=0o700,parents=True,exist_ok=False)
            files={}
            for name,filename in zip(NAMES,('sr-store-usc.bin','sr-load-usc.bin')):
                data=expected[name][1];(out/filename).write_bytes(data)
                files[filename]={'sha256':hashlib.sha256(data).hexdigest(),'size':len(data),'metadata_words':expected[name][0]}
            (out/'manifest.json').write_text(json.dumps({'mesa_commit':PIN,'compiler_sha256':digest(binary),'spv_sha256':digest(spv),
                'generated_source_sha256':digest(built),'files':files,'gpu_execution_allowed':False,
                'limitations':['host pointers or unsafe immediates not fully excluded','FNV capture matching is not collision-resistant',
                               'GPU-generated shared-register size and all indirect accesses unverified','not tested on hardware']},indent=2)+'\n')
    except (OSError,ValueError,KeyError,TypeError,subprocess.SubprocessError,json.JSONDecodeError) as e:
        raise SystemExit('candidate generation failed: '+str(e))
    print('private USC candidates generated; GPU execution prohibited')
if __name__=='__main__':main()
