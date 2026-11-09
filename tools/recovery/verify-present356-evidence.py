#!/usr/bin/env python3
"""Offline evidence join; never promotes pixel match alone to sameboot/OS/hardware success."""
import argparse,hashlib,json,re,struct
from pathlib import Path

def fnv(b):
 h=2166136261
 for v in b:h=((h^v)*16777619)&0xffffffff
 return h

def verify(root,launcher_pid):
 index=json.loads((root/'capture-index.json').read_text());assert not index['errors'];files={}
 for x in index['files']:
  assert re.fullmatch(r'(?:launch356|present356)-[0-9a-f]{8}-[0-9a-f]{8}(?:\.log|-[01]\.bin)',x['path'])
  q=root/x['path'];assert q.stat().st_size==x['bytes'] and hashlib.sha256(q.read_bytes()).hexdigest()==x['sha256'];files[x['path']]=q
 def one(pattern):
  a=[n for n in files if re.fullmatch(pattern,n)];assert len(a)==1,('Exact one evidence file',pattern,a);return a[0]
 def log(n):
  vals={}
  for line in files[n].read_text().splitlines():
   m=re.fullmatch(r'([A-Z0-9_]+)=0x([0-9a-fA-F]{8})',line)
   if m:vals.setdefault(m[1],[]).append(int(m[2],16))
  return vals
 def single(v,k):assert len(v[k])==1,k;return v[k][0]
 launch=one(r'launch356-%08x-[0-9a-f]{8}\.log'%launcher_pid);l=log(launch);assert single(l,'LAUNCHER_PID')==launcher_pid
 assert single(l,'FIXED_EVALUATOR_SHA256_MATCH')==1 and single(l,'EVALUATOR_EXIT')==0 and single(l,'LAUNCH_RESULT')==0 and single(l,'CHILD_WAIT')==0
 pid=single(l,'EVALUATOR_PID');session=single(l,'EVALUATOR_SESSION');assert session!=0
 name=one(r'present356-%08x-[0-9a-f]{8}\.log'%pid);v=log(name);assert single(v,'PID')==pid and int(name.split('-')[2].split('.')[0],16)==single(v,'TICK')
 assert single(v,'RESULT')==1 and single(v,'RESULT_HRESULT')==0
 assert v['STANDARD_PRESENT_HRESULT']==[0,0] and v['SCANOUT_OBSERVE_STATUS']==[0,0] and v['SCANOUT_PIXEL_MISMATCH']==[0,0]
 frames=[]
 for slot,color in enumerate((0xffff00ff,0xff00ff00)):
  bn=name[:-4]+'-'+str(slot)+'.bin';assert bn in files;b=files[bn].read_bytes();assert len(b)==4096064
  h=struct.unpack('<16I',b[:64]);assert h[:8]==(4096064,1,0x36355347,3,800,1280,3200,4096000)
  assert h[10]==h[11] and h[10]!=0 and h[14:]==(0,0)
  assert h[12]==fnv(b[64:]) and b[64:]==struct.pack('<I',color)*1024000
  frame=h[8]|h[9]<<32;assert frame>0 and (not frames or frame>frames[-1]['frame_serial'])
  assert v['SCANOUT_FRAME_LOW'][slot]==h[8] and v['SCANOUT_FRAME_HIGH'][slot]==h[9] and v['SCANOUT_FENCE'][slot]==h[10] and v['SCANOUT_PIXEL_HASH'][slot]==h[12] and v['SCANOUT_PFN_HASH'][slot]==h[13]
  frames.append({'slot':slot,'frame_serial':frame,'completed_fence':h[10],'submitted_fence':h[11],'pixel_sha256':hashlib.sha256(b[64:]).hexdigest(),'all_pixels':1024000,'mismatch':0,'pfn_hash':h[13]})
 return {'schema':1,'launcher_pid':launcher_pid,'evaluator_pid':pid,'evaluator_session_launcher_reported':session,'frame_evidence':frames,'raw_join_and_pixels_valid':True,'sameboot_module_FW_and_runtime_join_required':True,'hardware_success_asserted':False,'general_DWM_complete':False}
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--directory',type=Path,required=True);a.add_argument('--launcher-pid',type=int,required=True);a.add_argument('--output',type=Path,required=True);p=a.parse_args();assert not p.output.exists()
 try:r=verify(p.directory,p.launcher_pid)
 except Exception as e:r={'raw_join_and_pixels_valid':False,'error':str(e),'hardware_success_asserted':False}
 p.output.write_text(json.dumps(r,indent=2)+'\n');raise SystemExit(0 if r['raw_join_and_pixels_valid'] else 1)
