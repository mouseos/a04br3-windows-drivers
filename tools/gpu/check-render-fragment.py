#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Cross-check fixed probe PPP shader bases with fragment PDS metadata."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import re
import struct
import sys

def fnv(data):
    h=14695981039346656037
    for byte in data: h=((h^byte)*1099511628211)&((1<<64)-1)
    return h

def check(root):
    root = pathlib.Path(root)
    manifest = json.loads((root/'run-manifest.json').read_text())
    diag = manifest.get('diagnostic_manifest') or {}
    events = (root/'events.jsonl').read_bytes()
    log = (root/'probe.log').read_bytes()
    if not diag.get('pds_construct_metadata_only') or not manifest.get('control_stream_dump'):
        raise ValueError('missing instrumentation')
    if hashlib.sha256(events).hexdigest() != manifest['events_sha256'] or hashlib.sha256(log).hexdigest() != manifest['probe_log_sha256']:
        raise ValueError('capture digest mismatch')
    spec = importlib.util.spec_from_file_location('trace', pathlib.Path(__file__).with_name('check-render-trace.py'))
    trace = importlib.util.module_from_spec(spec); spec.loader.exec_module(trace)
    report = trace.check(root/'events.jsonl')
    if report['fragment_unmapped'] or report['fragment_missing_uploads'] or report['uploads_ambiguous_vm']:
        raise ValueError('fragment ranges not established')
    links = [e for e in map(json.loads,events.splitlines()) if e['event'] == 'fragment_shader_link']
    bases = [int(x,16) for x in re.findall(rb'^\s*shaderbase\s*:\s*(0x[0-9a-fA-F]+)',log,re.M)]
    # This parser is deliberately specific to the two-draw fixed probe. A
    # stream with extra PPP updates cannot be correlated by ordinal alone.
    if len(links) != 2 or len(bases) != 2:
        raise ValueError('unexpected fragment/PPP count')
    for base, link in zip(bases, links):
        if base != link['pds_data_gpu'] or link['pds_code_gpu'] != base + link['pds_data_size']:
            raise ValueError('PPP/PDS layout mismatch')
        if (link['doutu_offset'] != 0 or link['pds_data_written'] != 32 or link['pds_code_written'] != 4 or
            link['entry_gpu'] >> 32 != 0xe0 or link['entry_gpu'] & 3 or link['doutu_control'] & ~0x3fe00000000 or
            link['doutu_control'] & (3<<33)):
            raise ValueError('unsupported fragment program')
        doutu=struct.pack('<Q',(link['entry_gpu'] & 0xfffffffc) | link['doutu_control'])
        code=struct.pack('<I',0xf4000002)
        if fnv(doutu) != link['doutu_fnv64'] or fnv(code) != link['code_fnv64']:
            raise ValueError('fragment generated words mismatch')
    return {'fragment_ppp_pairs':len(links),'gpu_execution_allowed':False,
            'limitations':['ordinal matching is specific to this fixed probe and does not prove job-time liveness',
                           'shader bytes, PDS padding, remaining indirect references and CPU writes are not validated',
                           'FNV comparison is not collision-resistant']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try: print(json.dumps(check(a.capture),indent=2))
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError):
        print('invalid or unsupported fragment capture',file=sys.stderr);return 2
    return 1
if __name__=='__main__': sys.exit(main())
