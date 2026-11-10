"""Host-only controller transport and C-header contracts; no hive/VM/device."""
import copy,importlib.util,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from gpu368_terminal_hold_test import Fixture,m,pb,start,terminal,repo
class ClientFixture(Fixture):
 def __init__(self,root,raw,wifi):
  super().__init__(root,raw,'run216');self.c['planned_windows_reboots']=int(wifi)
 def run(self,*extra):
  with patch.object(m,'stage_preflight'):
   super().run(*extra)
 def latch(self):return self.root/'port/gsi/logs/ios-feasibility/gpu373-run216-executions/combined-run216.json'
def direct(code=0):
 raw=start(11,22)
 for i in (0,1):raw+=pb(i,0x220,100+i)+pb(i,0x221,code if i==0 else 0)
 return raw+pb(0,0x288,0)+pb(0,0x207,code)
class Pipeline(unittest.TestCase):
 def setUp(self):
  (repo/'build').mkdir(exist_ok=True);self.tmp=tempfile.TemporaryDirectory(dir=repo/'build',prefix='client373-host-');self.root=Path(self.tmp.name)
 def tearDown(self):self.tmp.cleanup()
 def test_modes_hold_success_and_failure_then_collect_only(self):
  for wifi in (False,True):
   for code in (0,1):
    with self.subTest(wifi=wifi,code=code):
     f=ClientFixture(self.root/str(wifi)/str(code),terminal(code) if wifi else direct(code),wifi);f.run()
     self.assertEqual(f.result()['status'],m.HOLD_STATUS);self.assertEqual(f.result()['planned_runtime_restarts'],int(wifi));self.assertEqual(f.result()['terminal']['phase'],1)
     self.assertNotIn('matched-boot-write.log',f.names());self.assertNotIn('terminal-to-recovery.log',f.names())
     originals={p:p.read_bytes() for p in (f.latch(),f.out/'result.json',f.out/'terminal-proof.json')};f.calls.clear();f.run('--collect-only')
     self.assertNotIn('prepare-write.log',f.names());self.assertNotIn('normal-esp-write.log',f.names())
     for p,b in originals.items():self.assertEqual(p.read_bytes(),b)
 def test_client_only_panic_and_restart_preserve_recovery(self):
  for i,suffix in enumerate((b'A04WIN BUGCHECK DEAD0001\n',b'BOOT_REASON=00000001\n')):
   f=ClientFixture(self.root/str(i),direct()+suffix,False);f.run();self.assertIn('terminal-to-recovery.log',f.names());self.assertFalse((f.out/'terminal-proof.json').exists())
 def test_header_has_only_wifi_DIF_or_direct_diagnostics(self):
  spec=importlib.util.spec_from_file_location('client373inputs',repo/'tools/recovery/prepare-gpu373-client-inputs.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
  # Use a real saved baseline policy for emitter regression; no fake capture or receipt.
  saved=repo.parents[1]/'port/xnu/out/windows-rt-gpu371-finalize212/profile212/peripheral-policy.json'
  p=json.loads(saved.read_text());p.update(profile=g.PROFILE,service=g.SERVICE,data_directory=g.SERVICE)
  for wifi in (False,True):
   q=copy.deepcopy(p);q.update(g.admission.mode(wifi));q['install_targets']=['wifi'] if wifi else []
   header=g.configure(wifi).generate(q)
   self.assertIn('#define A04_PERIPHERAL_INSTALL_MASK '+('0x8u' if wifi else '0x0u'),header)
   self.assertIn('#define A04_DIAGNOSTIC_ONLY '+('0' if wifi else '1'),header)
   self.assertNotIn('#define A04_PERIPHERAL_INSTALL_MASK 0x400u',header)
if __name__=='__main__':unittest.main()
