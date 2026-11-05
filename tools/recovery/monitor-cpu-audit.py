#!/usr/bin/env python3
"""Passively wait for a complete UART report; never reset or access a device."""
import argparse, importlib.util, json, re, time
from pathlib import Path
spec=importlib.util.spec_from_file_location('cpu_audit_decoder',Path(__file__).with_name('decode-cpu-audit-uart.py'))
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
p=argparse.ArgumentParser();p.add_argument('uart_log',type=Path);p.add_argument('--offset',required=True,type=int);p.add_argument('--token',required=True);p.add_argument('--output',required=True,type=Path);p.add_argument('--timeout',type=int,default=1800)
a=p.parse_args(); workspace=Path(__file__).resolve().parents[4]; output=a.output.resolve()
if workspace not in output.parents or output.exists() or a.offset<0 or not 1<=a.timeout<=43200 or not re.fullmatch('[0-9A-F]{16}',a.token):p.error('invalid argument or output path')
deadline=time.monotonic()+a.timeout
while True:
 try: data=a.uart_log.read_bytes()
 except OSError as e: print(json.dumps({'status':'log-unavailable','reason':str(e)}));raise SystemExit(2)
 if len(data)<a.offset:print(json.dumps({'status':'log-truncated'}));raise SystemExit(2)
 try: payload=mod.decode(data[a.offset:],a.token)
 except ValueError as e:
  if str(e) not in ('missing or repeated frame boundary','missing or unexpected chunk'):
   print(json.dumps({'status':'invalid-frame','reason':str(e)}));raise SystemExit(2)
 else:
  with output.open('xb') as f:f.write(payload)
  audit_ok=bool(re.search(rb'^TOPRGU_AUDIT_EXIT=0\r?$',payload,re.M))
  print(json.dumps({'status':'complete-report','audit_exit_zero':audit_ok,'note':'No reset or restore was initiated.'}));raise SystemExit(0 if audit_ok else 3)
 if time.monotonic()>=deadline:print(json.dumps({'status':'incomplete-timeout','note':'No reset or restore was initiated.'}));raise SystemExit(4)
 time.sleep(1)
