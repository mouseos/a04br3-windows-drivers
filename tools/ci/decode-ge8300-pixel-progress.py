#!/usr/bin/env python3
"""Read fixed 4096B mixed GPR1 boundary/GPR2 completed pixel evidence."""
import argparse,json,struct
from pathlib import Path

def checksum(words):
 h=2166136261
 for word in words:h=((h^word)*16777619)&0xffffffff
 return h

def decode(data):
 if len(data)!=4096:raise ValueError('expected precommitted 4096B file')
 out={'schema':'ge8300-pixel-progress-v2','boundary':[],'pixels':[],'invalid_slots':[],'hardware_result_inferred_from_map':False}
 for slot in range(64):
  row=data[64*slot:64*(slot+1)]
  if not any(row):continue
  w=struct.unpack('<16I',row)
  if w[0]==0x31525047 and w[1]==1:
   if w[3]!=slot or checksum(w[:9])!=w[9]:out['invalid_slots'].append(slot);continue
   out['boundary'].append(dict(slot=slot,pid=w[2],status=w[4],detail=w[5],process_tick_ms=w[6]|w[7]<<32));continue
  if w[0]!=0x32525047 or w[1]!=2 or checksum(w[:15])!=w[15] or w[3]!=slot or w[4]>1:
   out['invalid_slots'].append(slot);continue
  kind,case=w[5:7];expected=(32+case if kind==1 and case<2 else 34 if kind==2 and case==0 else 35 if kind==3 and case==0 else 36+case if kind==4 and case<8 else -1)
  if slot!=expected:out['invalid_slots'].append(slot);continue
  match,mismatch,a,b,c,h=w[9:15]
  if kind==1:valid=match+mismatch==480000 and b==480000;passed=mismatch==0;aux={'expected_bgra':a,'total':b}
  elif kind==2:valid=match==a+b and match+mismatch==480000;passed=a>0 and b>0 and mismatch==0 and c==0;aux={'red':a,'black':b,'sample_mismatch':c,'other':mismatch}
  else:valid=match+mismatch==a+b and a+b+c==480000;passed=(a>140000 and b>300000 if kind==3 else a>0 and b>0) and mismatch==0;aux={'interior':a,'exterior':b,'edge_excluded':c}
  if not valid or bool(w[4])!=bool(passed):out['invalid_slots'].append(slot);continue
  out['pixels'].append(dict(slot=slot,pid=w[2],kind={1:'clear',2:'triangle',3:'color',4:'texture'}[kind],case=case,process_tick_ms=w[7]|w[8]<<32,match=match,mismatch=mismatch,passed=bool(w[4]),pixel_hash=h,**aux))
 return out

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('file',type=Path);p.add_argument('--output',type=Path);a=p.parse_args();result=decode(a.file.read_bytes());text=json.dumps(result,indent=2)+'\n'
 if a.output:a.output.write_text(text)
 else:print(text,end='')
