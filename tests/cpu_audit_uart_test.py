#!/usr/bin/env python3
import importlib.util,zlib
from pathlib import Path
root=Path(__file__).resolve().parents[1];spec=importlib.util.spec_from_file_location('cpu',root/'tools/recovery/decode-cpu-audit-uart.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
t='0123456789ABCDEF'; payload=b'TOPRGU_SYS_MATCH=NO\r\nTOPRGU_AUDIT_FINISHED_READ_ONLY\r\nTOPRGU_AUDIT_EXIT=0\r\nMOUNTVOL_EXIT=0\r\n'; c=zlib.crc32(payload)
head=f'A04CPU0 {t} {len(payload):08X} {c:08X}\n'.encode();end=f'A04CPUE {t} {c:08X}\n'.encode()
parts=[]
for i in range(0,len(payload),16):
 b=payload[i:i+16];parts.append(f'A04CPU1 {t} {i//16:04X} {len(b):02X} {b.hex().upper()} {zlib.crc32(b):08X}\n'.encode())
assert m.decode(head+b''.join(parts)+end,t)==payload
for invalid in (head+b''.join(parts[:-1])+end,head+b''.join(parts),head+b''.join(parts)+end+end,head+parts[0].replace(b'544F5052',b'554F5052')+b''.join(parts[1:])+end):
 try:m.decode(invalid,t)
 except ValueError:continue
 raise AssertionError('invalid frame accepted')
print('CPU UART framing: completeness, token, chunk CRC and final CRC verified')
import subprocess,tempfile
(root/'build').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='cpu-audit-monitor-',dir=root/'build') as x:
 d=Path(x);log=d/'uart';out=d/'report';log.write_bytes(head+b''.join(parts)+end)
 monitor=root/'tools/recovery/monitor-cpu-audit.py'
 r=subprocess.run(['python3',str(monitor),str(log),'--offset','0','--token',t,'--output',str(out),'--timeout','1'],capture_output=True)
 assert r.returncode==0 and out.read_bytes()==payload
 log.write_bytes(head+parts[0]);out2=d/'incomplete'
 r=subprocess.run(['python3',str(monitor),str(log),'--offset','0','--token',t,'--output',str(out2),'--timeout','1'],capture_output=True)
 assert r.returncode==4 and not out2.exists() and b'No reset or restore' in r.stdout
 log.write_bytes(head+parts[0].replace(b'544F5052',b'554F5052')+b''.join(parts[1:])+end);out3=d/'bad'
 r=subprocess.run(['python3',str(monitor),str(log),'--offset','0','--token',t,'--output',str(out3),'--timeout','1'],capture_output=True)
 assert r.returncode==2 and not out3.exists()
print('CPU UART monitor: complete frame and timeout without reset verified')
