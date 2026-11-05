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
  finished=re.findall(rb'^TOPRGU_AUDIT_FINISHED_READ_ONLY\r?$',payload,re.M)
  exits=re.findall(rb'^TOPRGU_AUDIT_EXIT=([^\r\n]*)\r?$',payload,re.M)
  mounts=re.findall(rb'^MOUNTVOL_EXIT=([^\r\n]*)\r?$',payload,re.M)
  releases=re.findall(rb'^TOPRGU_RELEASE_EXIT=([^\r\n]*)\r?$',payload,re.M)
  released=re.findall(rb'^TOPRGU_VOLUME_RELEASED\r?$',payload,re.M)
  audit_ok=len(finished)==1 and exits==[b'0']
  dismounted=mounts==[b'0'] and releases==[b'0'] and len(released)==1
  print(json.dumps({'status':'complete-report','audit_exit_zero':audit_ok,'volume_dismount_zero':dismounted,'note':'No reset or restore was initiated.'}));raise SystemExit(0 if audit_ok and dismounted else (5 if dismounted else 3))
 if time.monotonic()>=deadline:print(json.dumps({'status':'incomplete-timeout','note':'No reset or restore was initiated.'}));raise SystemExit(4)
 time.sleep(1)
