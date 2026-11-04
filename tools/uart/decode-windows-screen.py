#!/usr/bin/env python3
"""Reassemble a complete A04SC capture from an existing UART log."""
import argparse, re, zlib
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--log',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--expected-size',help='require WIDTHxHEIGHT')
a=p.parse_args()
expected=None
if a.expected_size:
    m=re.fullmatch(r'([1-9][0-9]{0,3})x([1-9][0-9]{0,3})',a.expected_size)
    if not m: p.error('invalid expected size')
    expected=tuple(map(int,m.groups()))
if a.output.exists(): raise SystemExit('output already exists')
frames={}
for line in a.log.read_bytes().splitlines():
    m=re.search(rb'A04SC0 ([0-9A-F]{8}) ([0-9A-F]{4}) ([0-9A-F]{4}) ([0-9A-F]{4})$',line)
    if m:
        ident=m[1];w,h,total=(int(x,16) for x in m.groups()[1:]);size=((w+7)//8)*h
        if 0<w<=2048 and 0<h<=2048 and 0<size<=512*1024 and total==(size+23)//24 and (expected is None or expected==(w,h)):
            frames[ident]={'w':w,'h':h,'total':total,'size':size,'parts':{},'bad':False}
        continue
    m=re.search(rb'A04SC1 ([0-9A-F]{8}) ([0-9A-F]{4}) ([0-9A-F]{2}) ([0-9A-F]+) ([0-9A-F]{8})$',line)
    if m and m[1] in frames:
        f=frames[m[1]];seq=int(m[2],16);n=int(m[3],16)
        if seq>=f['total'] or n!=min(24,f['size']-seq*24) or len(m[4])!=2*n: continue
        data=bytes.fromhex(m[4].decode())
        if zlib.crc32(data)!=int(m[5],16):continue
        if seq in f['parts'] and f['parts'][seq]!=data:f['bad']=True
        f['parts'][seq]=data
        continue
    m=re.search(rb'A04SCE ([0-9A-F]{8}) ([0-9A-F]{8})$',line)
    if not m or m[1] not in frames:continue
    f=frames[m[1]]
    if f['bad'] or len(f['parts'])!=f['total']:continue
    data=b''.join(f['parts'][i] for i in range(f['total']))
    if len(data)!=f['size'] or zlib.crc32(data)!=int(m[2],16):continue
    with a.output.open('xb') as out:
        out.write(f"P4\n{f['w']} {f['h']}\n".encode());out.write(bytes(b^255 for b in data))
    print(f"保存: {a.output} {f['w']}x{f['h']} id={m[1].decode()}")
    break
else: raise SystemExit('完全なフレームがありません（欠落・混入・CRC不一致）')
