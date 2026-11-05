#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util,pathlib,struct,unittest
root=pathlib.Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('vdm',root/'tools/gpu/check-fixed-vdm.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)

def fixture():
    d,c,v,vc,pp,pf=(0xda00001000,0xda00001010,0xda00001020,0xda00001030,0x8000002000,0x8000002100)
    words=(0x20010001,d&0xffffffff,c&0xffffffff,0x1980,pp&0xffffffff,0x4c000f80,v&0xffffffff,vc&0xffffffff,5,0x20000441,0x68000600,3,0x380,pf&0xffffffff,0xc0000000)
    dump='CONTROL STREAM DUMP\n[0000] 0: PDS_STATE_UPDATE\n  pds_data_addr : 0x%x\n  pds_code_addr : 0x%x\n[000c] 1: PPP_STATE_UPDATE\n  addr : 0x%x\n[0014] 2: VDM_STATE_UPDATE\n  vs_pds_data_base_addr : 0x%x\n  vs_pds_code_base_addr : 0x%x\n[0028] 3: INDEX_LIST\n[0030] 4: PPP_STATE_UPDATE\n  addr : 0x%x\n[0038] 5: TERMINATE\n'%(d,c,pp,v,vc,pf)
    h=m.fnv(struct.pack('<15I',*words));hashes=[{'type':1,'valid':True,'bytes':12},*({'type':1,'valid':True,'bytes':60,'fnv64':h} for _ in range(2))]
    return dump*2,hashes,pp,pf
class TestVdm(unittest.TestCase):
    def test_fixed_and_hash_mismatch(self):
        text,h,pp,pf=fixture();self.assertEqual(m.streams(text,h),[(pp,pf),(pp,pf)])
        h[1]['fnv64']^=1
        with self.assertRaises(ValueError):m.streams(text,h)
    def test_reject_ppp_overlap(self):
        text,h,pp,pf=fixture();text=text.replace('0x%x'%pf,'0x%x'%(pp+4))
        with self.assertRaises(ValueError):m.streams(text,h)
    def test_ppp_must_be_live_at_job(self):
        _,_,pp,pf=fixture();events=[{'event':'vm_map','vm':1,'va':pp,'size':4096},{'event':'context_create','vm':1,'handle':2},
          {'event':'fragment_shader_link'},{'event':'job','type':0,'context':2},
          {'event':'fragment_shader_link'},{'event':'job','type':0,'context':2}]
        m.ppp_mapped_at_job(events,[(pp,pf)]*2)
        events.insert(4,{'event':'vm_unmap','vm':1,'va':pp})
        with self.assertRaises(ValueError):m.ppp_mapped_at_job(events,[(pp,pf)]*2)
if __name__=='__main__':unittest.main()
