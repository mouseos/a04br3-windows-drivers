#!/usr/bin/env python3
"""Host-only validation and actual batch control-flow; no real media/device."""
import copy,importlib.util,json,re,tempfile,unittest
from pathlib import Path
repo=Path(__file__).resolve().parents[1];here=repo/'tools/winpe'
s=importlib.util.spec_from_file_location('wifi_only',here/'build-mt6630-wifi-only-media.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class Test(unittest.TestCase):
 def setUp(self):
  (repo/'build').mkdir(exist_ok=True);self.tmp=tempfile.TemporaryDirectory(dir=repo/'build');self.root=Path(self.tmp.name);self.boot='11111111-1111-1111-1111-111111111111'
 def tearDown(self):self.tmp.cleanup()
 def backup(self):
  directory=self.root/'private-windows-rt-prebackup-run206';directory.mkdir();files=[]
  names=['Windows/System32/config/'+h+s for h in ('SYSTEM','SOFTWARE','DRIVERS')for s in ('','.LOG1','.LOG2')]+['Windows/System32/Drivers/mt6630-combo-runtime.sys']
  for name in names:
   p=directory/'files'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'HOST-FIXTURE-NOT-A-HIVE');files.append(dict(source_path=name,backup_path='files/'+name,bytes=p.stat().st_size,sha256=m.sha(p)))
  data=dict(schema_version=1,status='captured-unmounted',read_only=True,unmounted=True,errors=[],serial='0C0U265083',recovery_boot_id=self.boot,files=files);p=directory/'manifest.json';p.write_text(json.dumps(data));return p,data
 def test_actual_backup_required_exact_serial_boot_and_contents(self):
  p,b=self.backup();self.assertEqual(m.backup_check(self.root,p,m.sha(p),self.boot)['serial'],'0C0U265083')
  for key,value in [('serial','OTHER'),('read_only',False),('unmounted',False),('errors',['failed']),('recovery_boot_id','2'*36)]:
   bad=copy.deepcopy(b);bad[key]=value;p.write_text(json.dumps(bad))
   with self.assertRaises(ValueError):m.backup_check(self.root,p,m.sha(p),self.boot)
  p.write_text(json.dumps(b));first=p.parent/b['files'][0]['backup_path'];first.write_bytes(b'changed')
  with self.assertRaises(ValueError):m.backup_check(self.root,p,m.sha(p),self.boot)
  wrong=self.root/'private-windows-rt-prebackup-run205';wrong.mkdir();q=wrong/'manifest.json';q.write_text(json.dumps(b))
  with self.assertRaises(ValueError):m.backup_check(self.root,q,m.sha(q),self.boot)
 def test_singer_native_receipt_no_invented_boolean(self):
  d=self.root/'package';d.mkdir();inf='Class=Net\nCatalogFile=mt6630-combo-runtime.cat\nDriverVer=11/11/2026,2026.11.11.1029\n%Device%=Install,A04BR3\\MT6630SDIO\nAddService=Mt6630Runtime,0x2,Service\nmt6630-combo-runtime.sys\n'
  for name in m.NAMES:(d/name).write_bytes(inf.encode()if name.endswith('.inf')else b'HOST-FIXTURE-NOT-SIGNED')
  record=dict(source_commit='a'*40,signer_thumbprint=m.SIGNER,signature_status='Valid',files=[dict(path=n,sha256=m.sha(d/n))for n in m.NAMES]);p=d/'signed-manifest.json';p.write_text(json.dumps(record))
  self.assertNotIn('signature_and_catalog_verified',record);self.assertEqual(set(m.package_check(self.root,d,p,m.sha(p))[1]),set(m.NAMES))
  for key,value in [('signature_status','NotSigned'),('signer_thumbprint','F'*40)]:
   bad=dict(record);bad[key]=value;p.write_text(json.dumps(bad))
   with self.assertRaises(ValueError):m.package_check(self.root,d,p,m.sha(p))
  p.write_text(json.dumps(record));(d/m.NAMES[1]).write_bytes(b'changed')
  with self.assertRaises(ValueError):m.package_check(self.root,d,p,m.sha(p))
 def test_command_surface_only_single_wifi_dism_and_release_before_commit(self):
  text=m.render(here,'A'*16,'b'*64,self.boot).decode();self.assertNotRegex(text,r'@[A-Z_]+@');lines=text.splitlines()
  self.assertEqual(sum(x.lower().startswith('dism.exe ')for x in lines),1)
  forbidden=('reg.exe ','wpeutil','shutdown','reboot','drvload X:\\a04br3\\wifi','SetupDi','mt6630-wmt.bin','mt6630-wlan.bin')
  executable='\n'.join(x for x in lines if not x.lower().startswith('rem '))
  for x in forbidden:self.assertNotIn(x.lower(),executable.lower())
  self.assertIn('/ForceUnsigned',text);self.assertLess(text.index('release-offline-volume.exe'),text.index('echo UPDATE_STATUS=%A04_STATUS%'))
  self.assertIn('if exist "%A04_BK%" goto journalexists',text)
 def test_batch_success_failure_release_failure_and_no_second_dism(self):
  # Execute this template's real labels/IF/GOTO/SET subset with OS operations
  # mocked. This is not a Windows cmd.exe execution or a signature test.
  text=m.render(here,'A'*16,'b'*64,self.boot).decode()
  for dism,release,exists,expected,code,count in [(0,0,False,'COMMITTED','0',1),(3010,0,False,'COMMITTED','0',1),(5,0,False,'DISM_FAILED_RESTORE_FROM_HOST_BACKUP','6',1),(0,5,False,'VOLUME_RELEASE_FAILED','8',1),(0,0,True,'JOURNAL_EXISTS_NO_CHANGE','4',0)]:
   env,n=run_batch(text,dism,release,exists);self.assertEqual((env['A04_STATUS'],env['A04_RC'],n),(expected,code,count))
