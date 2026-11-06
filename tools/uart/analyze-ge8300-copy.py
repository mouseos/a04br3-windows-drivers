#!/usr/bin/env python3
"""固定m1.11コピーの単一起動ログを解析し、復元の可否を記録する。UARTは開かない。"""
import argparse, hashlib, json, re, sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
HEX=r'([0-9A-F]{8})'
PATTERNS={
 'power':r'A04GPU NATIVE POWER='+HEX,
 'start':r'A04GPU NATIVE START STATUS='+HEX,
 'fence':r'A04GPU NATIVE FENCE='+HEX,
 'context':r'A04GPU CONTEXT INDEX='+HEX+r' STATUS='+HEX,
 'copy':r'A04GPU COPY STATUS='+HEX+r' MATCH='+HEX+r' GUARD='+HEX+r' PHASE='+HEX+r' UFO='+HEX+r' EXPECT='+HEX,
 'cleanup':r'A04GPU CONTEXT CLEANUP STATUS='+HEX+r' FAILED='+HEX+r' MASK='+HEX,
 'idle':r'A04GPU FORCED IDLE='+HEX,
 'off':r'A04GPU NATIVE OFF='+HEX,
}

def digest(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def analyze(data):
 # One complete, exact line only. UART interleaving cannot be repaired safely.
 lines=data.splitlines(keepends=True); records={k:[] for k in PATTERNS}; events=[]; malformed=[]; boot=[]; pos=0
 for raw in lines:
  full=raw.endswith(b'\n'); line=raw.rstrip(b'\r\n')
  if line==b'A04GPU NATIVE POST ENTER':boot.append(pos)
  if line in (b'A04VOL_LISTENER_READY',b'A04VOL_RELEASED',b'A04VOL_RELEASE_FAILED',b'A04VOL_RELEASE_UNKNOWN',b'A04VOL_LAUNCH_FAILED'):
   if full:events.append((pos,line.decode()))
  if b'A04GPU COPY NOT PREPARED' in line:malformed.append('copy_not_prepared')
  if b'A04GPU BUGCHECK' in line or b'A04WIN BUGCHECK' in line:malformed.append('bugcheck')
  for key,pattern in PATTERNS.items():
   try: match=re.fullmatch(pattern,line.decode('ascii')) if full else None
   except UnicodeDecodeError:match=None
   if match:records[key].append((pos,tuple(int(v,16) for v in match.groups())))
   elif line.startswith(('A04GPU '+ ('NATIVE POWER=' if key=='power' else 'NATIVE START STATUS=' if key=='start' else 'NATIVE FENCE=' if key=='fence' else 'CONTEXT INDEX=' if key=='context' else 'COPY STATUS=' if key=='copy' else 'CONTEXT CLEANUP STATUS=' if key=='cleanup' else 'FORCED IDLE=' if key=='idle' else 'NATIVE OFF=')).encode()):malformed.append('malformed_'+key)
  pos+=len(raw)
 problems=[]; missing=[]
 if len(boot)!=1:problems.append('開始マーカーが1個ではありません')
 if data.count(b'A04BR3 recovery: EBS')!=1:problems.append('EBSが1回ではありません')
 elif len(boot)==1 and (data.find(b'A04BR3 recovery: EBS')>boot[0] or b'BOOT_REASON' in data[data.find(b'A04BR3 recovery: EBS'):]):problems.append('再起動または起動順序が不正')
 for key in ('power','start','fence','copy','cleanup','idle','off'):
  if len(records[key])==0:missing.append(key)
  elif len(records[key])!=1:problems.append(key+'の重複')
 if len(records['context'])!=4 or [x[1][0] for x in records['context']]!=[0,1,2,3]:
  if not records['context']:missing.append('context')
  else:problems.append('contextの件数または順序が不正')
 for key in ('power','start','fence','idle','off'):
  if len(records[key])==1 and records[key][0][1][0]!=0:problems.append(key+'失敗')
 if any(v[1][1]!=0 for v in records['context']):problems.append('context失敗')
 if len(records['copy'])==1:
  status,match,guard,phase,ufo,expected=records['copy'][0][1]
  if status or match!=1024 or guard or phase!=4 or ufo!=expected or expected==0:problems.append('copy/fence/guard不一致')
 if len(records['cleanup'])==1:
  status,failed,mask=records['cleanup'][0][1]
  if status or failed!=4 or mask!=15:problems.append('cleanup不一致')
 if malformed:problems.extend(sorted(set(malformed)))
 ordered=['power','start','fence','context','copy','cleanup','idle','off']
 if not missing and not problems and len(boot)==1:
  flat=[boot[0]]+[records[k][0][0] for k in ordered]
  if flat!=sorted(flat) or records['context'][-1][0]>=records['copy'][0][0]:problems.append('診断イベントの順序が不正')
 ready=[x for x in events if x[1]=='A04VOL_LISTENER_READY']
 released=[x for x in events if x[1]=='A04VOL_RELEASED']
 failures=[x for x in events if x[1] in ('A04VOL_RELEASE_FAILED','A04VOL_RELEASE_UNKNOWN','A04VOL_LAUNCH_FAILED')]
 ebs=data.find(b'A04BR3 recovery: EBS')
 release_ok=(len(released)==1 and ebs>=0 and data.count(b'A04BR3 recovery: EBS')==1
             and any(ebs<r[0]<released[0][0] for r in ready) and not failures
             and b'BOOT_REASON' not in data[ebs:])
 verdict='失敗' if problems else ('不明' if missing else '一致')
 return {'判定':verdict,'異常':problems,'欠落':missing,'記録':{k:[{'offset':p,'values':[f'{n:08X}' for n in vals]} for p,vals in v] for k,v in records.items()},'解除':{'READY数':len(ready),'RELEASED数':len(released),'失敗応答数':len(failures),'完全受信':release_ok},'復元ゲート':{'C解除確認':release_ok,'GPU_DMA停止証明':False,'自動resetまたは書込み':False,'理由':'C:解除・fence・cleanup・OFFはfabric DMA停止を証明しない。候補の復旧書込みやresetは実行しない。'}}

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--log',type=Path,required=True);p.add_argument('--start',type=int,required=True,help='対象起動の先頭byte offset');p.add_argument('--end',type=int,required=True,help='対象起動の末尾byte offset（exclusive）');p.add_argument('--output',type=Path,required=True)
 p.add_argument('--restore-boot',type=Path);p.add_argument('--restore-boot-sha256');p.add_argument('--restore-esp',type=Path);p.add_argument('--restore-esp-sha256')
 a=p.parse_args();log=a.log.resolve();out=a.output.resolve()
 if not log.is_relative_to(ROOT) or not out.is_relative_to(ROOT) or out.exists() or not out.parent.is_dir():p.error('ログと新規出力は作業ルート内に置く')
 if a.start<0 or a.end<=a.start or a.end>log.stat().st_size:p.error('対象範囲が不正')
 with log.open('rb') as f:f.seek(a.start);data=f.read(a.end-a.start)
 if a.start and data[:1] and not _at_line_start(log,a.start):p.error('開始位置が行頭ではありません')
 report=analyze(data);report['入力']={'sha256':hashlib.sha256(data).hexdigest(),'start':a.start,'end':a.end}
 images={}
 for name,path,want in (('boot',a.restore_boot,a.restore_boot_sha256),('esp',a.restore_esp,a.restore_esp_sha256)):
  if bool(path)!=bool(want):p.error(name+'の復元画像とSHAを両方指定してください')
  if path:
   actual=path.resolve()
   if not actual.is_relative_to(ROOT) or not actual.is_file() or not re.fullmatch('[0-9a-fA-F]{64}',want):p.error(name+'の入力不正')
   got=digest(actual);images[name]={'path':str(actual),'sha256':got,'一致':got==want.lower()}
 report['復元画像']=images
 if any(not x['一致'] for x in images.values()):report['異常'].append('復元画像SHA不一致');report['判定']='失敗'
 with out.open('x',encoding='utf-8') as f:json.dump(report,f,ensure_ascii=False,indent=2);f.write('\n')
 print(json.dumps(report,ensure_ascii=False,indent=2));return {'一致':0,'失敗':1,'不明':2}[report['判定']]

def _at_line_start(path,offset):
 with path.open('rb') as f:f.seek(offset-1);return f.read(1)==b'\n'
if __name__=='__main__':sys.exit(main())
