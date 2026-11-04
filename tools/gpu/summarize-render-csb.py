#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Summarize private Mesa dumps without emitting addresses or raw BO bytes."""
import argparse
import hashlib
import json
import pathlib
import re
import sys
BLOCKS=('PPP_STATE_UPDATE','PDS_STATE_UPDATE','VDM_STATE_UPDATE','INDEX_LIST','STREAM_LINK','STREAM_RETURN','STREAM_TERMINATE')
FIELDS=('vs_pds_data_base_addr','vs_pds_code_base_addr','pds_data_addr','pds_code_addr','usc_target','pds_data_size','vs_pds_data_size')
def inspect(root):
    root=pathlib.Path(root)
    manifest=json.loads((root/'run-manifest.json').read_text())
    if not manifest.get('control_stream_dump'):raise ValueError('debug dump not recorded')
    raw=(root/'probe.log').read_bytes()
    if hashlib.sha256(raw).hexdigest()!=manifest.get('probe_log_sha256'):raise ValueError('log digest mismatch')
    if len(raw)>128*1024*1024:raise ValueError('log too large')
    text=raw.decode('utf-8','replace')
    counts={key:len(re.findall(r'\b'+key+r'\b',text)) for key in BLOCKS}
    fields={key:len(re.findall(r'^\s*'+key+r'\s*:',text,re.M)) for key in FIELDS}
    errors=sum(text.lower().count(x) for x in ('<unable to read buffer>','<could not decode','<out of bounds>'))
    event_bytes=(root/'events.jsonl').read_bytes()
    if hashlib.sha256(event_bytes).hexdigest()!=manifest.get('events_sha256'):raise ValueError('event digest mismatch')
    uploads=[row for row in map(json.loads,event_bytes.splitlines()) if row.get('event')=='upload']
    correlations={}
    for key in ('vs_pds_data_base_addr','vs_pds_code_base_addr','pds_data_addr','pds_code_addr'):
        values=[int(x,16) for x in re.findall(r'^\s*'+key+r'\s*:\s*(0x[0-9a-fA-F]+)',text,re.M)]
        matched=sum(any(u['gpu_va']<=v<u['gpu_va']+u['size'] for u in uploads) for v in values)
        correlations[key]={'numeric_fields':len(values),'address_within_upload':matched,'unmatched':len(values)-matched}
    dumps=text.count('CONTROL STREAM DUMP')
    if not dumps:raise ValueError('no dumps')
    return {'dumps':dumps,'blocks':counts,'field_occurrences':fields,'reported_decode_errors':errors,'address_correlations_any_time':correlations,
       'fixture_exportable':False,'limitations':['raw dump may contain host pointers and is not exported',
       'only the first control stream BO is decoded by Mesa',
       'address matches use starts and any-time uploads, not complete ranges or job lifetimes'
       ,'field counts do not establish indirect access ranges or content provenance']}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try:print(json.dumps(inspect(a.capture),indent=2))
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):print('invalid private dump',file=sys.stderr);return 2
    return 1
if __name__=='__main__':sys.exit(main())
