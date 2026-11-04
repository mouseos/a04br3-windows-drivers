#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Model pinned Mesa shared-state loop; diagnostic only, no GPU execution."""
import argparse
import json
import math
import sys

PIN='ef11349e8e5d620a2227fac0eed17e7ed1f86428'
MASK=(1<<32)-1
def model(blocks,chunk):
    if not isinstance(blocks,int) or blocks<0 or blocks>MASK or chunk not in (256,1024):raise ValueError('invalid input')
    initial=(blocks*16)&MASK
    if not initial:return {'blocks':blocks,'initial_dwords':0,'iterations':0,'terminates':True,'within_state':True,
                           'arithmetic_overflow':blocks>MASK//16}
    gcd=math.gcd(chunk,1<<32)
    if initial%gcd:return {'blocks':blocks,'initial_dwords':initial,'iterations':None,'terminates':False,'within_state':False}
    iterations=initial//chunk if initial%chunk==0 else None
    if iterations is None:
        # The loop wraps in 32-bit arithmetic; with these power-of-two steps
        # a non-multiple of chunk cannot reach zero.
        return {'blocks':blocks,'initial_dwords':initial,'iterations':None,'terminates':False,'within_state':False}
    return {'blocks':blocks,'initial_dwords':initial,'iterations':iterations,'terminates':True,
            'within_state':initial<=4096,'arithmetic_overflow':blocks>MASK//16}
def bounded_model(blocks,chunk):
    if not isinstance(blocks,int) or blocks<0 or blocks>MASK or chunk not in (256,1024):raise ValueError('invalid input')
    accepted=blocks if blocks<=256 else 0
    words=accepted*16;iterations=(words+chunk-1)//chunk
    return {'input_blocks':blocks,'accepted_blocks':accepted,'iterations':iterations,
            'max_dwords':words,'within_state':words<=4096,'terminates':True}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--blocks',type=int,nargs='+',default=[0,1,16,64,256,257]);a=p.parse_args()
    try: result={str(n):{'store':model(n,1024),'load':model(n,256),
                         'bounded_store':bounded_model(n,1024),'bounded_load':bounded_model(n,256)} for n in a.blocks}
    except ValueError:print('invalid block count',file=sys.stderr);return 2
    print(json.dumps({'mesa_commit':PIN,'shared_data_bytes':16384,'prefix_bytes':48,'cases':result,
       'gpu_execution_allowed':False,'limitations':['source-level arithmetic model, not a GPU emulator',
       'hardware allocation granularity and firmware ordering not proven','GPU-generated size is untrusted until validated']},indent=2))
    return 1
if __name__=='__main__':sys.exit(main())