def run_batch(text,dism_rc,release_rc,exists):
 lines=text.splitlines();labels={x[1:]:i for i,x in enumerate(lines)if x.startswith(':')};env={'errorlevel':'0'};i=0;calls=0
 def expand(x):return re.sub(r'%([^%]+)%',lambda v:env.get(v[1],''),x)
 for _ in range(500):
  line=expand(lines[i]).strip();i+=1
  if not line or line.startswith((':','rem ','@echo','setlocal')):continue
  if line.lower().startswith('if '):
   body=line[3:];matched=False
   if body.startswith('errorlevel '):
    value,command=body[11:].split(' ',1);matched=int(env['errorlevel'])>=int(value)
   elif body.startswith('exist ')or body.startswith('not exist '):
    neg=body.startswith('not ');body=body[4:]if neg else body
    match=re.fullmatch(r'exist "([^"]+)" (.*)',body);path,command=match.groups();matched=(exists if 'wifi1029-' in path else True)^neg
   else:
    match=re.fullmatch(r'"([^"]*)"=="([^"]*)" (.*)',body)
    if match:a,b,command=match.groups();matched=a==b
    else:
     match=re.fullmatch(r'(\d+) GEQ (\d+) (.*)',body);a,b,command=match.groups();matched=int(a)>=int(b)
   if not matched:continue
   line=command
  if line.startswith('goto '):i=labels[line[5:]]+1;continue
  if line.startswith('set /a '):
   key,_,value=line[7:].partition('=')
   if key.endswith('+'):key=key[:-1];value=str(int(env[key])+int(value))
   env[key]=value;continue
  if line.startswith('set "'):key,value=line[5:-1].split('=',1);env[key]=value;continue
  if line.startswith('exit /b '):return env,calls
  if line=='cmd.exe /c exit 0':env['errorlevel']='0'
  elif line.startswith('dism.exe '):calls+=1;env['errorlevel']=str(dism_rc)
  elif 'release-offline-volume.exe' in line:env['errorlevel']=str(release_rc)
  elif line.startswith('mkdir '):exists=True;env['errorlevel']='0'
  elif line.startswith(('>','type ','copy ')):pass
  elif line.startswith('X:\\a04br3\\'):env['errorlevel']='0'
  else:raise AssertionError('unhandled batch statement: '+line)
 raise AssertionError('batch did not terminate')
if __name__=='__main__':unittest.main()
