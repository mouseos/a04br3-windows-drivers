#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import copy
import importlib.util
import pathlib
import struct
import unittest
root=pathlib.Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('fixture',root/'tools/gpu/generate-render-fixture.py')
m=importlib.util.module_from_spec(s); s.loader.exec_module(m)

class FixturePlanTest(unittest.TestCase):
    def setUp(self):
        stream=struct.pack('<IIQQIIIIII',40,0,0x100000,0x12345678,514,131072,536870912,0,1,32)
        self.events=[dict(event='bo_create',fd=1,handle=2,size=0x100000),
          dict(event='vm_map',fd=1,vm=3,handle=2,va=0x100000,offset=0,size=0x100000),
          dict(event='context_create',handle=4,vm=3,type=0),
          dict(event='freelist_create',handle=5,vm=3,gpu=0x110000,max=65536),
          dict(event='freelist_create',handle=6,vm=3,gpu=0x150000,max=48),
          dict(event='hwrt_create',handle=7,width=64,height=64,samples=1,layers=1,
               tpc_size=128,tpc_stride=0,tpc=0x101000,rtc=0,vheap=0x102000,region=16,
               rt=[[0x103000,0,0x105000],[0x104000,0,0x105080]],freelists=[5,6],merge=[0]*6),
          dict(event='submit_begin'),dict(event='job',type=0,context=4,hwrt=7,flags=3,rt_index=0,stream_hex=stream.hex(),stream_length=48)]
    def test_zeroed_pointers_and_sizes(self):
        p=m.make_plan(self.events,0); self.assertFalse(p['gpu_execution_allowed'])
        self.assertEqual(bytes.fromhex(p['geometry_template_hex'])[8:24],bytes(16))
        r={x['name']:x['length'] for x in p['ranges']}
        self.assertEqual((r['tpc'],r['vheap'],r['mlist0'],r['region0'],r['freelist0']), (128,1536,4096,128,262144))
    def test_reject_unknown_register_bits(self):
        e=copy.deepcopy(self.events); e[-1]['stream_hex']=e[-1]['stream_hex'][:-2]+'ff'
        with self.assertRaises(ValueError):m.make_plan(e,0)
    def test_reject_bad_stride(self):
        e=copy.deepcopy(self.events); e[-3]['rt'][1][2]+=4
        with self.assertRaises(ValueError):m.make_plan(e,0)
    def test_reject_unmapped_full_extent(self):
        e=copy.deepcopy(self.events);e[-3]['vheap']=0x1fff00
        with self.assertRaises(ValueError):m.make_plan(e,0)
    def test_reject_vdm_outside_vm(self):
        e=copy.deepcopy(self.events); b=bytearray.fromhex(e[-1]['stream_hex']); struct.pack_into('<Q',b,8,0x300000);e[-1]['stream_hex']=b.hex()
        with self.assertRaises(ValueError):m.make_plan(e,0)
    def test_reject_overlapping_mapping(self):
        e=copy.deepcopy(self.events);e.insert(2,dict(event='vm_map',fd=1,vm=3,handle=2,va=0x101000,offset=0,size=4096))
        with self.assertRaises(ValueError):m.make_plan(e,0)
if __name__=='__main__': unittest.main()
