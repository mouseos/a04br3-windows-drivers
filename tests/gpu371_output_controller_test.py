#!/usr/bin/env python3
"""既hold fixtureのfake transportだけで371分岐を検証。実seal/freshは作らない。"""
import importlib.util,subprocess,tempfile,unittest
from pathlib import Path
s=importlib.util.spec_from_file_location('hold371',Path(__file__).with_name('gpu368_terminal_hold_test.py'))
e=importlib.util.module_from_spec(s);s.loader.exec_module(e);m=e.m
class Fixture(e.Fixture):
 def __init__(self,root):super().__init__(root,e.terminal(),profile='run213')
 def fake_run(self,args,**kw):
  name=Path(kw['stdout'].name).name
  if '-preflight-' in name:
   self.calls.append((name,args));assert 'dd if=' not in args[2] and 'adb reboot' not in args[2];return subprocess.CompletedProcess(args,0)
  assert 'of=/dev/block/by-name/boot' not in str(args)
  return super().fake_run(args,**kw)
 def latch(self):return self.root/'port/gsi/logs/ios-feasibility/gpu371-run213-executions/combined-run213.json'
class OutputTest(unittest.TestCase):
 def test_known_variant_holds_then_collects_once_without_boot_or_install_reexecution(self):
  repo=Path(__file__).resolve().parents[1];(repo/'build').mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(dir=repo/'build',prefix='host-only371-') as d:
   f=Fixture(Path(d));f.run();self.assertEqual(f.result()['status'],m.HOLD_STATUS)
   self.assertFalse(f.result()['boot_written']);self.assertFalse(f.result()['gpt_written']);self.assertFalse(f.result()['hardware_success'])
   self.assertNotIn('matched-boot-write.log',f.names());self.assertNotIn('terminal-to-recovery.log',f.names())
   proof=f.latch().read_bytes();f.calls.clear();f.run('--collect-only')
   self.assertNotIn('prepare-write.log',f.names());self.assertNotIn('matched-boot-write.log',f.names());self.assertEqual(proof,f.latch().read_bytes())
   f.calls.clear()
   with self.assertRaises((ValueError,FileExistsError)):f.run('--collect-only')
   self.assertEqual(f.calls,[])
 def test_unavailable_inputs_and_unknown_manifest_rejected(self):
  sp=importlib.util.spec_from_file_location('inputs371',Path(__file__).resolve().parents[1]/'tools/recovery/prepare-gpu371-output-inputs.py');v=importlib.util.module_from_spec(sp);sp.loader.exec_module(v)
  with self.assertRaises(ValueError):v.configure(None)
  with self.assertRaises(ValueError):v.configure('0'*64)
  with self.assertRaises(ValueError):v.configure('f'*64)
  k=m.contract('run213');self.assertEqual((k['run'],k['fresh'],k['capture']),(213,212,214));self.assertFalse(k.get('timing',False))
if __name__=='__main__':unittest.main()
