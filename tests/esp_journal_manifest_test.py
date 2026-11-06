#!/usr/bin/env python3
import hashlib
import importlib.util
import io
from pathlib import Path
import struct

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('journal',root/'tools/winpe/preserve-esp-journal.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Fat:
    def __init__(self,content):self.content=content
    def openbin(self,path,mode):return io.BytesIO(self.content[path])
    def getsize(self,path):return len(self.content[path])
def example(version):
    count=11;size=72 if version==1 else 328;capacity=11 if version==1 else 99
    data=bytearray(16+capacity*size+32);struct.pack_into('<IIII',data,0,0x41303442,version,count,42 if version==3 else 0)
    content={}
    for i in (0,4):
        b=('stream%d'%i).encode();off=16+i*size;struct.pack_into('<I',data,off,1);struct.pack_into('<Q',data,off+32,len(b));data[off+40:off+72]=hashlib.sha256(b).digest();content[f'/A04BACKUP/item{i:02}.bin']=b
    data[-32:]=hashlib.sha256(data[:-32]).digest();content['/A04BACKUP/READY']=bytes(data)
    files={k.lower():(k,hashlib.sha256(v).hexdigest()) for k,v in content.items()}
    return content,files
for version in (1,2,3):
    content,files=example(version);assert m.verify_journal(Fat(content),files)['version']==version
    bad=dict(content);bad['/A04BACKUP/item00.bin']+=b'x'
    try:m.verify_journal(Fat(bad),files)
    except ValueError:pass
    else:raise AssertionError('altered journal item accepted')
    bad=dict(content);bad['/A04BACKUP/READY']=bad['/A04BACKUP/READY'][:-1]+b'X'
    try:m.verify_journal(Fat(bad),files)
    except ValueError:pass
    else:raise AssertionError('altered manifest accepted')
content,files=example(3)
data=bytearray(content['/A04BACKUP/READY']);struct.pack_into('<I',data,8,12)
off=16+11*328;payload=b'tm-stream';struct.pack_into('<I',data,off,1);struct.pack_into('<Q',data,off+32,len(payload));data[off+40:off+72]=hashlib.sha256(payload).digest()
name='SYSTEM{fixture}.TM.blf'.encode('utf-16le');data[off+72:off+72+len(name)]=name;data[-32:]=hashlib.sha256(data[:-32]).digest()
content['/A04BACKUP/READY']=bytes(data);content['/A04BACKUP/item11.bin']=payload
files={k.lower():(k,hashlib.sha256(v).hexdigest()) for k,v in content.items()}
assert m.verify_journal(Fat(content),files)['count']==12
bad=bytearray(data);bad[off+72:off+328]=bytes(256);bad[-32:]=hashlib.sha256(bad[:-32]).digest();content['/A04BACKUP/READY']=bytes(bad)
try:m.verify_journal(Fat(content),files)
except ValueError:pass
else:raise AssertionError('invalid sidecar name accepted')
print('ESP journal manifest tests: PASS')
