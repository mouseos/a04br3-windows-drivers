#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import struct
import zlib

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('gpt_inspector',root/'tools/recovery/inspect-gpt-snapshots.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
entries=bytearray(128*128)
entries[:16]=bytes(range(1,17));entries[16:32]=bytes(range(17,33))
struct.pack_into('<QQQ',entries,32,2048,4095,0)
entries[56:56+len('DATA'.encode('utf-16le'))]='DATA'.encode('utf-16le')
crc=zlib.crc32(entries)
def hdr(current,other,table):
    h=bytearray(512);h[:8]=b'EFI PART';struct.pack_into('<IIII',h,8,0x10000,92,0,0)
    struct.pack_into('<QQQQ',h,24,current,other,34,9966);h[56:72]=bytes(range(32,48))
    struct.pack_into('<QIII',h,72,table,128,128,crc);struct.pack_into('<I',h,16,zlib.crc32(h[:92]));return h
primary=bytearray(512)+hdr(1,9999,2)+entries
secondary=entries+hdr(9999,1,9967)
r=m.inspect(bytes(primary),bytes(secondary))
assert r['partitions'][0]['name']=='DATA' and len(r['unallocated_gaps'])==2
for which in ('header','table','secondary'):
    p=bytearray(primary);s=bytearray(secondary)
    if which=='header':p[512+24]^=1
    if which=='table':p[1024+100]^=1
    if which=='secondary':s[4]^=1
    try:m.inspect(bytes(p),bytes(s))
    except ValueError:pass
    else:raise AssertionError('corruption accepted: '+which)
print('GPT snapshot validation tests: PASS')
