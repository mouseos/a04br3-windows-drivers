#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reindex one 09c630d foreground mask from its mistaken Morton interpretation."""
import argparse,json,pathlib,re
p=argparse.ArgumentParser();p.add_argument('--log',type=pathlib.Path,required=True);p.add_argument('--offset',type=int,required=True);p.add_argument('--end',type=int,required=True);a=p.parse_args()
if not 0<=a.offset<a.end<=a.log.stat().st_size:raise SystemExit('invalid range')
with a.log.open('rb') as f:f.seek(a.offset);raw=f.read(a.end-a.offset)
pat=rb'A04GPU ROW Y=([0-9A-F]{8}) LO=([0-9A-F]{8}) HI=([0-9A-F]{8})\r?\n';rr=re.findall(pat,raw)
if raw.count(b'A04GPU ROW Y=')!=64 or len(rr)!=64:raise SystemExit('incomplete/ambiguous rows')
rows=[tuple(int(x,16) for x in row) for row in rr]
if [r[0] for r in rows]!=list(range(64)):raise SystemExit('invalid row ordering')
pixels=[0]*4096
for y,lo,hi in rows:
 mask=lo|hi<<32
 for x in range(64):
  i=sum((((x>>k)&1)<<(2*k))|(((y>>k)&1)<<(2*k+1)) for k in range(6));pixels[i]=(mask>>x)&1
r=[''.join('#' if v else '.' for v in pixels[i:i+64]) for i in range(0,4096,64)]
print(json.dumps({'layout':'linear, 64 pixels/row; foreground only','foreground_pixels':sum(pixels),'rows':r,'limitations':['background versus other pixels cannot be recovered from this record','render failure is not explained by the layout correction']},indent=2))
