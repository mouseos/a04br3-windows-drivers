#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util
import pathlib
import tempfile
import unittest
root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('usc',root/'tools/gpu/generate-context-usc.py')
usc=importlib.util.module_from_spec(spec);spec.loader.exec_module(usc)
class TestContextUsc(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.path=pathlib.Path(self.tmp.name)/'program.c'
    def make(self,replace=''):
        return '\n'.join('static const uint32_t %s[3] = { 0x0, 0x100, %s };'%(name,replace or '0x12345678') for name in usc.NAMES)
    def test_parse_and_reject_untrusted_initializer(self):
        self.path.write_text(self.make());r=usc.parse(self.path)
        self.assertEqual(len(r),2);self.assertEqual(r[usc.NAMES[0]][1],bytes.fromhex('78563412'))
        self.path.write_text(self.make('(host_pointer)'))
        with self.assertRaises(ValueError):usc.parse(self.path)
        self.path.write_text(self.make().replace('0x100','0x200'))
        with self.assertRaises(ValueError):usc.parse(self.path)
if __name__=='__main__':unittest.main()
