#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util
import pathlib
import struct
import unittest
root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('pt',root/'tools/gpu/check-context-pt-data.py')
pt=importlib.util.module_from_spec(spec);spec.loader.exec_module(pt)
class TestContextPt(unittest.TestCase):
    def test_fixed_constants_and_zero_padding(self):
        addr=0xda00001000;store=pt.make(addr,'pt_store_data');load=pt.make(addr,'pt_resume_data')
        self.assertEqual(struct.unpack('<Q',store[:8])[0],addr|(15<<40)|(32<<46))
        self.assertEqual(struct.unpack('<Q',load[16:24])[0],(addr+96)|(4<<41)|(28<<47))
        self.assertEqual(store[24:],bytes(8));self.assertEqual(load[24:],bytes(8))
        shared=pt.make_sr(addr,0xe000005040,1<<35)
        self.assertEqual(len(shared),64);self.assertEqual(shared[40:],bytes(24))
        self.assertEqual(struct.unpack_from('<I',shared,24)[0],0x80004002)
        self.assertEqual(len(pt.make_code('pt_store_code')),20)
        self.assertEqual(len(pt.make_code('pt_resume_code')),24)
        self.assertEqual(len(pt.make_code('sr_code')),12)
    def test_reject_invalid_range_and_kind(self):
        for addr in (0,1,(1<<40)-64):
            with self.assertRaises(ValueError):pt.make(addr,'pt_store_data')
        with self.assertRaises(ValueError):pt.make(0xda00001000,'unknown')
        with self.assertRaises(ValueError):pt.make_sr(0xda00001000,0xe000005040,1<<33)
if __name__=='__main__':unittest.main()
