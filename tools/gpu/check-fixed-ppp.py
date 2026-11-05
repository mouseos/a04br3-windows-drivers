#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate fixed PPP reconstruction against private Mesa digest metadata."""
import argparse,hashlib,importlib.util,json,pathlib,re,struct,sys
HERE=pathlib.Path(__file__).resolve().parent
FIXED=(0x007f0e67,0x00050000,0x022e0f00,0x0e000000,0,0,0,0x01000000,0x00804000,0,
       0x80000003,3,0x42000000,0x42000000,0x42000000,0x42000000,0,0x3f800000,0x29e12e13,0x05040000,0,0,0,0x6a0,0)
def fnv(data):
    h=14695981039346656037
    for b in data:h=((h^b)*1099511628211)&((1<<64)-1)
    return h

def match_records(rows,draws):
    if len(draws)!=2:raise ValueError('unexpected draw count')
    for pp,finish,shader,code,data in draws:
        refs=(shader,code,data);sizes=(48,16,32)
        for i,(addr,size) in enumerate(zip(refs,sizes)):
            if addr<=0 or addr>=(1<<40) or size>(1<<40)-addr or addr&15 or addr>>32!=0xda:
                raise ValueError('invalid PDS reference')
            if any(addr<refs[j]+sizes[j] and refs[j]<addr+size for j in range(i)):
                raise ValueError('overlapping PDS reference')
        words=list(FIXED);words[5]=shader&0xffffffff;words[6]=code&0xffffffff;words[9]=data&0xffffffff
        checks=((pp,100,fnv(struct.pack('<25I',*words))),(finish,12,fnv(struct.pack('<III',0x02000000,0x00040001,0))))
        for addr,size,h in checks:
            matches=[x for x in rows if x['gpu_va']==addr and x['bytes']==size]
            if not matches or any(x['fnv64']!=h for x in matches):raise ValueError('PPP hash mismatch')

def check(root):
    root=pathlib.Path(root);m=json.loads((root/'run-manifest.json').read_text());diag=m.get('diagnostic_manifest') or {}
    if not diag.get('ppp_hash_metadata_only'):raise ValueError('PPP hashes not recorded')
    raw=(root/'probe.log').read_bytes();hashes=(root/'ppp-hashes.jsonl').read_bytes()
    if hashlib.sha256(raw).hexdigest()!=m.get('probe_log_sha256') or hashlib.sha256(hashes).hexdigest()!=m.get('ppp_hashes_sha256'):
        raise ValueError('capture hash mismatch')
    for script in ('check-fixed-vdm','check-fragment-uniform'):
        spec=importlib.util.spec_from_file_location('dep',HERE/(script+'.py'));mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);mod.check(root)
    sections=[x for x in re.split('CONTROL STREAM DUMP',raw.decode('utf-8','replace'))[1:] if re.search(r'^\[[0-9a-f]+\]\s+\d+:\s+INDEX_LIST',x,re.M)]
    draws=[]
    for section in sections:
        def field(name,count,indent=r'\s*'):
            a=[int(x,16) for x in re.findall(r'^'+indent+name+r'\s*:\s*(0x[0-9a-fA-F]+)',section,re.M)]
            if len(a)!=count:raise ValueError('missing PPP field')
            return a
        pp,finish=field('addr',2,'  ')
        draws.append((pp,finish,field('shaderbase',1)[0],field('texunicodebase',1)[0],field('uniformdatabase',1)[0]))
    match_records([json.loads(x) for x in hashes.splitlines()],draws)
    return {'draws':2,'ppp_state_bytes':100,'ppp_finish_bytes':12,'gpu_execution_allowed':False,
      'limitations':['FNV is not collision resistant','scissor, render targets and other indirect references need independent validation',
        'GPU lifetime and device execution are not established']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try:print(json.dumps(check(a.capture),indent=2));return 1
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):print('invalid fixed PPP capture',file=sys.stderr);return 2
if __name__=='__main__':sys.exit(main())
