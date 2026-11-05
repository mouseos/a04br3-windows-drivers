#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check typed fragment uniform data against private Mesa metadata."""
import argparse,hashlib,importlib.util,json,pathlib,re,struct,sys
HERE=pathlib.Path(__file__).resolve().parent

def fnv(data):
    h=14695981039346656037
    for b in data:h=((h^b)*1099511628211)&((1<<64)-1)
    return h

def check(root):
    root=pathlib.Path(root);m=json.loads((root/'run-manifest.json').read_text());diag=m.get('diagnostic_manifest') or {}
    if not m.get('scratchless_vertex') or not diag.get('pds_fs_meta_metadata_only') or not diag.get('pds_code_hash_metadata_only'):
        raise ValueError('instrumentation missing')
    events=(root/'events.jsonl').read_bytes();log=(root/'probe.log').read_bytes();hashes=(root/'pds-code-hashes.jsonl').read_bytes()
    for data,key in ((events,'events_sha256'),(log,'probe_log_sha256'),(hashes,'pds_code_hashes_sha256')):
        if hashlib.sha256(data).hexdigest()!=m.get(key):raise ValueError('capture hash mismatch')
    spec=importlib.util.spec_from_file_location('submit',HERE/'check-render-submit-dependencies.py');submit=importlib.util.module_from_spec(spec);spec.loader.exec_module(submit)
    result=submit.check(root)
    if result['fragment_fs_meta_checked']!=2:raise ValueError('fragment metadata not live')
    events=[json.loads(x) for x in events.splitlines()];rows=[json.loads(x) for x in hashes.splitlines()];text=log.decode('utf-8','replace')
    def addresses(field):
        values=[int(x,16) for x in re.findall(r'^\s*'+field+r'\s*:\s*(0x[0-9a-fA-F]+)',text,re.M)]
        if len(values)!=2:raise ValueError('unexpected fragment state')
        return values
    for addr,code in zip(addresses('uniformdatabase'),addresses('texunicodebase')):
        construct=[e for e in events if e['event']=='pds_construct' and e['gpu_va']==addr and e['kind']=='descriptor_mappings']
        entries=[e for e in events if e['event']=='pds_entry' and e['gpu_va']==addr and e['kind']=='descriptor_mappings']
        deps=[e for e in events if e['event']=='pds_dependency' and e['parent_gpu']==addr and e['kind']=='spill']
        meta=[e for e in events if e['event']=='pds_fs_meta' and e['parent_gpu']==addr]
        expected=[(5,0,12,0),(5,2,10,0),(2,4,0,0x10008003),(2,5,0,0x90000001)]
        if len(construct)!=1 or construct[0]['size']!=24 or construct[0]['entries']!=4 or sorted((e['entry_type'],e['const_offset'],e['subtype'],e['value']) for e in entries)!=sorted(expected) or len(deps)!=1 or len(meta)!=1:
            raise ValueError('fragment uniform layout')
        if deps[0]['offset']!=0 or meta[0]['offset']!=2 or meta[0]['value']!=0x01fffffe:
            raise ValueError('fragment uniform values')
        data=struct.pack('<QQII',deps[0]['info_gpu'],meta[0]['gpu_va'],0x10008003,0x90000001)
        program=struct.pack('<II',0xf0040000,0xf4050100)
        if fnv(data)!=construct[0]['fnv64'] or sum(r['gpu_va']==code and r['size']==8 and r['fnv64']==fnv(program) for r in rows)!=1:
            raise ValueError('fragment PDS mismatch')
    return {'draws':2,'fragment_uniform_bytes':24,'fs_meta_bytes':4,'pds_code_bytes':8,'gpu_execution_allowed':False,
      'limitations':['FNV is not collision resistant','CPU writes outside instrumentation are not observed',
        'PPP contents, complete GPU references and device execution are not validated']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try:print(json.dumps(check(a.capture),indent=2));return 1
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):print('invalid fragment uniform capture',file=sys.stderr);return 2
if __name__=='__main__':sys.exit(main())
