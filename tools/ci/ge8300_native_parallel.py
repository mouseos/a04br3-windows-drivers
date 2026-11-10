"""Explicit same-invocation native resume; no SDK/compiler cache rebuild."""
import argparse,concurrent.futures,datetime,hashlib,json,os,subprocess,sys,time,uuid
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def now():return datetime.datetime.now(datetime.timezone.utc).isoformat()
def require(v,msg):
 if not v:raise ValueError(msg)
def invoke(exe,args,logs,tag):
 logs.mkdir(parents=True,exist_ok=True);start=time.monotonic();utc=now()
 stdout=logs/(tag+'.stdout.log');stderr=logs/(tag+'.stderr.log')
 with stdout.open('wb') as so,stderr.open('wb') as se:
  p=subprocess.run([str(exe),*args],stdout=so,stderr=se)
 row={'tool':str(exe),'arguments':args,'exit_code':p.returncode,'started_utc':utc,'ended_utc':now(),'elapsed_seconds':time.monotonic()-start,'stdout':str(stdout),'stderr':str(stderr),'stdout_sha256':sha(stdout),'stderr_sha256':sha(stderr)}
 (logs/(tag+'.receipt.json')).write_text(json.dumps(row,indent=2)+'\n');return row

def main():
 if len(sys.argv)>1 and sys.argv[1]=='--verify-live-plan':
  import ctypes
  plan=read(sys.argv[2]);live=read(sys.argv[3]);argc=ctypes.c_int()
  api=ctypes.windll.shell32.CommandLineToArgvW;api.argtypes=[ctypes.c_wchar_p,ctypes.POINTER(ctypes.c_int)];api.restype=ctypes.POINTER(ctypes.c_wchar_p)
  ptr=api(live['CommandLine'],ctypes.byref(argc));actual=[ptr[i] for i in range(argc.value)]
  ctypes.windll.kernel32.LocalFree(ctypes.cast(ptr,ctypes.c_void_p))
  matches=[r for r in plan['units'] if r['arguments']==actual[1:]]
  require(len(matches)==1 and actual[0].lower()==plan['compiler'].lower(),'Live original compiler arguments differ from generated plan')
  print(json.dumps({'live_process_id':live['ProcessId'],'arguments_identical':True,'unit':matches[0]['unit'],'units':len(plan['units'])}));return 0
 if len(sys.argv)>1 and sys.argv[1]=='--invoke':
  _,_,exe,logs,*args=sys.argv
  row=invoke(exe,args,Path(logs),uuid.uuid4().hex)
  sys.stdout.buffer.write(Path(row['stdout']).read_bytes());sys.stdout.buffer.flush()
  if row['exit_code']:print('Compiler failed; retained stderr: '+row['stderr'],file=sys.stderr)
  return row['exit_code']
 p=argparse.ArgumentParser();p.add_argument('--enable-resume',action='store_true');p.add_argument('--plan',type=Path,required=True);p.add_argument('--handoff',type=Path,required=True);p.add_argument('--jobs',type=int,choices=range(1,5),default=1);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 require(a.enable_resume and os.name=='nt','Explicit Windows resume only');plan=read(a.plan);handoff=read(a.handoff)
 require(handoff['success'] and handoff['source_commit']==plan['source_commit']=='30cb20f79ec3c5f71c056d5783fe6743e92bf2ce','Original fixed source handoff')
 require(handoff['selected_exit_code']==0,'In-flight TU must complete successfully')
 known={str(Path(x['path']).resolve()).lower():x for x in handoff['completed_objects']};paths={str(Path(x['object']).resolve()).lower() for x in plan['units']}
 require(set(known)<=paths,'Unexpected retained objects');tool=Path(plan['compiler']);pinned={x['path'].lower():x['sha256'] for x in handoff['tool_hashes']};require(sha(tool)==pinned[str(tool).lower()],'Compiler changed')
 for key,row in known.items():require(sha(row['path'])==row['sha256'],'Retained object changed')
 for path in handoff.get('incomplete_output_paths',[]):
  f=Path(path)
  if f.exists():
   dest=a.output/'interrupted-successor';dest.mkdir(exist_ok=True);require(not (dest/f.name).exists(),'Interrupted object collision');f.rename(dest/f.name)
 todo=[r for r in plan['units'] if str(Path(r['object']).resolve()).lower() not in known]
 for r in todo:require(not Path(r['object']).exists(),'Unproven preexisting output '+r['object'])
 logs=a.output/'parallel-compile-logs';logs.mkdir(exist_ok=True)
 def compile_unit(row):
  obj=Path(row['object']);dep=logs/(obj.stem+'.d');args=[*row['arguments'],'-MD','-MF',str(dep)]
  result=invoke(tool,args,logs,obj.stem);require(result['exit_code']==0,'Compile failed '+row['unit']);require(obj.is_file() and dep.is_file(),'Object and original compile-time dependency trace required')
  result.update(unit=row['unit'],object_sha256=sha(obj),dependency_trace=str(dep),dependency_trace_sha256=sha(dep),include_trace_recorded=True)
  (logs/(obj.stem+'.receipt.json')).write_text(json.dumps(result,indent=2)+'\n');return result
 results=[]
 with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
  for row in pool.map(compile_unit,todo):results.append(row)
 require(sha(tool)==pinned[str(tool).lower()],'Compiler changed during batch')
 for row in known.values():require(sha(row['path'])==row['sha256'],'Retained object was recompiled/changed')
 receipt={'schema':'native30cb-direct-io-parallel-resume-v1','source_commit':plan['source_commit'],'jobs':a.jobs,'handoff_sha256':sha(a.handoff),'plan_sha256':sha(a.plan),'completed_objects_retained':len(known),'objects_compiled':len(results),'retained_objects_recompiled':0,'historical_exit_codes_recorded':False,'historical_include_traces_recorded':False,'metadata_reconstructed_after_controlled_handoff':True,'SDK_rebuilt':0,'Mesa365_recompiled':0,'commands':results,'success':True}
 (a.output/'parallel-resume.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({k:v for k,v in receipt.items() if k!='commands'}))
 return 0
if __name__=='__main__':raise SystemExit(main())
