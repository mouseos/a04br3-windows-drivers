#!/usr/bin/env python3
"""Classify a bounded UART slice; the decoded Morton layout remains a hypothesis."""
import argparse, json, re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--log',type=Path,required=True);p.add_argument('--offset',type=int,required=True);p.add_argument('--end',type=int,required=True);a=p.parse_args()
size=a.log.stat().st_size
if not 0<=a.offset<a.end<=size:raise SystemExit('invalid bounded log offsets')
with a.log.open('rb') as f:f.seek(a.offset);raw=f.read(a.end-a.offset)
pattern=rb'A04GPU RENDER STATUS=([0-9A-F]{8}) STAGE=([0-9A-F]{8}) BG=([0-9A-F]{8}) TRI=([0-9A-F]{8}) OTHER=([0-9A-F]{8}) MISMATCH=([0-9A-F]{8}) BOUNDED=([0-9A-F]{8}) B0_VALID=([0-9A-F]{8}) B0=([0-9A-F]{8}) B1_VALID=([0-9A-F]{8}) B1=([0-9A-F]{8})\r?\n'
row_pattern=rb'A04GPU ROW Y=([0-9A-F]{8}) LO=([0-9A-F]{8}) HI=([0-9A-F]{8})\r?\n'
records=list(re.finditer(pattern,raw)); rows=list(re.finditer(row_pattern,raw));ebs=raw.count(b'A04BR3 recovery: EBS');bugs=bool(re.search(rb'A04GPU BUGCHECK (?:(?:[0-9A-F]{8} ?){5}|DATA_UNAVAILABLE)\r?\n|A04WIN BUGCHECK',raw))
result={'verdict':'inconclusive','ebs_markers':ebs,'render_records':len(records),'row_records':len(rows),'bugcheck':bugs,'shutdown_verified':False,'context_switch_verified':False,'device_safety_verified':False,'layout':'64x64 Morton assumption; device unverified'}
if len(records)==1:
 keys=('status','stage','background','triangle','other','mismatch','bounded','b0_valid','b0','b1_valid','b1');vals=dict(zip(keys,(int(v,16) for v in records[0].groups())));result['render']=vals
 if ebs==1 and not bugs and raw.count(b'A04GPU ROW Y=')==64 and raw.count(b'A04GPU RENDER STATUS=')==1 and len(rows)==64 and raw.find(b'A04BR3 recovery: EBS') < rows[0].start() and not re.search(rb'BOOT_REASON|A04WIN BUGCHECK',raw[raw.find(b'A04BR3 recovery: EBS'):records[0].start()]) and all(r.start()<records[0].start() for r in rows):
  data=[tuple(int(x,16) for x in r.groups()) for r in rows]
  if [r[0] for r in data]==list(range(64)):
   mismatch=count=0; first=[]; masks=[]
   for y,lo,hi in data:
    mask=lo | hi<<32;count+=mask.bit_count();masks.append(f"{mask:016X}")
    for x in range(64):
     edge=x>=8 and x<49 and y>=8 and x+y==63;inside=x>=8 and x<49 and y>=8 and x+y<63
     if not edge and bool(mask & (1<<x))!=inside:
      mismatch+=1
      if len(first)<8:first.append([x,y])
   result['row_foreground']=count;result['row_mismatch']=mismatch;result['triangle_rows_hex']=masks;result['first_mismatches_xy']=first
   ok=vals['status']==0 and vals['stage']==32 and vals['background']+vals['triangle']==4096 and vals['other']==0 and 1107<=vals['triangle']<=1148 and vals['triangle']==count and vals['mismatch']==0 and mismatch==0
   result['verdict']='spatial_match_morton_assumed' if ok else 'render_failed_or_inconsistent'
print(json.dumps(result,indent=2));raise SystemExit(0 if result['verdict']=='spatial_match_morton_assumed' else 1)
