"""Host-only source dispatch/hold checks; no new capture or signed inputs."""
import importlib.util,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from gpu368_terminal_hold_test import Fixture,m,repo
from gpu373_pipeline_test import direct

class Client375Fixture(Fixture):
 def __init__(self,root,code):
  super().__init__(root,direct(code),'run222');self.c['planned_windows_reboots']=0
 def run(self,*args):
  with patch.object(m,'stage_preflight'):super().run(*args)
 def latch(self):return self.root/'port/gsi/logs/ios-feasibility/gpu375-run222-executions/combined-run222.json'

class Variant375(unittest.TestCase):
 def test_hold_and_collect_never_redeploy_or_restart(self):
  with tempfile.TemporaryDirectory(dir=repo/'build',prefix='variant375-host-') as tmp:
   for code in (0,1):
    f=Client375Fixture(Path(tmp)/str(code),code);f.run()
    self.assertEqual(f.result()['status'],m.HOLD_STATUS);self.assertEqual(f.result()['planned_runtime_restarts'],0)
    self.assertNotIn('matched-boot-write.log',f.names());self.assertNotIn('planned-restart-resume.log',f.names())
    old={p:p.read_bytes() for p in (f.out/'result.json',f.latch())};f.calls.clear();f.run('--collect-only')
    self.assertNotIn('prepare-write.log',f.names())
    for p,b in old.items():self.assertEqual(p.read_bytes(),b)
 def test_facade_source_and_common_emitter(self):
  s=importlib.util.spec_from_file_location('variant375',repo/'tools/recovery/prepare-gpu375-client-inputs.py');g=importlib.util.module_from_spec(s);s.loader.exec_module(g)
  self.assertEqual(g.PROFILE,'combined-features-fresh221-gpu375-clients-v1');self.assertEqual(g.SERVICE,'MT8167CombinedFollowup221')
  self.assertEqual(g.m.admission.CLIENT375_SOURCE,'3247c8923871630e14313aa00c9ccc4085ca03b6')
  # Only the emitter's mode switches are exercised with a saved policy in RAM.
  p=json.loads((repo.parents[1]/'port/xnu/out/windows-rt-gpu373-finalize215/profile215/peripheral-policy.json').read_text())
  p.update(profile=g.PROFILE,service=g.SERVICE,data_directory=g.SERVICE,install_targets=[],diagnostic_only=True,driver_updates=False,planned_reboots=0)
  h=g.m.configure().generate(p)
  self.assertIn('#define A04_DIAGNOSTIC_ONLY 1',h);self.assertIn('#define A04_PERIPHERAL_INSTALL_MASK 0x0u',h)
  with self.assertRaises(ValueError):g.m.admission.admit(repo,dict(capture_number=221,schema='GPU375_CLIENT_UPDATE_ADMISSION_V1',main_execution_approved=True,DWM372_selected=False,wifi_update={},install_targets=['wifi']))
if __name__=='__main__':unittest.main()
