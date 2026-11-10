#!/usr/bin/env python3
"""Actual main/state machine with fake transport only; never contacts a device."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

repo=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('existing_hold_tests',Path(__file__).with_name('gpu368_terminal_hold_test.py'))
existing=importlib.util.module_from_spec(spec);spec.loader.exec_module(existing)
m=existing.m

class TimingFixture(existing.Fixture):
 def __init__(self,root):
  super().__init__(root,existing.terminal(),profile='run209')
  image=self.write('matched-boot.img',b'ANDROID!host-only-never-written-to-a-device')
  self.c.update(matched_boot='matched-boot.img',matched_boot_sha256=m.sha(image))
  self.fail_preflight=False;self.fail_boot=False
 def fake_run(self,args,**kwargs):
  name=Path(kwargs['stdout'].name).name
  if '-preflight-' in name:
   self.calls.append((name,args))
   assert 'dd if=' not in args[2] and 'adb reboot' not in args[2]
   if self.fail_preflight:raise subprocess.CalledProcessError(1,args)
   return subprocess.CompletedProcess(args,0)
  if name=='matched-boot-write.log':
   self.calls.append((name,args))
   assert 'prepare-write.log' not in self.names()
   assert 'of=/dev/block/by-name/boot' in args[2]
   assert 'adb reboot' not in args[2]
   if self.fail_boot:raise subprocess.CalledProcessError(1,args)
   return subprocess.CompletedProcess(args,0)
  return super().fake_run(args,**kwargs)
 def latch(self):
  return self.root/('port/gsi/logs/ios-feasibility/gpu369-run209-executions/'+self.k['transaction']+'.json')

class TimingControllerTest(unittest.TestCase):
 def setUp(self):
  (repo/'build').mkdir(exist_ok=True)
  self.temp=tempfile.TemporaryDirectory(dir=repo/'build',prefix='timing-controller-')
  self.root=Path(self.temp.name)
 def tearDown(self):self.temp.cleanup()
 def test_matched_boot_once_before_maintenance_and_collect_never_reexecutes(self):
  f=TimingFixture(self.root);f.run()
  names=f.names();self.assertLess(names.index('matched-boot-write.log'),names.index('prepare-write.log'))
  self.assertTrue(f.result()['boot_written']);self.assertFalse(f.result()['gpt_written'])
  self.assertEqual(f.result()['status'],m.HOLD_STATUS)
  self.assertFalse(f.result()['hardware_success'])
  original={p:p.read_bytes() for p in (f.out/'result.json',f.out/'terminal-proof.json',f.latch())}
  f.calls.clear();f.run('--collect-only')
  self.assertNotIn('matched-boot-write.log',f.names())
  self.assertNotIn('prepare-write.log',f.names())
  self.assertFalse(any('-preflight-' in n for n in f.names()))
  for p,data in original.items():self.assertEqual(p.read_bytes(),data)
 def test_stale_recovery_rejected_before_latch_or_any_write(self):
  f=TimingFixture(self.root);f.fail_preflight=True
  with self.assertRaises(subprocess.CalledProcessError):f.run()
  self.assertFalse(f.out.exists());self.assertFalse(f.latch().exists())
  self.assertFalse(any(n.endswith('-write.log') for n in f.names()))
 def test_uncertain_boot_write_never_proceeds_to_maintenance_or_normal(self):
  f=TimingFixture(self.root);f.fail_boot=True
  with self.assertRaises(subprocess.CalledProcessError):f.run()
  self.assertTrue(f.latch().exists());self.assertIsNone(f.result()['boot_written'])
  self.assertEqual(f.result()['status'],'pipeline-stopped')
  self.assertNotIn('prepare-write.log',f.names());self.assertNotIn('normal-esp-write.log',f.names())

if __name__=='__main__':unittest.main()
