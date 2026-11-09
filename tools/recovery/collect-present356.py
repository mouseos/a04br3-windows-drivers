#!/usr/bin/env python3
"""Main-owned mounted Recovery volume only; fixed readonly Present evidence, no device state changes."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path

def collect(serial,out,runner=subprocess.run):
 contract=json.loads(Path(__file__).with_name('present356-collection-contract.json').read_text());base='/tmp/rt125-diag/MT8167DXGIPresent';rx=re.compile(contract['basename_regex'])
 def adb(*args):return runner(['adb','-s',serial,*args],check=True,capture_output=True).stdout
 if out.exists():raise ValueError('New evidence directory required')
 out.mkdir(parents=True,mode=0o700);records=[];errors=[]
 try:
  present=adb('shell','sh','-c',"'if [ -d /tmp/rt125-diag/MT8167DXGIPresent ]; then echo PRESENT; else echo ABSENT; fi'").decode().strip()
  if present=='ABSENT':
   errors.append('Present directory absent; no success proof')
  else:
   if present!='PRESENT':raise ValueError('Unknown fixed directory response')
   names=adb('shell','find',base,'-maxdepth','1','-type','f').decode().splitlines()
   if len(names)>contract['max_files']:raise ValueError('File count bound')
   total=0
   for remote in sorted(names):
    if not remote.startswith(base+'/') or not rx.fullmatch(remote[len(base)+1:]):raise ValueError('Unknown Present evidence name')
    name=remote[len(base)+1:];size=int(adb('shell','stat','-c','%s',remote).decode().strip())
    if size<0 or size>(contract['raw_bin_exact_bytes'] if name.endswith('.bin') else contract['max_log_bytes']):raise ValueError('Size bound')
    total+=size
    if total>contract['max_total_bytes']:raise ValueError('Total bound')
    # Bounded stream; unlike adb pull, never write beyond admitted size if source changes.
    proc=subprocess.Popen(['adb','-s',serial,'exec-out','cat',remote],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    data=proc.stdout.read(size+1)
    if len(data)!=size:proc.kill();proc.communicate();raise ValueError('Source size changed')
    _,err=proc.communicate(timeout=30)
    if proc.returncode:raise ValueError('Readonly stream failed')
    if int(adb('shell','stat','-c','%s',remote).decode().strip())!=size:raise ValueError('Source changed after capture')
    (out/name).write_bytes(data);(out/name).chmod(0o600);records.append({'path':name,'bytes':size,'sha256':hashlib.sha256(data).hexdigest(),'partial_binary':name.endswith('.bin') and size!=contract['raw_bin_exact_bytes']})
 except Exception as e:errors.append(str(e))
 finally:
  (out/'capture-index.json').write_text(json.dumps({'schema':1,'files':records,'errors':errors,'read_only':True,'mount_owner':'main','hardware_success_asserted':False},indent=2)+'\n')
 if errors:raise ValueError('; '.join(errors))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--serial',required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();collect(a.serial,a.out)
