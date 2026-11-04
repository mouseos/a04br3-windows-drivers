#!/usr/bin/env python3
"""Classify one bounded UART slice; never send commands or claim GPU safety."""
import argparse, json, re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--log',type=Path,required=True);p.add_argument('--offset',type=int,required=True);p.add_argument('--end',type=int,required=True);a=p.parse_args()
size=a.log.stat().st_size
if not 0<=a.offset<a.end<=size:raise SystemExit('invalid bounded log offsets')
with a.log.open('rb') as f:f.seek(a.offset);raw=f.read(a.end-a.offset)
pattern=rb'A04GPU RENDER STATUS=([0-9A-F]{8}) STAGE=([0-9A-F]{8}) BG=([0-9A-F]{8}) TRI=([0-9A-F]{8}) OTHER=([0-9A-F]{8}) BOUNDED=([0-9A-F]{8}) B0_VALID=([0-9A-F]{8}) B0=([0-9A-F]{8}) B1_VALID=([0-9A-F]{8}) B1=([0-9A-F]{8})\r?\n'
rows=re.findall(pattern,raw); ebs=raw.count(b'A04BR3 recovery: EBS'); bugs=bool(re.search(rb'A04GPU BUGCHECK (?:(?:[0-9A-F]{8} ?){5}|DATA_UNAVAILABLE)\r?\n|A04WIN BUGCHECK',raw))
result={'verdict':'inconclusive','ebs_markers':ebs,'render_records':len(rows),'bugcheck':bugs,'shutdown_verified':False,'context_switch_verified':False,'geometry_verified':False,'device_safety_verified':False}
if len(rows)==1:
 keys=('status','stage','background','triangle','other','bounded','b0_valid','b0','b1_valid','b1')
 vals=dict(zip(keys,(int(v,16) for v in rows[0])));result['render']=vals
 if ebs==1 and not bugs and vals['status']==0 and vals['stage']==32 and vals['background']>0 and 1128<=vals['triangle']<=1176 and vals['other']==0 and vals['background']+vals['triangle']==4096:
  result['verdict']='histogram_match_only'
 elif ebs==1 and not bugs:result['verdict']='render_failed_or_inconsistent'
print(json.dumps(result,indent=2));raise SystemExit(0 if result['verdict']=='histogram_match_only' else 1)
