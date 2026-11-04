#!/usr/bin/env python3
import json, pathlib, subprocess, tempfile, unittest
TOOL=pathlib.Path(__file__).resolve().parents[1]/'tools/uart/analyze-ge8300-render.py'
E=b'A04BR3 recovery: EBS\n'
R=b'A04GPU RENDER STATUS=00000000 STAGE=00000020 BG=00000BAD TRI=00000453 OTHER=00000000 ZERO=00000000 MISMATCH=00000000 BOUNDED=00000000 B0_VALID=00000000 B0=00000000 B1_VALID=00000000 B1=00000000\n'
def rows(mirror=False,transpose=False):
 out=b''
 for y in range(64):
  bits=sum(1<<x for x in range(64) if (lambda tx,ty:tx>=8 and tx<49 and ty>=8 and tx+ty<63)(y if transpose else 63-x if mirror else x,x if transpose else y))
  bg=((1<<64)-1)^bits
  out+=('A04GPU ROW Y=%08X LO=%08X HI=%08X BGLO=%08X BGHI=%08X ZLO=00000000 ZHI=00000000\n'%(y,bits&0xffffffff,bits>>32,bg&0xffffffff,bg>>32)).encode()
 return out
class Test(unittest.TestCase):
 def run_case(self,b):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d)/'log';p.write_bytes(b);x=subprocess.run(['python3',str(TOOL),'--log',str(p),'--offset','0','--end',str(len(b))],capture_output=True,text=True);return x.returncode,json.loads(x.stdout)
 def test_match(self):
  rc,v=self.run_case(E+rows()+R);self.assertEqual(rc,0);self.assertEqual(v['verdict'],'spatial_match_linear_captured');self.assertFalse(v['shutdown_verified'])
 def test_reject_ambiguous(self):
  for b in (R,rows()+E+R,E+rows()+b"BOOT_REASON: 9\n"+R,E+E+rows()+R,E+rows()+R+R,E+rows()+R+b'A04WIN BUGCHECK\n',E+rows(True)+R,E+rows(transpose=True)+R,E+rows().replace(b"BGLO=FFFFFFFF",b"BGLO=00000000",1)+R,E+rows().replace(b"ZLO=00000000",b"ZLO=FFFFFFFF",1)+R,E+rows()[:-1]+R,E+rows()+R[:-1]):
   rc,v=self.run_case(b);self.assertNotEqual(rc,0);self.assertNotEqual(v['verdict'],'spatial_match_linear_captured')
if __name__=='__main__':unittest.main()
