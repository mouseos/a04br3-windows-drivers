"""Pure transition contract; no fake capture/receipt, registry, SDK or device."""
import importlib.util,unittest
from pathlib import Path
s=importlib.util.spec_from_file_location('admission',Path(__file__).resolve().parents[1]/'tools/recovery/gpu373_client_admission.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class Contract(unittest.TestCase):
 def test_client_only_skips_install_phase_and_extra_restart(self):
  x=m.mode();self.assertEqual((x['install_targets'],x['install_mask'],x['planned_reboots']),([],0,0));self.assertTrue(x['diagnostic_only']);self.assertFalse(x['driver_updates'])
 def test_wifi_is_the_only_optional_DIF(self):
  x=m.mode(True);self.assertEqual((x['install_targets'],x['install_mask'],x['planned_reboots']),(['wifi'],8,1));self.assertFalse(x['diagnostic_only']);self.assertEqual((x['render_DIF'],x['display_DIF']),(0,0));self.assertFalse(x['boot_written']);self.assertFalse(x['DWM372_selected'])
 def test_no_placeholder_or_unapproved_plan(self):
  with self.assertRaises(ValueError):m.admit(Path.cwd(),{})
  with self.assertRaises(ValueError):m.mode('yes')
if __name__=='__main__':unittest.main()
