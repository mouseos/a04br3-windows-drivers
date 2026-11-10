#!/usr/bin/env python3
"""Verify ordered UART report fragments without controlling the device."""
import argparse, hashlib, json, re, zlib
from pathlib import Path
LIMIT=1048576

def decode(data, token):
    token=token.upper()
    if not re.fullmatch(r'[0-9A-F]{16}',token): raise ValueError('invalid token')
    t=token.encode(); state=0; result=bytearray(); seq=0; size=expected=None
    for line in data.splitlines():
        m=re.fullmatch(rb'A04AU20 '+t+rb' ([0-9A-F]{8}) ([0-9A-F]{8})',line)
        if m:
            if state!=0: raise ValueError('repeated or out-of-order header')
            size,expected=int(m[1],16),int(m[2],16)
            if not 1<=size<=LIMIT: raise ValueError('invalid size')
            state=1;continue
        m=re.fullmatch(rb'A04AU21 '+t+rb' ([0-9A-F]{8}) ([0-9A-F]{2}) ([0-9A-F]+) ([0-9A-F]{8})',line)
        if m:
            if state!=1 or int(m[1],16)!=seq: raise ValueError('missing, repeated or out-of-order chunk')
            n=int(m[2],16);chunk=bytes.fromhex(m[3].decode())
            if n!=min(16,size-len(result)) or len(chunk)!=n or n==0 or zlib.crc32(chunk)!=int(m[4],16): raise ValueError('invalid chunk')
            result.extend(chunk);seq+=1;continue
        m=re.fullmatch(rb'A04AU2E '+t+rb' ([0-9A-F]{8})',line)
        if m:
            if state!=1 or int(m[1],16)!=expected or len(result)!=size or zlib.crc32(result)!=expected: raise ValueError('invalid end')
            state=2
    if state!=2: raise ValueError('incomplete report')
    return bytes(result)

def main():
    p=argparse.ArgumentParser();p.add_argument('uart_log',type=Path);p.add_argument('--offset',type=int,required=True);p.add_argument('--token',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    data=a.uart_log.read_bytes()
    if a.offset<0 or a.offset>=len(data) or a.output.exists():p.error('invalid offset or output exists')
    try: result=decode(data[a.offset:],a.token)
    except ValueError as e: print(json.dumps({'status':'incomplete','reason':str(e)}));return 1
    with a.output.open('xb') as f:f.write(result)
    print(json.dumps({'status':'frame-complete-only','bytes':len(result),'sha256':hashlib.sha256(result).hexdigest()}));return 0
if __name__=='__main__':raise SystemExit(main())
