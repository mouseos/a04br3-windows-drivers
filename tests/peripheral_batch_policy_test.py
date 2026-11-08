#!/usr/bin/env python3
"""Private-artifact host validation only. Never contacts a device or loads a hive."""
import copy,importlib.util,json,sys,unittest,tempfile
from pathlib import Path
REPO=Path(__file__).resolve().parents[1];ROOT=REPO.parents[1]
sys.dont_write_bytecode=True;sys.path.insert(0,str(REPO/'tools/winpe'))
import peripheral_batch_policy as policy
spec=importlib.util.spec_from_file_location('batch_media',REPO/'tools/winpe/build-private-peripheral-batch-media.py');media=importlib.util.module_from_spec(spec);spec.loader.exec_module(media)
class BatchContract(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.path=ROOT/'port/xnu/out/windows-rt-peripheral-batch-candidates-draft-v2/package-map-DRAFT-UNPINNED.json'
  if not cls.path.exists():raise unittest.SkipTest('Private signed artifacts and draft map required')
  cls.draft=json.loads(cls.path.read_text());cls.template=(REPO/'tools/winpe/run-mt8167-shared-io-update.cmd.in').read_text()
 def test_explicit_capture_selection(self):
  boot='12345678-1234-1234-1234-123456789abc'
  selection={'schema':1,'profile':policy.PROFILE,'capture_number':114,'manifest_sha256':'1'*64,'recovery_boot_id':boot,'gpu_policy_sha256':'2'*64,'unchanged_since_capture':True}
  manifest='port/xnu/out/private-windows-rt-prebackup-run114/manifest.json'
  policy.validate_selection(selection,manifest,'1'*64,boot)
  for field,value in [('capture_number',112),('capture_number',True),('manifest_sha256','3'*64),('recovery_boot_id','00000000-1234-1234-1234-123456789abc'),('gpu_policy_sha256','bad'),('unchanged_since_capture',False)]:
   bad=dict(selection);bad[field]=value
   with self.subTest(field=field,value=value),self.assertRaises(ValueError):policy.validate_selection(bad,manifest,'1'*64,boot)
  with self.assertRaises(ValueError):policy.validate_selection(selection,manifest.replace('114','116'),'1'*64,boot)
 def test_native_capture_crosslinks(self):
  scratch=ROOT/'port/gsi/logs/ios-feasibility/windows-rt-peripheral-batch-source/scratch';scratch.mkdir(parents=True,exist_ok=True)
  with tempfile.TemporaryDirectory(dir=scratch) as td:
   root=Path(td);d=root/'private-windows-rt-prebackup-run114';d.mkdir()
   boot='12345678-1234-1234-1234-123456789abc';entries=[];trio={}
   for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2'):
    p=d/n;p.write_bytes(n.encode());trio[n]=policy.sha(p);entries.append({'source_path':'Windows/System32/config/'+n,'backup_path':n,'sha256':trio[n],'bytes':p.stat().st_size})
   mf=d/'manifest.json';mf.write_text(json.dumps({'status':'captured-unmounted','read_only':True,'unmounted':True,'errors':[],'recovery_boot_id':boot,'files':entries}))
   selection={'schema':1,'profile':policy.PROFILE,'capture_number':114,'manifest_sha256':policy.sha(mf),'recovery_boot_id':boot,'gpu_policy_sha256':'2'*64,'unchanged_since_capture':True}
   sp=root/'selection.json';sp.write_text(json.dumps(selection))
   native={'source_manifest_sha256':policy.sha(mf),'source_files':trio,'recovery_boot_id':boot,'gpu_policy_sha256':'2'*64,'original_unchanged':True,'errors':[],'load_rc':0,'unload_rc':0,'privilege_restore_error':0}
   bp=root/'bindings.json';bp.write_text(json.dumps(native))
   f={'manifest':str(mf),'sha256':policy.sha(mf),'bindings':str(bp),'bindings_sha256':policy.sha(bp),'main_selection':str(sp),'main_selection_sha256':policy.sha(sp)}
   policy.bind_capture(f,root)
   extra=root/'supplement';extra.mkdir();rows=[]
   for i,name in enumerate(('Windows/System32/MT8167RenderSingleGpu.exe','Windows/System32/ge8300-umd.dll','Windows/System32/Drivers/ge8300-mips.fw')):
    q=extra/str(i);q.write_bytes(name.encode());rows.append({'source_path':name,'backup_path':str(i),'sha256':policy.sha(q),'bytes':q.stat().st_size})
   sm=extra/'manifest.json';supp={'schema':1,'source_manifest_sha256':policy.sha(mf),'recovery_boot_id':boot,'status':'captured-unmounted','read_only':True,'unmounted':True,'errors':[],'files':rows}
   sm.write_text(json.dumps(supp));sf={**f,'supplement':str(sm),'supplement_sha256':policy.sha(sm)};policy.bind_capture(sf,root)
   for field,value in [('source_manifest_sha256','0'*64),('recovery_boot_id','other'),('unmounted',False),('read_only',False),('errors',['failure']),('files',rows[:2])]:
    bad=dict(supp);bad[field]=value;sm.write_text(json.dumps(bad));sf['supplement_sha256']=policy.sha(sm)
    with self.subTest(supplement=field),self.assertRaises(ValueError):policy.bind_capture(sf,root)
   for field,value in [('gpu_policy_sha256','3'*64),('recovery_boot_id','00000000-1234-1234-1234-123456789abc'),('original_unchanged',False),('unload_rc',5),('load_rc',False),('source_files',{})]:
    bad=dict(native);bad[field]=value;bp.write_text(json.dumps(bad));f['bindings_sha256']=policy.sha(bp)
    with self.subTest(field=field),self.assertRaises(ValueError):policy.bind_capture(f,root)
 def test_six_actual_signed_candidates(self):
  result=policy.packages(self.draft['packages'],ROOT)
  self.assertEqual(list(result),['rtc','thermal','usb','mma','emmc','parent'])
  self.assertEqual(result['emmc']['version'],'0.0.1.9');self.assertEqual(result['mma']['version'],'0.0.1.10')
 def test_fresh_not_bound_cannot_load(self):
  with self.assertRaisesRegex(ValueError,'main指定capture未結合draft'):policy.load_map(self.path,ROOT)
 def test_draft_cannot_generate_header(self):
  with self.assertRaises(ValueError):policy.generate(self.draft)
 def test_boolean_flip_does_not_authorize_header(self):
  p=copy.deepcopy(self.draft);p.update(draft=False,normal_install_allowed=True)
  with self.assertRaises(ValueError):policy.generate(p)
 def test_no_gpu_or_old_driver_role(self):
  for extra in ('render','audio','wifi','display','parent105x'):
   p=copy.deepcopy(self.draft['packages']);p[extra]=p['usb']
   with self.subTest(extra=extra),self.assertRaises(ValueError):policy.packages(p,ROOT)
 def test_missing_each_package_rejected(self):
  for role in policy.TARGETS:
   p=copy.deepcopy(self.draft['packages']);del p[role]
   with self.subTest(role=role),self.assertRaises(ValueError):policy.packages(p,ROOT)
 def test_manifest_pin_rejected_for_each(self):
  for role in policy.TARGETS:
   p=copy.deepcopy(self.draft['packages']);p[role]['manifest_sha256']='0'*64
   with self.subTest(role=role),self.assertRaises(ValueError):policy.packages(p,ROOT)
 def test_wrong_source_rejected_for_each(self):
  for role in policy.TARGETS:
   p=copy.deepcopy(self.draft['packages']);p[role]['source_commit']='0'*40
   with self.subTest(role=role),self.assertRaises(ValueError):policy.packages(p,ROOT)
 def test_bad_actual_file_pin_each_role_each_member(self):
  for role in policy.TARGETS:
   for member in ('inf','sys','cat'):
    p=copy.deepcopy(self.draft['packages']);p[role]['files'][member]['sha256']='0'*64
    with self.subTest(role=role,member=member),self.assertRaises(ValueError):policy.packages(p,ROOT)
 def test_manifest_member_mixing_rejected(self):
  for role in policy.TARGETS:
   p=copy.deepcopy(self.draft['packages']);p[role]['files']['sys']['manifest_member']='ge8300-render.sys'
   with self.subTest(role=role),self.assertRaises(ValueError):policy.packages(p,ROOT)
 def test_fixed_certificate_pin_rejected(self):
  p=copy.deepcopy(self.draft['packages']);p['usb']['certificate']['sha256']='0'*64
  with self.assertRaises(ValueError):policy.packages(p,ROOT)
 def test_stage_frame_has_no_old_installer_trust_or_rollback(self):
  cmd=media.stage_script(self.template,['if exist C:\\Windows\\System32\\MT8167PeripheralBatch goto baselinefail','set "A04_CANDIDATE_STARTED=1"','> "%A04_BK%\\JOURNAL.txt" echo STATE=CANDIDATE_WRITE_STARTED'],'1234567890abcdef','0'*64)
  self.assertNotIn('dism.exe',cmd.lower());self.assertNotIn('reg.exe',cmd.lower());self.assertNotIn('RESTORE_FROM',cmd)
  self.assertIn('STATE=PRECHECK',cmd);self.assertIn('STATE=BASELINE_REJECTED_NO_CHANGE',cmd);self.assertIn('release-offline-volume.exe',cmd)
 def test_stage_rejects_reg_and_old_dism(self):
  for line in ('reg.exe load HKLM\\Bad C:\\Windows\\System32\\config\\SOFTWARE','dism.exe /Image:C:\\ /Add-Driver old.inf'):
   with self.subTest(line=line),self.assertRaises(ValueError):media.stage_script(self.template,[line],'1234567890abcdef','0'*64)
if __name__=='__main__':unittest.main()
