#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Summarize already-collected TA context shared-size firmware trace lines."""
import argparse
import json
import pathlib
import re
import sys
PAT=re.compile(r'\bTA context shared alloc size store 0x([0-9a-fA-F]{1,8}), load 0x([0-9a-fA-F]{1,8})(?![0-9a-fA-F])')
UART=re.compile(r'^A04GPU SHARED CANDIDATES=([0-9A-F]{8}) STORE_MASK=([0-9A-F]{8}) LOAD_MASK=([0-9A-F]{8}) OTHER=([0-9A-F]{8}) DIFFERENT=([0-9A-F]{8}) CHANGED=([0-9A-F]{8})$',re.M)
def analyze_uart(text):
    samples=[]
    for fields in UART.findall(text):
        n,store,load,other,different,changed=(int(x,16) for x in fields)
        if store&~31 or load&~31 or changed>1 or different>n or other>2*n:
            raise ValueError('invalid diagnostic counters')
        samples.append({'candidates':n,'store_values':[64*i for i in range(5) if store&(1<<i)],
          'load_values':[64*i for i in range(5) if load&(1<<i)],'other_fields':other,
          'different_pairs':different,'trace_changed':bool(changed)})
    if not samples:raise ValueError('no diagnostic samples')
    return {'samples':samples,'gpu_execution_allowed':False,
      'limitations':['candidates are not validated trace packet boundaries',
        'snapshot may race with firmware even if trace pointers appear unchanged',
        'firmware fields are not proven to equal special register 103',
        'no records does not establish a safe value or context switch behavior']}
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
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--uart',action='store_true');p.add_argument('log',type=pathlib.Path);a=p.parse_args()
    try:
        if a.log.stat().st_size>64*1024*1024:raise ValueError('log too large')
        contents=a.log.read_text(encoding='utf-8',errors='replace')
        print(json.dumps(analyze_uart(contents) if a.uart else analyze(contents),indent=2))
    except (OSError,ValueError):print('no usable shared-size records',file=sys.stderr);return 2
    return 1
if __name__=='__main__':sys.exit(main())
