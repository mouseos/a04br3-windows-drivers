#!/usr/bin/env python3
"""Reproduce model tables from the pinned public CT3 source, never OTP bytes."""
import argparse,hashlib,json,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('vendor_root',type=Path);p.add_argument('--check',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[2];dst=root/'drivers/camera/hi556/vendor'
meta=json.loads((dst/'tables-provenance.json').read_text())
source=a.vendor_root/meta['source_path'];data=source.read_bytes()
if hashlib.sha256(data).hexdigest()!=meta['source_sha256']:raise SystemExit('Pinned vendor source hash mismatch')
s=data.decode();tables={}
for key,func in [('global','sensor_init'),('preview','preview_setting')]:
 m=re.search(r'static void '+func+r'\([^)]*\)\s*\{',s)
 if not m:raise SystemExit('Source function missing: '+func)
 tail=s[m.end():];depth=1;end=0
 for end,c in enumerate(tail):
  depth+=(c=='{')-(c=='}')
  if depth==0:break
 body=re.sub(r'/\*.*?\*/|//[^\n]*','',tail[:end],flags=re.S)
 pairs=[(int(r,16),int(v,16)) for r,v in re.findall(r'write_cmos_sensor\(\s*(0x[\da-fA-F]+)\s*,\s*(0x[\da-fA-F]+)\s*\)',body)]
 if len(pairs)!=meta['source_call_counts'][key]:raise SystemExit('Unexpected table call count')
 if key=='preview':
  if pairs[-1]!=(0x114,0x100):raise SystemExit('Unexpected stream-on')
  pairs=pairs[:-1]
 tables[key]=pairs
header='''/* SPDX-License-Identifier: GPL-2.0-only
 * Register values from MediaTek/SANYO A04BR3 HI556 vendor source.
 * CT3_GPL_LGPL_Src 7c4782127d4ea11965be00ce1343056be9bfb769.
 * Generic model tables; no device calibration/OTP data. */
#include <stdint.h>
struct a04_hi556_reg {uint16_t reg,value;};
'''
for k,values in tables.items():
 header+='static const struct a04_hi556_reg a04_hi556_'+k+'[] = {\n'
 header+=''.join(' {0x%04x,0x%04x},\n'%v for v in values)+'};\n'
encoded=header.encode()
if hashlib.sha256(encoded).hexdigest()!=meta['header_sha256']:raise SystemExit('Generated header provenance mismatch')
if a.check:
 if (dst/'hi556_tables.h').read_bytes()!=encoded:raise SystemExit('Checked-in header differs')
 print('Pinned model tables reproduced; stream-on excluded from preview')
else:(dst/'hi556_tables.h').write_bytes(encoded)
