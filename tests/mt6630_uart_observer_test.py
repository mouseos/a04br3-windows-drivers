#!/usr/bin/env python3
"""数値allowlist、秘密の非出力、未観測段階をhost fixtureで確認。"""
import importlib.util
import io
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
p=root/'tools/host/observe-mt6630-1027-uart.py'
s=importlib.util.spec_from_file_location('wifi_observer',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
secret=b'PRIVATE_SSID password=do-not-emit MAC=02:11:22:33:44:55 IP=192.0.2.42'
def event(seq,label,value,run=10):return f'A04WLAN RUN={run} SEQ={seq} IF=0 {label}={value}\n'.encode()
def read(data,**kwargs):return m.observe(io.BytesIO(data),0,len(data),**kwargs)
r=read(secret+b'\n'+event(0,'BEGIN',1)+event(1,'OPEN_ERROR',0)+event(2,'REQUEST_ACCEPTED',1)+event(3,'COMPLETE_EVENT',1)+event(4,'FAIL_EVENT',0)+event(5,'BSS_ERROR',0)+event(6,'BSS_TOTAL',3)+event(7,'SCAN_EXIT',0)+event(8,'END',0))
assert r['stages']['scan']=='scan_callback_observed' and r['scan_probe_runs'][0]['bss_list_observed']
assert all(v.startswith('unverified') for k,v in r['stages'].items() if k!='scan')
assert not r['communication_success_claimed'] and not r['checkpoint_stages'] and not r['NTSTATUS_events']
assert r['connection_test_performed']=='unknown_from_this_uart_contract' and not r['failures']
assert len(r['timeline'])==9
r=read(event(0,'BEGIN',1)+event(1,'OPEN_ERROR',5)+event(2,'FAIL_REASON',327681)+event(3,'COMPLETE_EVENT',0))
assert r['failures'][0]['scope']=='wlan_handle_not_80211_auth' and r['failures'][0]['code']==5
assert r['failures'][1]['code_kind']=='WLAN_REASON_CODE' and r['stages']['authentication'].startswith('unverified')
assert r['stages']['scan']=='scan_events_only'
for data in (secret+b'\n',b'PB 00000000 00000207 00000000\n',b'A04CP stage=19 NTSTATUS=00000000\n',event(0,'ASSOCIATION_SUCCESS',1),event(1,'KEY',123),event(2,'IP',123),event(3,'END',0).rstrip()+b' '+secret+b'\n',event(4,'END',0).rstrip()):
 r=read(data);assert not r['timeline'] and r['stages']['scan']=='unobserved_in_selected_bytes'
r=read(b'A04WIFI PNP=00000000 MATCHES=00000001 STATUS=00000008 PROBLEM=00000000\nA04WIFI FW WMT=00000100 ERR=00000000 WLAN=00000200 ERR=00000002\n')
assert len(r['timeline'])==2 and r['failures'][0]['scope']=='firmware_file_metadata_not_boot' and r['stages']['scan']=='unobserved_in_selected_bytes'
# Known storage insertion only; no raw duplication. Unknown insertion is discarded.
r=read(b'A04WLAN RUN=10 SEQ=0 IF=0 COMPLETE_A04WIN CDB=00000000 status=00000000\nEVENT=1\n')
assert r['scan_probe_runs'][0]['scan_complete_callback_observed']
assert not read(event(0,'COMPLETE_EVENT',1).replace(b'EVENT',secret+b'EVENT'))['timeline']
r=read(b'x'*10000+b'\n'+event(0,'END',0));assert len(r['timeline'])==1
r=read(event(0,'BEGIN',1)+event(1,'END',0),max_events=1);assert len(r['timeline'])==1 and r['event_limit_reached']
# A truncated start drops its first line instead of claiming a complete record.
data=event(0,'BEGIN',1)+event(1,'END',0);r=m.observe(io.BytesIO(data),1,len(data));assert len(r['timeline'])==1
# Only the input file and new output are touched. No call to a device/API/program by observer.
out=root/'build/mt6630-uart-observer-test';out.mkdir(parents=True,exist_ok=True)
input_=out/'input.log';output=out/'report.json'
input_.write_bytes(secret+b'\n'+event(0,'COMPLETE_EVENT',1))
if output.exists():output.unlink()
args=['python3','-B',str(p),'--workspace',str(root),'--uart-file',str(input_),'--output',str(output)]
run=subprocess.run(args,capture_output=True);assert run.returncode==0,run.stderr
text=output.read_bytes()+run.stdout+run.stderr
for value in (b'PRIVATE_SSID',b'do-not-emit',b'02:11:22:33:44:55',b'192.0.2.42'):assert value not in text
assert output.stat().st_mode&0o777==0o600 and input_.read_bytes().startswith(secret)
run=subprocess.run(args,capture_output=True);assert run.returncode==2 # no overwrite
print('PASS numeric-only UART snapshot: scan != connection/auth/key/port/IP; no invented checkpoint; secrets excluded; no device/VM')
