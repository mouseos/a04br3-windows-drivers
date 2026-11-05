#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare fixed, address-free PDS instructions with private Mesa upload hashes."""
import argparse
import hashlib
import json
import pathlib
import re
import struct
import sys

PIN='ef11349e8e5d620a2227fac0eed17e7ed1f86428'
CODE={'vs_pds_code_base_addr':(0xf0024001,0xfc000002,0xd3000000),
      'pds_code_addr':(0xf0040000,0xf4050100)}
def fnv(data):
    h=14695981039346656037
    for b in data:h=((h^b)*1099511628211)&((1<<64)-1)
    return h

def check(root):
    root=pathlib.Path(root);manifest=json.loads((root/'run-manifest.json').read_text())
    diag=manifest.get('diagnostic_manifest') or {}
    if not diag.get('pds_code_hash_metadata_only') or not diag.get('sipf1_patch') or not manifest.get('control_stream_dump') or manifest['mesa_build_manifest']['mesa_commit']!=PIN:
        raise ValueError('capture provenance')
    data=(root/'pds-code-hashes.jsonl').read_bytes();log=(root/'probe.log').read_bytes()
    if hashlib.sha256(data).hexdigest()!=manifest.get('pds_code_hashes_sha256') or hashlib.sha256(log).hexdigest()!=manifest.get('probe_log_sha256'):
        raise ValueError('capture hash')
    rows=[json.loads(x) for x in data.splitlines()];text=log.decode('utf-8','replace')
    codes=dict(CODE)
    if manifest.get('scratchless_vertex'):
        shader=pathlib.Path(__file__).with_name('shaders')/'triangle-scratchless.vert'
        if manifest['scratchless_vertex'].get('source_sha256')!=hashlib.sha256(shader.read_bytes()).hexdigest():raise ValueError('variant source')
        codes['pds_code_addr']=(0xf4020000,)
    for field,words in codes.items():
        values=[int(x,16) for x in re.findall(r'^\s*'+field+r'\s*:\s*(0x[0-9a-fA-F]+)',text,re.M)]
        raw=struct.pack('<%dI'%len(words),*words); expected=fnv(raw)
        if len(values)!=2 or any(sum(r['gpu_va']==v and r['size']==len(raw) and r['fnv64']==expected for r in rows)!=1 for v in values):
            raise ValueError('fixed PDS code mismatch')
    return {'draws':2,'vertex_code_bytes':12,'descriptor_code_bytes':4 if manifest.get('scratchless_vertex') else 8,'gpu_execution_allowed':False,
            'limitations':['FNV is not collision resistant','shader and indirect GPU references remain unverified',
                           'capture is metadata only and does not execute GPU work']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try: print(json.dumps(check(a.capture),indent=2));return 1
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):
        print('invalid or unsupported PDS code capture',file=sys.stderr);return 2
if __name__=='__main__':sys.exit(main())
