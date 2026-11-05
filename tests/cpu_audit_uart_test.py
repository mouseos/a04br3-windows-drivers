#!/usr/bin/env python3
import importlib.util,zlib
from pathlib import Path
root=Path(__file__).resolve().parents[1];spec=importlib.util.spec_from_file_location('cpu',root/'tools/recovery/decode-cpu-audit-uart.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
t='0123456789ABCDEF'; payload=b'TOPRGU_SYS_MATCH=NO\r\nTOPRGU_AUDIT_EXIT=0\r\n'; c=zlib.crc32(payload)
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
