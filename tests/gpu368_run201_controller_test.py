#!/usr/bin/env python3
"""UART状態機械だけのhost検証。adb/fastboot/runner/実機を呼ばない。"""
import hashlib,importlib.util
from pathlib import Path
repo=Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('gpu201',repo/'tools/recovery/run-gpu368-fresh200.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
def event(i,e,code):return ('PB %08X %08X %08X'%(i,e,code)).encode()
def start(o,run=11,pid=22):
 o.line(b'A04TRACE ExitBootServices event signaled');o.line(event(0,0x286,run));o.line(event(0,0x287,pid))
def installed(o):
 start(o);o.line(event(0,0x288,0));o.line(event(0,0x207,3010));o.line(event(0,0x10b,3010));actions=o.line(b'BOOT_REASON=00000001');assert ('planned-restart',1) in actions;return o

def main():
 # Install -> one planned warm reboot -> two distinct clients -> safe boundary.
 o=installed(m.Observation());assert ('resume',1) in o.line(b'fastboot: processing commands');start(o,33,44)
 for i in (0,1):o.line(event(i,0x220,100+i));o.line(event(i,0x221,0))
 o.line(event(0,0x288,0));assert ('terminal',0) in o.line(event(0,0x207,0));assert o.terminal['PID']==44
 # Missing flush cannot grant reset/recovery; second restart isn't accepted.
 o=m.Observation();start(o);assert ('unproven-terminal',1) in o.line(event(0,0x207,1))
 o=installed(m.Observation());start(o,33,44);assert ('unexpected-restart',2) in o.line(b'BOOT_REASON=00000001')
 # Failure evidence can be collected even when Present was skipped, helper ran.
 o=installed(m.Observation());start(o,33,44);o.line(event(0,0x221,21));o.line(event(1,0x220,101));o.line(event(1,0x221,0));o.line(event(0,0x288,0));assert ('terminal',21) in o.line(event(0,0x207,21))
 # A terminal0 without both actual client completions must fail closed.
 o=installed(m.Observation());start(o,33,44);o.line(event(0,0x288,0))
 try:o.line(event(0,0x207,0));raise AssertionError('false success accepted')
 except ValueError:pass
 # Restart missing the SCM waiter evidence is not automatically resumed.
 o=m.Observation();start(o);o.line(event(0,0x288,0));o.line(event(0,0x207,3010));assert ('unproven-restart',1) in o.line(b'BOOT_REASON=00000001');assert not o.await_resume
 # Duplicated client start never becomes a second test attempt.
 o=installed(m.Observation());start(o,33,44);o.line(event(0,0x220,100))
 try:o.line(event(0,0x220,101));raise AssertionError('client replay accepted')
 except ValueError:pass
 # Pending seal rejects before source/device lookup or output creation.
 path=repo/'tools/recovery/gpu368-run201-seal.pending.json'
 try:m.validate_seal(repo,path,hashlib.sha256(path.read_bytes()).hexdigest());raise AssertionError('pending seal accepted')
 except ValueError:pass
 print('GPU201_UART_BOUNDARIES_AND_PENDING_SEAL_PASS_NO_DEVICE')
if __name__=='__main__':main()
