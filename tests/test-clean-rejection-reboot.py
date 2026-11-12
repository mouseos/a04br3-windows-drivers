#!/usr/bin/env python3
"""WinPE再起動gateだけをcmd.exeで検証する。実rebootコマンドは生成しない。"""
import argparse,importlib.util,json,os,subprocess,sys
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--directory',type=Path,required=True);ap.add_argument('--emit-only',action='store_true');a=ap.parse_args()
p=Path(__file__).resolve().parents[1]/'tools/winpe/build-private-peripheral-batch-media.py';sys.path.insert(0,str(p.parent));s=importlib.util.spec_from_file_location('clean_reject_media',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
template='set "A04_CANDIDATE_STARTED=0"\necho UPDATE_STATUS=BASELINE_REJECTED_NO_CHANGE\necho UPDATE_STATUS=COMMITTED\nset "A04_RC=0"\nset "A04_RELEASE_RC=%errorlevel%"\nX:\\a04br3\\uart-audit-stream.exe report\nexit /b %A04_RC%\n'
default=m.reboot_committed_stage(template)
assert 'a04_clean_baseline_reboot' not in default
code=m.reboot_committed_stage(template,True).split('set "A04_STREAM_RC=%errorlevel%"\n',1)[1]
lines=code.splitlines();assert sum('wpeutil.exe reboot' in x for x in lines)==2
safe=[]
for line in lines:
 if 'wpeutil.exe reboot' in line:safe+=['echo REBOOT_REACHED','exit /b 0']
 else:safe.append(line)
body='\r\n'.join(safe)+'\r\n';assert 'wpeutil' not in body and 'uart-audit-stream' not in body
cases=[('clean-reject',6,0,0,0,True,0),('started-reject',6,1,0,0,False,6),('unreleased-reject',6,0,1,0,False,7),('unreported-reject',6,0,0,1,False,8),('other-error',2,0,0,0,False,2),('commit',0,1,0,0,True,0),('unreleased-commit',0,1,1,0,False,7),('unreported-commit',0,1,0,1,False,8)]
assert not a.directory.exists();a.directory.mkdir(parents=True)
rows=[]
for name,rc,started,release,stream,reboot,expected_rc in cases:
 head='@echo off\r\nsetlocal EnableExtensions DisableDelayedExpansion\r\n'+''.join('set "'+k+'='+str(v)+'"\r\n' for k,v in [('A04_RC',rc),('A04_CANDIDATE_STARTED',started),('A04_RELEASE_RC',release),('A04_STREAM_RC',stream)])
 path=a.directory/(name+'.cmd');path.write_bytes((head+body).encode('ascii'));row=dict(name=name,file=path.name,expected_reboot=reboot,expected_exit=expected_rc)
 if not a.emit_only:
  if os.name!='nt':raise RuntimeError('cmd.exe execution needs Windows; use --emit-only elsewhere')
  run=subprocess.run(['cmd.exe','/d','/c',str(path)],capture_output=True,text=True,timeout=10)
  assert ('REBOOT_REACHED' in run.stdout)==reboot and run.returncode==expected_rc,(name,run.stdout,run.stderr,run.returncode)
  row.update(actual_exit=run.returncode,actual_reboot_marker='REBOOT_REACHED' in run.stdout)
 rows.append(row)
(a.directory/'gate-cases.json').write_text(json.dumps(dict(schema='CLEAN_REJECT_REBOOT_GATE_CASES_V1',real_reboot_command_present=False,cases=rows),indent=2)+'\n')
print('Safe command-gate cases emitted' if a.emit_only else 'Windows command-gate cases passed; no hardware reboot')
