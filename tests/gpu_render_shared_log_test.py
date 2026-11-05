#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util
import pathlib
import unittest
root=pathlib.Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('log',root/'tools/gpu/summarize-shared-size-log.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class TestLog(unittest.TestCase):
    def test_matching_and_bounds(self):
        r=m.analyze('TA context shared alloc size store 0x40, load 0x40\nTA context shared alloc size store 0x1, load 0x100\n')
        self.assertEqual(r['records'],2);self.assertEqual(r['different_pairs'],1);self.assertEqual(r['outside_source_loop_conditions'],1)
        self.assertFalse(r['gpu_execution_allowed'])
    def test_reject_unrelated_or_oversized_hex(self):
        for value in ('nothing','TA context shared alloc size store 0x123456789, load 0x40'):
            with self.assertRaises(ValueError):m.analyze(value)
    def test_uart_candidates_and_validation(self):
        line='A04GPU SHARED CANDIDATES=00000002 STORE_MASK=00000002 LOAD_MASK=00000014 OTHER=00000001 DIFFERENT=00000002 CHANGED=00000000'
        r=m.analyze_uart(line);self.assertFalse(r['gpu_execution_allowed'])
        self.assertEqual(r['samples'][0]['store_values'],[64])
        self.assertEqual(r['samples'][0]['load_values'],[128,256])
        with self.assertRaises(ValueError):m.analyze_uart(line.replace('STORE_MASK=00000002','STORE_MASK=00000020'))
        with self.assertRaises(ValueError):m.analyze_uart(line.replace('DIFFERENT=00000002','DIFFERENT=00000003'))
if __name__=='__main__':unittest.main()
