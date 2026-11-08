#!/usr/bin/env python3
"""Decode load identity; never symbolize old PCs against a later boot's base."""
import argparse,json,struct,datetime,hashlib
from pathlib import Path

def decode(data):
 if len(data)!=144:raise ValueError('Exact144-byte module identity required')
 schema,size,bits,reserved,base,image_size,flags,wall,uptime,source,build=struct.unpack('<4IQ2I2Q48s48s',data)
 if schema!=1 or size!=144 or bits!=32 or reserved or flags!=1 or not base or not image_size or base+image_size>2**32:raise ValueError('Invalid ARM32 load identity')
 def string(b):
  z=b.find(b'\0')
  if z<0 or any(b[z:]):raise ValueError('NUL-padded fixed identity required')
  return b[:z].decode('ascii')
 source=string(source);build=string(build)
 if len(source)!=40 or any(x not in '0123456789abcdef' for x in source):raise ValueError('Exact source base commit required')
 epoch=datetime.datetime(1601,1,1,tzinfo=datetime.timezone.utc)
 return {'schema':'ge8300-module-identity-v1','record_sha256':hashlib.sha256(data).hexdigest(),'driver_start':hex(base),'driver_size':hex(image_size),'pointer_bits':bits,'load_FILETIME':wall,'load_utc':(epoch+datetime.timedelta(microseconds=wall//10)).isoformat(),'interrupt_time_100ns':uptime,'boot_epoch_estimate_FILETIME':wall-uptime,'runtime_source_base':source,'build_id':build,'same_boot_PC_correlation_proven':False,'not_valid_for_old_run143_PC':True,'rootfix_proven':False}

def main():
 p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 if a.out.exists():raise ValueError('New output required')
 result=decode(a.binary.read_bytes());a.out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
