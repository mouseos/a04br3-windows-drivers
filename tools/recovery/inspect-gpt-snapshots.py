#!/usr/bin/env python3
"""Read-only GPT snapshot inspector. Never opens a block device."""
import argparse
import hashlib
import json
from pathlib import Path
import stat
import struct
import uuid
import zlib

ROOT=Path(__file__).resolve().parents[2]
WORKSPACE=ROOT.parents[1]
SECTOR=512

def load(p):
    p=p.resolve(strict=True)
    if WORKSPACE not in p.parents or not stat.S_ISREG(p.stat().st_mode):
        raise ValueError('snapshot must be a regular workspace file')
    return p.read_bytes()

def header(b,offset):
    raw=bytearray(b[offset:offset+SECTOR])
    if len(raw)!=SECTOR or raw[:8]!=b'EFI PART':raise ValueError('GPT signature')
    size,crc=struct.unpack_from('<II',raw,12)
    if not 92<=size<=SECTOR:raise ValueError('header size')
    raw[16:20]=b'\0'*4
    if zlib.crc32(raw[:size])!=crc:raise ValueError('header CRC')
    current,other,first,last=struct.unpack_from('<QQQQ',raw,24)
    table,n,esize,ecrc=struct.unpack_from('<QIII',raw,72)
    if n==0 or n>4096 or esize<128 or esize>4096 or esize%8:raise ValueError('entry geometry')
    return {'current':current,'other':other,'first':first,'last':last,
            'disk_guid':str(uuid.UUID(bytes_le=bytes(raw[56:72]))),
            'table':table,'count':n,'entry_size':esize,'entry_crc':ecrc}

def inspect(primary,secondary):
    p=header(primary,SECTOR);s=header(secondary,len(secondary)-SECTOR)
    if (p['current']!=1 or s['current']!=p['other'] or s['other']!=1 or
        any(p[k]!=s[k] for k in ('first','last','disk_guid','count','entry_size','entry_crc'))):
        raise ValueError('primary/secondary mismatch')
    length=p['count']*p['entry_size']
    pa=primary[p['table']*SECTOR:p['table']*SECTOR+length]
    secondary_start=s['current']-(len(secondary)//SECTOR-1)
    so=(s['table']-secondary_start)*SECTOR
    sa=secondary[so:so+length]
    if len(pa)!=length or len(sa)!=length or pa!=sa or zlib.crc32(pa)!=p['entry_crc']:
        raise ValueError('partition array mismatch or CRC')
    entries=[]
    for i in range(p['count']):
        e=pa[i*p['entry_size']:(i+1)*p['entry_size']]
        if not any(e[:16]):continue
        first,last,attrs=struct.unpack_from('<QQQ',e,32)
        if first<p['first'] or last>p['last'] or first>last:raise ValueError('partition outside usable disk')
        entries.append({'name':e[56:128].decode('utf-16le').split('\0',1)[0],
                        'first_lba':first,'last_lba':last,'bytes':(last-first+1)*SECTOR,
                        'type_guid':str(uuid.UUID(bytes_le=e[:16])),
                        'unique_guid':str(uuid.UUID(bytes_le=e[16:32])), 'attributes':attrs})
    entries.sort(key=lambda x:x['first_lba'])
    gaps=[]; cursor=p['first']
    for e in entries:
        if e['first_lba']<cursor:raise ValueError('overlapping partitions')
        if e['first_lba']>cursor:gaps.append({'first_lba':cursor,'last_lba':e['first_lba']-1,'bytes':(e['first_lba']-cursor)*SECTOR})
        cursor=e['last_lba']+1
    if cursor<=p['last']:gaps.append({'first_lba':cursor,'last_lba':p['last'],'bytes':(p['last']-cursor+1)*SECTOR})
    return {'disk_guid':p['disk_guid'],'first_usable_lba':p['first'],'last_usable_lba':p['last'],
            'primary_sha256':hashlib.sha256(primary).hexdigest(), 'secondary_sha256':hashlib.sha256(secondary).hexdigest(),
            'partitions':entries,'unallocated_gaps':gaps,
            'note':'snapshot only; gap contents, current device state and reuse safety not established'}

def main():
    a=argparse.ArgumentParser();a.add_argument('--primary',type=Path,required=True);a.add_argument('--secondary',type=Path,required=True);args=a.parse_args()
    print(json.dumps(inspect(load(args.primary),load(args.secondary)),indent=2))
if __name__=='__main__':main()
