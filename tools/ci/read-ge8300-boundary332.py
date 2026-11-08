#!/usr/bin/env python3
"""Decode fixed precommitted process boundary slots. Offline, read-only."""
import argparse,json,struct,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('file',type=Path);a=p.parse_args();b=a.file.read_bytes()
if len(b)!=4096:raise SystemExit('Expected exactly4096 bytes; no partial progress success')
labels={0:'precommitted',2:'CreateDevice before',3:'CreateDevice after',8:'Copy backend entry',9:'RT lock before',10:'RT lock after',11:'staging lock before',12:'staging lock after',13:'CPU copy before',14:'CPU copy after',15:'boundary return; GOP1 detail means RenderCb return, not CPU unlock',16:'LockCb before',17:'LockCb after',20:'client Copy before',21:'client Copy after',22:'client Flush before',23:'client Flush after',24:'client Map before',25:'client Map after'}
rows=[];invalid=[];pixel_slots=[];pids=set()
for slot in range(64):
 data=b[slot*64:slot*64+64]
 if not any(data):continue
 r=struct.unpack('<10I',data[:40]);
 if r[0]==0x32525047 and 32<=slot<=43:pixel_slots.append(slot);continue
 checksum=2166136261
 for value in r[:9]:checksum=((checksum^value)*16777619)&0xffffffff
 if r[0]!=0x31525047 or r[1]!=1 or r[3]!=slot or r[8] or checksum!=r[9] or any(data[40:]):invalid.append(slot);continue
 pids.add(r[2]);rows.append({'stage':slot,'label':labels.get(slot,'unknown'),'pid':r[2],'status':f'0x{r[4]:08x}','detail':r[5],'process_tick_ms':r[6]|r[7]<<32})
if len(pids)>1:raise SystemExit('Mixed process identity')
print(json.dumps({'schema':'GPR1-fixed-boundary332-v1','sha256':hashlib.sha256(b).hexdigest(),'records':sorted(rows,key=lambda r:(r['process_tick_ms'],r['stage'])),'invalid_or_torn_slots':invalid,'GPR2_slots_require_pixel_decoder':pixel_slots,'GOP1_detail_is_RenderCb_return_not_CPU_unlock':True,'no_record_is_not_nonexecution':True,'GPU_success_inferred':False},indent=2))
