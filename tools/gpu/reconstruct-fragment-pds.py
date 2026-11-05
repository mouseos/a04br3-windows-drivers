#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate only the fixed fragment PDS allocation; never enable replay."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import struct

spec=importlib.util.spec_from_file_location('fragment',pathlib.Path(__file__).with_name('check-render-fragment.py'))
fragment=importlib.util.module_from_spec(spec);spec.loader.exec_module(fragment)

def build(link):
    # Only called after check-render-fragment has validated the layout,
    # tracked ranges, control bits and the generated instruction against Mesa.
    data=bytearray(link['pds_data_size'] + link['pds_code_size'])
    struct.pack_into('<Q',data,0,(link['entry_gpu'] & 0xfffffffc) | link['doutu_control'])
    struct.pack_into('<I',data,link['pds_data_size'],0xf4000002)
    return bytes(data)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=pathlib.Path)
    p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--index',type=int,choices=(0,1),default=0)
    p.add_argument('--experimental-incomplete',action='store_true');a=p.parse_args()
    if not a.experimental_incomplete:raise SystemExit('incomplete fixture: explicit offline opt-in required')
    fragment.check(a.capture)
    spec=importlib.util.spec_from_file_location('submit',pathlib.Path(__file__).with_name('check-render-submit-dependencies.py'))
    submit=importlib.util.module_from_spec(spec);spec.loader.exec_module(submit);submit.check(a.capture)
    links=[e for e in map(json.loads,(a.capture/'events.jsonl').read_bytes().splitlines()) if e['event']=='fragment_shader_link']
    link=links[a.index]
    # The observed layout is fixed; refuse large or unexpected allocations.
    if link['pds_data_size']!=32 or link['pds_code_size']!=16:raise SystemExit('unsupported fragment allocation')
    data=build(link)
    a.output.mkdir(mode=0o700,parents=True,exist_ok=False)
    (a.output/'fragment-pds.bin').write_bytes(data)
    (a.output/'manifest.json').write_text(json.dumps({'gpu_execution_allowed':False,'index':a.index,
        'sha256':hashlib.sha256(data).hexdigest(),
        'limitations':['only PDS DOUTU and code generated','no USC or command stream','not tested on hardware']},indent=2)+'\n')
    print('fragment PDS allocation regenerated; GPU execution prohibited')
if __name__=='__main__':main()
