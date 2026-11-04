#!/usr/bin/env python3
import json, pathlib, subprocess, tempfile, unittest
TOOL=pathlib.Path(__file__).resolve().parents[1]/'tools/uart/analyze-ge8300-render.py'
E=b'A04BR3 recovery: EBS\n'
R=b'A04GPU RENDER STATUS=00000000 STAGE=00000020 BG=00000B80 TRI=00000480 OTHER=00000000 BOUNDED=00000000 B0_VALID=00000000 B0=00000000 B1_VALID=00000000 B1=00000000\n'
class Test(unittest.TestCase):
 def run_case(self,b):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d)/'log';p.write_bytes(b);x=subprocess.run(['python3',str(TOOL),'--log',str(p),'--offset','0','--end',str(len(b))],capture_output=True,text=True);return x.returncode,json.loads(x.stdout)
 def test_match(self):
  rc,v=self.run_case(E+R);self.assertEqual(rc,0);self.assertEqual(v['verdict'],'histogram_match_only');self.assertFalse(v['shutdown_verified'])
 def test_reject_ambiguous(self):
  for b in (R,E+E+R,E+R+R,E+R+b'A04WIN BUGCHECK\n',E+R.replace(b'00000480',b'00000481'),E+R[:-1]):
   rc,v=self.run_case(b);self.assertNotEqual(rc,0,b);self.assertNotEqual(v['verdict'],'histogram_match_only')
if __name__=='__main__':unittest.main()
