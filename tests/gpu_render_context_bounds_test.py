#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util
import pathlib
import unittest
root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('bounds',root/'tools/gpu/analyze-context-shared-bounds.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
class BoundsTest(unittest.TestCase):
    def test_chunk_alignment_and_limits(self):
        self.assertTrue(b.model(0,1024)['within_state'])
        self.assertFalse(b.model(1,1024)['terminates'])
        self.assertTrue(b.model(16,256)['within_state'])
        self.assertFalse(b.model(16,1024)['terminates'])
        self.assertTrue(b.model(64,1024)['within_state'])
        self.assertTrue(b.model(256,256)['within_state'])
        self.assertFalse(b.model(320,1024)['within_state'])
    def test_outside_numeric_domain(self):
        for v in (-1,1<<32):
            with self.assertRaises(ValueError):b.model(v,256)
    def test_bounded_all_valid_sizes(self):
        for size in range(257):
            for chunk in (256,1024):
                result=b.bounded_model(size,chunk)
                self.assertTrue(result['terminates'] and result['within_state'])
                self.assertLessEqual(result['iterations'],16)
        self.assertEqual(b.bounded_model(257,256)['accepted_blocks'],0)
        self.assertEqual(b.bounded_model((1<<32)-1,1024)['accepted_blocks'],0)
if __name__=='__main__':unittest.main()
