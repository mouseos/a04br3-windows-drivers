#!/usr/bin/env python3
"""Preserve one UART slice and decode a complete screen; never reset hardware."""
import argparse, json, os, subprocess, time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--log',type=Path,required=True);p.add_argument('--offset',type=int,required=True);p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--timeout',type=int,default=1400);p.add_argument('--expected-size',default='800x600');a=p.parse_args()
if a.offset<0 or a.timeout<1 or a.output_dir.exists() or not a.log.is_file() or a.offset>a.log.stat().st_size:raise SystemExit('invalid offset, log or output')
a.output_dir.mkdir(parents=True);out=a.output_dir/'first-boot-uart.log';screen=a.output_dir/'windows-screen.pbm';scripts=Path(__file__).resolve().parent
stop='timeout';deadline=time.monotonic()+a.timeout; end=a.offset; tail=b''
with a.log.open('rb') as src, out.open('xb') as dst:
 src.seek(a.offset)
 while time.monotonic()<deadline:
  b=src.read(1048576)
  if b: dst.write(b);dst.flush();end+=len(b)
  else: time.sleep(1);continue
  # Only complete frames are accepted by the existing CRC-checked decoder.
  if b'A04SCE ' in tail+b:
   r=subprocess.run(['python3',str(scripts/'decode-windows-screen.py'),'--log',str(out),'--output',str(screen),'--expected-size',a.expected_size],capture_output=True,text=True)
   if r.returncode==0:stop='screen_complete';break
  tail=(tail+b)[-7:]
  if out.stat().st_size>32*1024*1024:stop='capture_limit';break
r=subprocess.run(['python3',str(scripts/'analyze-ge8300-render.py'),'--log',str(out),'--offset','0','--end',str(end-a.offset)],capture_output=True,text=True) if end>a.offset else None
verdict=json.loads(r.stdout) if r and r.stdout else {'verdict':'inconclusive'}
result={'start_offset':a.offset,'end_offset':end,'stop':stop,'screen_crc_complete':screen.exists(),'render':verdict,'reset_sent':False,'restoration_attempted':False,'dma_stopped':False}
(a.output_dir/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));raise SystemExit(0 if stop=='screen_complete' and verdict.get('verdict')=='spatial_match_morton_assumed' else 1)
