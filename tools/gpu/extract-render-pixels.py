#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Recover one complete linear RGBA capture; never accept incomplete UART data."""
import argparse
import binascii
import hashlib
import json
import os
import pathlib
import re
import sys

BEGIN = re.compile(rb'A04GPU PIXELS BEGIN LEN=([0-9A-F]{8}) CRC=([0-9A-F]{8})')
CHUNK = re.compile(rb'A04GPU PIXELS OFF=([0-9A-F]{8}) CRC=([0-9A-F]{8}) DATA=([0-9A-F]{128})')
END = re.compile(rb'A04GPU PIXELS END LEN=([0-9A-F]{8}) CRC=([0-9A-F]{8})')

def extract(data):
    begins=list(BEGIN.finditer(data))
    if len(begins)!=1:
        raise ValueError('select exactly one boot and complete capture using the UART offset')
    b=begins[0]; ends=list(END.finditer(data,b.end()))
    if len(ends)!=1: raise ValueError('missing or ambiguous end')
    e=ends[0]
    if int(b[1],16)!=16384 or b.groups()!=e.groups(): raise ValueError('capture header/trailer mismatch')
    chunks={}
    for match in CHUNK.finditer(data,b.end(),e.start()):
        off=int(match[1],16); raw=bytes.fromhex(match[3].decode('ascii'))
        if off%64 or off>=16384 or off in chunks: raise ValueError('invalid or duplicate offset')
        if binascii.crc32(raw)!=int(match[2],16): raise ValueError('fragment CRC mismatch')
        chunks[off]=raw
    if len(chunks)!=256: raise ValueError('missing fragment')
    result=b''.join(chunks[off] for off in range(0,16384,64))
    if binascii.crc32(result)!=int(b[2],16): raise ValueError('whole CRC mismatch')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('uart',type=pathlib.Path);parser.add_argument('output',type=pathlib.Path)
    parser.add_argument('--offset',type=int,default=0)
    a=parser.parse_args()
    if a.offset<0: raise ValueError('negative offset')
    with a.uart.open('rb') as f:
        f.seek(a.offset); pixels=extract(f.read())
    fd=os.open(a.output,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'wb') as f:f.write(pixels)
    print(json.dumps({'bytes':len(pixels),'width':64,'height':64,'stride':256,
                      'format':'RGBA byte order as emitted','crc32':'%08X'%binascii.crc32(pixels),
                      'sha256':hashlib.sha256(pixels).hexdigest()}))

if __name__=='__main__':
    try: main()
    except (ValueError,OSError) as exc:
        print(str(exc),file=sys.stderr);sys.exit(1)
