#!/usr/bin/env python3
"""Decode a complete nonce-scoped UART report; never control the target."""
import argparse, hashlib, json, re, zlib
from pathlib import Path
LIMIT=16384

def decode(data, token):
    token=token.upper()
    if not re.fullmatch(r'[0-9A-F]{16}',token): raise ValueError('invalid token')
    t=token.encode(); starts=[]; chunks={}; endings=[]; state='before'
    for line in data.splitlines():
        m=re.fullmatch(rb'A04CPU0 '+t+rb' ([0-9A-F]{8}) ([0-9A-F]{8})',line)
        if m:
            if state!='before': raise ValueError('out-of-order header')
            state='body'
            starts.append((int(m[1],16),int(m[2],16)));continue
        m=re.fullmatch(rb'A04CPU1 '+t+rb' ([0-9A-F]{4}) ([0-9A-F]{2}) ([0-9A-F]+) ([0-9A-F]{8})',line)
        if m:
            if state!='body': raise ValueError('out-of-order chunk')
            seq,n=int(m[1],16),int(m[2],16); chunk=bytes.fromhex(m[3].decode())
            if not 1<=n<=16 or len(chunk)!=n or zlib.crc32(chunk)!=int(m[4],16): raise ValueError('bad chunk')
            if seq in chunks and chunks[seq]!=chunk: raise ValueError('conflicting chunk')
            chunks[seq]=chunk;continue
        m=re.fullmatch(rb'A04CPUE '+t+rb' ([0-9A-F]{8})',line)
        if m:
            if state!='body': raise ValueError('out-of-order end')
            state='after'; endings.append(int(m[1],16))
    if len(starts)!=1 or len(endings)!=1: raise ValueError('missing or repeated frame boundary')
    size,expected=starts[0]
    if not 1<=size<=LIMIT or endings[0]!=expected: raise ValueError('bad size or final CRC')
    total=(size+15)//16
    if set(chunks)!=set(range(total)): raise ValueError('missing or unexpected chunk')
    result=b''.join(chunks[i] for i in range(total))
    if len(result)!=size or zlib.crc32(result)!=expected: raise ValueError('whole report mismatch')
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('uart_log',type=Path);p.add_argument('--offset',type=int,required=True);p.add_argument('--token',required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();data=a.uart_log.read_bytes()
    if a.offset<0 or a.offset>=len(data) or a.output.exists():p.error('invalid offset or output exists')
    try: result=decode(data[a.offset:],a.token)
    except ValueError as e:print(json.dumps({'status':'incomplete','reason':str(e)}));return 1
    a.output.write_bytes(result)
    print(json.dumps({'status':'complete-frame-only','bytes':len(result),'sha256':hashlib.sha256(result).hexdigest(),'audit_exit_recorded':bool(re.search(rb'^TOPRGU_AUDIT_EXIT=0\r?$',result,re.M)),'warning':'Frame completion does not establish signature acceptance, PnP or safe reset.'},indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
