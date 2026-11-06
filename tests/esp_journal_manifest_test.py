#!/usr/bin/env python3
import hashlib
import importlib.util
import io
from pathlib import Path
import struct
import sys

root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools/winpe'))
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
intent=struct.pack('<III',0x544e4934,1,42)+bytes(data[-32:]);intent+=hashlib.sha256(intent).digest()
content['/A04BACKUP/INTENT']=intent;files['/a04backup/intent']=('/A04BACKUP/INTENT',hashlib.sha256(intent).hexdigest())
assert m.verify_journal(Fat(content),files)['armed'] is True
commit=('A04COMMIT_V1 '+hashlib.sha256(content['/A04BACKUP/READY']).hexdigest()+'\n').encode()
content['/A04BACKUP/COMMIT']=commit;files['/a04backup/commit']=('/A04BACKUP/COMMIT',hashlib.sha256(commit).hexdigest())
assert m.verify_journal(Fat(content),files)['complete'] is True
content['/A04BACKUP/COMMIT']=b'bad'
try:m.verify_journal(Fat(content),files)
except ValueError:pass
else:raise AssertionError('invalid completion marker accepted')
content.pop('/A04BACKUP/COMMIT');files.pop('/a04backup/commit')
bad_intent=bytearray(intent);bad_intent[12]^=1;content['/A04BACKUP/INTENT']=bytes(bad_intent)
try:m.verify_journal(Fat(content),files)
except ValueError:pass
else:raise AssertionError('altered transaction intent accepted')
content.pop('/A04BACKUP/INTENT');files.pop('/a04backup/intent')
bad=bytearray(data);bad[off+72:off+328]=bytes(256);bad[-32:]=hashlib.sha256(bad[:-32]).digest();content['/A04BACKUP/READY']=bytes(bad)
try:m.verify_journal(Fat(content),files)
except ValueError:pass
else:raise AssertionError('invalid sidecar name accepted')
content,files=example(3)
data=bytearray(content['/A04BACKUP/READY']);struct.pack_into('<II',data,4,4,13)
for i,name in ((11,'FIXED-oem33.inf'),(12,'FIXED-oem35.inf')):
    off=16+i*328;payload=('fixed%d'%i).encode();struct.pack_into('<I',data,off,1);struct.pack_into('<Q',data,off+32,len(payload));data[off+40:off+72]=hashlib.sha256(payload).digest();raw=name.encode('utf-16le');data[off+72:off+72+len(raw)]=raw;content[f'/A04BACKUP/item{i:02}.bin']=payload
data[-32:]=hashlib.sha256(data[:-32]).digest();content['/A04BACKUP/READY']=bytes(data);files={k.lower():(k,hashlib.sha256(v).hexdigest()) for k,v in content.items()}
assert m.verify_journal(Fat(content),files)['version']==4
bad=bytearray(data);bad[16+11*328+72]^=1;bad[-32:]=hashlib.sha256(bad[:-32]).digest();content['/A04BACKUP/READY']=bytes(bad)
try:m.verify_journal(Fat(content),files)
except ValueError:pass
else:raise AssertionError('v4 fixed INF name mismatch accepted')
print('ESP journal manifest tests: PASS')
