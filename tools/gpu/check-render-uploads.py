#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check Mesa upload extents without reading or publishing BO contents."""
import argparse
import hashlib
import json
import pathlib
import sys

LIMIT=1 << 40
KINDS=('pds_data','pds_code','usc_code')
def inspect(capture):
    root=pathlib.Path(capture)
    manifest=json.loads((root/'run-manifest.json').read_text())
    uploads=(root/'uploads.jsonl').read_bytes(); events=(root/'events.jsonl').read_bytes()
    if not (manifest.get('diagnostic_manifest') or {}).get('upload_metadata_only'):
        raise ValueError('upload instrumentation missing')
    if hashlib.sha256(uploads).hexdigest()!=manifest.get('uploads_sha256') or hashlib.sha256(events).hexdigest()!=manifest.get('events_sha256'):
        raise ValueError('digest mismatch')
    maps=[e for e in map(json.loads,events.splitlines()) if e['event']=='vm_map']
    counts={k:0 for k in KINDS}; unmatched={k:0 for k in KINDS}
    for row in map(json.loads,uploads.splitlines()):
        kind=row['kind']; va=row['gpu_va']; length=row['size']
        if kind not in counts or not isinstance(va,int) or not isinstance(length,int) or va<=0 or length<=0 or va>=LIMIT or length>LIMIT-va:
            raise ValueError('invalid upload')
        counts[kind]+=1
        if not any(va>=m['va'] and length<=m['size'] and va-m['va']<=m['size']-length for m in maps):
            unmatched[kind]+=1
    if not all(counts.values()): raise ValueError('missing upload classes')
    return {'uploads':counts,'unmatched_any_time':unmatched,'safe_for_gpu':False,
      'limitations':['matches do not prove the VM or mapping was live at a particular job',
                     'not every compiler or command-buffer upload passes through these functions',
                     'content, relocations, CPU provenance and indirect references are unverified']}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try: print(json.dumps(inspect(a.capture),indent=2))
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):
        print('invalid upload capture',file=sys.stderr);return 2
    return 1
if __name__=='__main__':sys.exit(main())
