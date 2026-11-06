#!/usr/bin/env python3
"""捕捉器自身のTLV形式を検査する。実GPU成功の判定は行わない。"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

def parse(raw):
    pos=0;events=[];pending=set();seen=set()
    while pos<len(raw):
        if len(raw)-pos<32:raise ValueError("truncated header")
        magic,version,kind,seq,rc,error,count=struct.unpack_from("<8sIIIiiI",raw,pos);pos+=32
        if magic!=b"A04RGCAP" or version!=1 or kind not in (1,2) or not 1<=seq<=32:
            raise ValueError("invalid header")
        if count!=(5 if kind==1 else 1):raise ValueError("invalid component count")
        components={}
        for unused in range(count):
            if len(raw)-pos<8:raise ValueError("truncated TLV")
            tag,size=struct.unpack_from("<II",raw,pos);pos+=8
            if tag in components or size>0x1000 or size>len(raw)-pos:raise ValueError("invalid TLV")
            components[tag]=raw[pos:pos+size];pos+=size
        event={"phase":"pre" if kind==1 else "post","seq":seq,"ioctl_result":rc,
               "errno_only_if_failed":error if rc<0 else None}
        if kind==1:
            if seq in seen or set(components)!={1,2,3,4,5}:raise ValueError("pre pairing")
            if [len(components[x]) for x in (1,2,3,5)]!=[32,0x160,0x88,0x150]:raise ValueError("payload lengths")
            if len(components[4]) not in (0,0x150):raise ValueError("PR length")
            pending.add(seq);seen.add(seq)
            event["payloads"]={}
            for tag,name in [(3,"ta"),(4,"pr"),(5,"three")]:
                data=components[tag]
                event["payloads"][name]={"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest(),
                    "hex":data.hex(),"words32":[f"0x{x[0]:08x}" for x in struct.iter_unpack("<I",data)]}
            event["input_hex"]=components[2].hex()
        else:
            if seq not in pending or set(components)!={6} or len(components[6])!=12:raise ValueError("post pairing")
            pending.remove(seq)
            event["output_hex"]=components[6].hex()
            event["bridge_error"]=struct.unpack_from("<I",components[6],8)[0] if rc>=0 else None
        events.append(event)
    if pending or not events:raise ValueError("incomplete or empty capture")
    return {"capture_sha256":hashlib.sha256(raw).hexdigest(),"events":events,
            "gpu_success":"unknown: also require original harness readback and phase logs"}

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument("capture",type=Path)
    args=ap.parse_args()
    try:result=parse(args.capture.read_bytes())
    except (ValueError,OSError) as exc:ap.error(str(exc))
    print(json.dumps(result,indent=2))
if __name__=="__main__":main()
