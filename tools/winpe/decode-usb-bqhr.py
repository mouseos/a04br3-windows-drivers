#!/usr/bin/env python3
"""Decode a complete A04BQHR UART capture without accepting partial frames."""
import argparse, json, re, struct
from pathlib import Path
PAT=re.compile(r'A04BQHR SNAP ([0-9A-Fa-f]{8}) ([0-8])((?: [0-9A-Fa-f]{8}){4})(?:\r?$)')

def decode(text):
 runs={}
 for line in text.splitlines():
  m=PAT.search(line)
  if not m: continue
  ident=m.group(1).upper();part=int(m.group(2));words=tuple(int(x,16) for x in m.group(3).split())
  parts=runs.setdefault(ident,{})
  if part in parts and parts[part]!=words:raise ValueError('conflicting frames for '+ident)
  parts[part]=words
 result=[]
 for ident,parts in sorted(runs.items()):
  if 0 not in parts:raise ValueError('missing status for '+ident)
  state,ntstatus,_,_=parts[0]
  item={'id':ident,'transport_result':state,'ntstatus':f'0x{ntstatus:08X}'}
  if state in (3,4):
   if set(parts)!=set(range(9)):raise ValueError('incomplete record for '+ident)
   raw=b''.join(struct.pack('<4I',*parts[i]) for i in range(1,9))
   words=struct.unpack('<16Q',raw);seal=14695981039346656037
   for byte in raw[:120]:seal=((seal^byte)*1099511628211)&0xffffffffffffffff
   if words[0]!=0x314f524147484334 or words[1]!=128 or words[13] or words[14] or words[15]!=seal:raise ValueError('invalid record seal or structure for '+ident)
   if any(words[i]>255 for i in (7,8,10)) or words[12]>6:raise ValueError('invalid masks or reason for '+ident)
   if (state==3)!=(words[2]==2):raise ValueError('inconsistent state for '+ident)
   names=('magic','size','state','last_counter','counter_frequency','exit_counter','kicks','faults','first_valid','first_raw','latest_valid','latest_raw','reason','reserved1','reserved2','seal')
   item['record']={k:f'0x{v:016X}' for k,v in zip(names,words)}
   item['raw_hex']=raw.hex()
  elif set(parts)!={0}:raise ValueError('unexpected record after failed status for '+ident)
  result.append(item)
 if not result:raise ValueError('no BQHR frames found')
 return result

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('uart_log',type=Path);a=p.parse_args()
 print(json.dumps(decode(a.uart_log.read_text(errors='replace')),ensure_ascii=False,indent=2))
if __name__=='__main__': main()
