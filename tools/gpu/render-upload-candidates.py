#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Conservative live upload candidates at each geometry/fragment submission.

This is not a dependency decoder; it never proves that any job uses a range.
"""
import argparse
import collections
import json
import pathlib
import sys

def candidates(events):
    maps={};contexts={};uploads=[];result=[];submit=-1
    for e in events:
        kind=e['event']
        if kind=='vm_map':
            key=(e['vm'],e['va']); length=e['size']
            if key in maps or length<=0 or any(vm==e['vm'] and e['va']<va+n and va<e['va']+length for (vm,va),n in maps.items()):raise ValueError('mapping overlap')
            maps[key]=length
        elif kind=='vm_unmap':
            key=(e['vm'],e['va'])
            if maps.pop(key)!=e['size']:raise ValueError('invalid unmap')
            uploads=[u for u in uploads if u['mapping']!=key]
        elif kind=='vm_destroy':
            maps={k:v for k,v in maps.items() if k[0]!=e['handle']}
            uploads=[u for u in uploads if u['mapping'][0]!=e['handle']]
        elif kind=='context_create':contexts[e['handle']]=e['vm']
        elif kind=='context_destroy':contexts.pop(e['handle'])
        elif kind in ('upload','pds_construct'):
            va=e['gpu_va']; size=e['size']
            if size<=0:raise ValueError('invalid upload')
            matches=[k for k,n in maps.items() if va>=k[1] and size<=n and va-k[1]<=n-size]
            if len(matches)!=1:raise ValueError('ambiguous or unmapped upload')
            key=matches[0]
            # A later write can invalidate any previously observed overlapping
            # upload, even if the recorded write covers only part of it.
            uploads=[u for u in uploads if u['mapping']!=key or not (va<u['va']+u['size'] and u['va']<va+size)]
            uploads.append(dict(mapping=key,va=va,size=size,kind=e['kind']))
        elif kind=='submit_begin':submit+=1
        elif kind=='job' and e['type'] in (0,1):
            vm=contexts[e['context']]
            counts=collections.Counter(u['kind'] for u in uploads if u['mapping'][0]==vm)
            result.append(dict(submit=submit,index=e['index'],type=e['type'],candidates=dict(sorted(counts.items()))))
    if not result:raise ValueError('no render jobs')
    return {'jobs':result,'safe_for_gpu':False,
            'limitations':['upload candidates are not proven GPU dependencies',
              'CPU writes outside instrumented upload routines are not tracked',
              'concurrent event order and BO reuse are not fully established']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('events');a=p.parse_args()
    try:
        events=[json.loads(x) for x in pathlib.Path(a.events).read_bytes().splitlines()]
        print(json.dumps(candidates(events),indent=2))
    except (OSError,KeyError,TypeError,ValueError):
        print('invalid capture',file=sys.stderr);return 2
    return 1
if __name__=='__main__':sys.exit(main())
