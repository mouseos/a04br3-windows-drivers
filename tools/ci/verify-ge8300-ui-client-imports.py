#!/usr/bin/env python3
"""Captured RT8.1 PE exports/API-set recursive forwarders only; no disassembly."""
from pathlib import Path
import argparse,json,hashlib,struct,pefile
ap=argparse.ArgumentParser();ap.add_argument('--project-root',type=Path,required=True);ap.add_argument('--candidate',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
R=a.project_root.resolve();O=R/'port/xnu/out/windows-rt-present356-r2';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert a.candidate.resolve().is_relative_to(R) and a.out.resolve().is_relative_to(R)
dirs=[O/'rt81-exports',R/'port/xnu/out/private-windows-rt-run155-kernel-images',R/'port/xnu/out/private-windows-rt-run91-loader-dlls',R/'port/xnu/out/private-windows-rt-display-rotation-probe-exports',R/'port/xnu/out/windows-rt-peripheral-next122-packet-v2/rt81-exports',R/'port/xnu/out/windows-rt-dxgkrnl-rea']
files={}
for d in dirs:
 for p in d.iterdir():
  if p.is_file()and p.suffix.lower()in ['.dll','.sys','.exe']:files.setdefault(p.name.lower(),p)
export_cache={};used={};missing=[]
schema_path=O/'rt81-exports/apisetschema.dll';schema_pe=pefile.PE(str(schema_path));schema_data=next(x.get_data()for x in schema_pe.sections if x.Name.startswith(b'.apiset'))
version,schema_size,schema_flags,schema_count=struct.unpack_from('<4I',schema_data);assert version==4 and schema_size<=len(schema_data)
def utf16(offset,length):
 assert length%2==0 and offset+length<=schema_size
 return schema_data[offset:offset+length].decode('utf-16le')
api_sets={}
for index in range(schema_count):
 flags,no,nl,alias,alen,data=struct.unpack_from('<6I',schema_data,16+index*24);name=utf16(no,nl);vflags,n=struct.unpack_from('<2I',schema_data,data);values=[]
 for j in range(n):
  vf,ao,al,ho,hl=struct.unpack_from('<5I',schema_data,data+8+j*20);values.append((utf16(ao,al).lower(),utf16(ho,hl).lower()))
 api_sets[name.lower()]=values
used['apisetschema.dll']=dict(path=str(schema_path.relative_to(R)),sha256=sha(schema_path),machine='ARMNT',schema_version=version,namespace_count=schema_count,scope='official installed-image API-set namespace metadata, not instruction decoding')
def exported(module,name,seen=()):
 module=module.lower()
 if '.'not in module:module+='.dll'
 if(module,name)in seen:return {'status':'forwarder-cycle'}
 if module.startswith(('api-','ext-')):
  key=module[4:-4];values=api_sets.get(key,[]);importer=seen[-1][0]if seen else ''
  named=[host for alias,host in values if alias and alias==importer];default=[host for alias,host in values if not alias];hosts=named or default
  if len(hosts)!=1 or not hosts[0]:return dict(status='unresolved-api-set',module=module,symbol=name)
  child=exported(hosts[0],name,seen+((module,name),));return dict(status=child['status'],api_set_contract=module,actual_schema_host=hosts[0],schema_sha256=sha(schema_path),target=child)
 p=files.get(module)
 if not p:return {'status':'module-image-missing','module':module,'symbol':name}
 if module not in export_cache:
  q=pefile.PE(str(p));assert q.FILE_HEADER.Machine==0x1c4,module
  table={}
  for x in q.DIRECTORY_ENTRY_EXPORT.symbols:
   if x.name:table[x.name.decode()]=x
   table['#'+str(x.ordinal)]=x
  export_cache[module]=table;used[module]=dict(path=str(p.relative_to(R)),sha256=sha(p),machine='ARMNT')
 x=export_cache[module].get(name)
 if not x:return {'status':'export-missing','module':module,'symbol':name}
 if x.forwarder:
  f=x.forwarder.decode();target,symbol=f.rsplit('.',1);child=exported(target,symbol,seen+((module,name),));return dict(status=child['status'],module=module,symbol=name,forwarder=f,target=child)
 return dict(status='present',module=module,symbol=name)

p=a.candidate;q=pefile.PE(str(p));assert q.FILE_HEADER.Machine==0x1c4
assert (q.OPTIONAL_HEADER.MajorSubsystemVersion,q.OPTIONAL_HEADER.MinorSubsystemVersion)==(6,3)
rows=[]
for entry in q.DIRECTORY_ENTRY_IMPORT:
 for x in entry.imports:
  module=entry.dll.decode();name=x.name.decode() if x.name else '#'+str(x.ordinal)
  proof=exported(module,name);rows.append(dict(module=module,symbol=name,validation=proof))
  if proof['status']!='present':missing.append(proof)
a.out.write_text(json.dumps(dict(scope='Captured RT8.1 PE exports and recursive API-set/forwarders; no device read',candidate_sha256=sha(p),machine='ARMNT',subsystem=[6,3],imports=rows,images=used,missing=missing,hardware_verified=False),indent=2)+'\n')
assert not missing,missing
print('RT81_IMPORTS_ALL_RESOLVED',len(rows))
