"""No private hive/VM/build; pending candidates cannot become executable policy."""
import importlib.util,json,subprocess,tempfile,types,unittest
from pathlib import Path
from unittest.mock import patch
repo=Path(__file__).resolve().parents[1]
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
g=module('followup231',repo/'tools/recovery/prepare-followup231-inputs.py')
e=module('followup231_engine',repo/'tools/recovery/run-gpu368-fresh200.py')
class Pending(unittest.TestCase):
 def test_no_candidate_no_adapter_no_finalize(self):
  with self.assertRaisesRegex(ValueError,'pending'):g.accepted(repo.parents[1])
  forged=json.loads(g.ACCEPTED.read_text());forged.update(status='actual-signed-candidates-main-reviewed',mode='client-only',adapter_id='unreviewed-generic')
  with patch.object(g,'read',return_value=forged),self.assertRaisesRegex(ValueError,'adapter'):g.accepted(repo.parents[1])
 def test_draft_not_executable_and_paths_fixed(self):
  with self.assertRaises(ValueError):g.generate({'profile':g.BASELINE_PROFILE,'draft':True})
  self.assertEqual(g.SERVICE,'MT8167CombinedFollowup231')
  self.assertIn('MT8167CombinedFollowup231',g.paths())
 def test_existing_controller_contracts_unchanged(self):
  old=types.ModuleType('old_engine');old.__file__=str(repo/'tools/recovery/run-gpu368-fresh200.py')
  exec(subprocess.check_output(['git','-C',str(repo),'show','be8b8f349f980e417e95863ac8b6db82a81ce76d:tools/recovery/run-gpu368-fresh200.py'],text=True),old.__dict__)
  for name in old.KNOWN_PROFILES:self.assertEqual(e.contract(name),old.contract(name))
  n=e.contract('run232');self.assertEqual((n['fresh'],n['capture'],n['service']),(231,233,'MT8167CombinedFollowup231'))
if __name__=='__main__':unittest.main()
