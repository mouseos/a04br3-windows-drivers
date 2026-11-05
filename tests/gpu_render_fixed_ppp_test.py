#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util,pathlib,struct,unittest
root=pathlib.Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('ppp',root/'tools/gpu/check-fixed-ppp.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
def fixture():
    a=0xda00001000;b=0xda00001040;c=0xda00001060;pp=0x8000002000;pf=0x8000002100
    words=list(m.FIXED);words[5]=a&0xffffffff;words[6]=b&0xffffffff;words[9]=c&0xffffffff
    rows=[{'gpu_va':pp,'bytes':100,'fnv64':m.fnv(struct.pack('<25I',*words))},
          {'gpu_va':pf,'bytes':12,'fnv64':m.fnv(struct.pack('<III',0x02000000,0x00040001,0))}]
    return rows,[(pp,pf,a,b,c)]*2
class TestPpp(unittest.TestCase):
    def test_fixed(self):
        rows,draws=fixture();m.match_records(rows,draws)
    def test_tamper(self):
        rows,draws=fixture();rows[0]['fnv64']^=1
        with self.assertRaises(ValueError):m.match_records(rows,draws)
    def test_overlap_rejected(self):
        rows,draws=fixture();pp,pf,a,b,c=draws[0];draws[0]=(pp,pf,a,a+16,c)
        with self.assertRaises(ValueError):m.match_records(rows,draws)
if __name__=='__main__':unittest.main()
