"""Host-only variant dispatch, no unknown fresh/receipt or signed image generation."""
import copy,importlib.util,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from gpu368_terminal_hold_test import Fixture,m,repo
from gpu373_pipeline_test import direct
class Client374Fixture(Fixture):
 def __init__(self,root,code):
  super().__init__(root,direct(code),'run219');self.c['planned_windows_reboots']=0
 def run(self,*args):
  with patch.object(m,'stage_preflight'):super().run(*args)
 def latch(self):return self.root/'port/gsi/logs/ios-feasibility/gpu374-run219-executions/combined-run219.json'
class Variant374(unittest.TestCase):
 def test_no_restart_terminal_hold_and_collection_preserve_candidate(self):
  (repo/'build').mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(dir=repo/'build',prefix='variant374-host-') as tmp:
   for code in (0,1):
    f=Client374Fixture(Path(tmp)/str(code),code);f.run()
    self.assertEqual(f.result()['status'],m.HOLD_STATUS);self.assertEqual(f.result()['planned_runtime_restarts'],0)
    self.assertEqual(f.result()['terminal']['phase'],1);self.assertNotIn('planned-restart-resume.log',f.names());self.assertNotIn('matched-boot-write.log',f.names())
    old={p:p.read_bytes() for p in (f.out/'result.json',f.latch())};f.calls.clear();f.run('--collect-only');self.assertNotIn('prepare-write.log',f.names())
    for p,b in old.items():self.assertEqual(p.read_bytes(),b)
 def test_known_variant_mode0_header_and_reject_other_captures(self):
  s=importlib.util.spec_from_file_location('variant374',repo/'tools/recovery/prepare-gpu374-client-inputs.py');g=importlib.util.module_from_spec(s);s.loader.exec_module(g)
  self.assertEqual(g.PROFILE,'combined-features-fresh218-gpu374-clients-v1');self.assertEqual(g.SERVICE,'MT8167CombinedFollowup218')
  p=json.loads((repo.parents[1]/'port/xnu/out/windows-rt-gpu373-finalize215/profile215/peripheral-policy.json').read_text())
  # Exercise common C emitter on a real saved policy as an in-memory test value.
  p.update(profile=g.PROFILE,service=g.SERVICE,data_directory=g.SERVICE,install_targets=[],diagnostic_only=True,driver_updates=False,planned_reboots=0)
  header=g.m.configure().generate(p)
  self.assertIn('#define A04_DIAGNOSTIC_ONLY 1',header);self.assertIn('#define A04_PERIPHERAL_INSTALL_MASK 0x0u',header)
  for bad in (217,219,True):
   with self.assertRaises(ValueError):g.m.admission.variant(bad)
  with self.assertRaises(ValueError):g.m.admission.admit(repo,dict(capture_number=218,schema='GPU374_CLIENT_UPDATE_ADMISSION_V1',main_execution_approved=True,DWM372_selected=False,wifi_update={},install_targets=['wifi']))
 def test_real373_policy_header_unchanged_by_variant_refactor(self):
  s=importlib.util.spec_from_file_location('variant373',repo/'tools/recovery/prepare-gpu373-client-inputs.py');g=importlib.util.module_from_spec(s);s.loader.exec_module(g)
  root=repo.parents[1];old=root/'port/xnu/out/windows-rt-gpu373-finalize215/profile215'
  p=g.load_map(root/'port/xnu/out/windows-rt-gpu373-finalize215/private-map373.json',root)
  self.assertEqual(p,json.loads((old/'peripheral-policy.json').read_text()));self.assertEqual(g.generate(p),(old/'peripheral-batch-policy.h').read_text())
if __name__=='__main__':unittest.main()
