#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Summarize already-collected TA context shared-size firmware trace lines."""
import argparse
import json
import pathlib
import re
import sys
PAT=re.compile(r'\bTA context shared alloc size store 0x([0-9a-fA-F]{1,8}), load 0x([0-9a-fA-F]{1,8})(?![0-9a-fA-F])')
def analyze(text):
    values=[(int(a,16),int(b,16)) for a,b in PAT.findall(text)]
    if not values:raise ValueError('no matching trace records')
    def safe(n):return n<=256 and n%64==0
    return {'records':len(values),'unique_store_values':sorted({a for a,b in values}),
      'unique_load_values':sorted({b for a,b in values}),
      'different_pairs':sum(a!=b for a,b in values),
      'outside_source_loop_conditions':sum(not safe(a) or not safe(b) for a,b in values),
      'gpu_execution_allowed':False,'limitations':['input is not authenticated as a device trace',
        'firmware fields are not independently proven to equal special register 103',
        'observed values do not establish all possible values or DMA bounds']}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=pathlib.Path);a=p.parse_args()
    try:
        if a.log.stat().st_size>64*1024*1024:raise ValueError('log too large')
        print(json.dumps(analyze(a.log.read_text(encoding='utf-8',errors='replace')),indent=2))
    except (OSError,ValueError):print('no usable shared-size records',file=sys.stderr);return 2
    return 1
if __name__=='__main__':sys.exit(main())
