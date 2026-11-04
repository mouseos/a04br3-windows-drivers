#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util
import pathlib
import unittest
root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('candidates',root/'tools/gpu/render-upload-candidates.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class CandidateTest(unittest.TestCase):
    def setUp(self):
        self.e=[{'event':'vm_map','vm':1,'va':4096,'size':4096},
                {'event':'upload','kind':'pds_code','gpu_va':4096,'size':16},
                {'event':'context_create','handle':3,'vm':1},
                {'event':'submit_begin'}, {'event':'job','context':3,'type':0,'index':0}]
    def test_present_only_as_candidate(self):
        r=m.candidates(self.e);self.assertFalse(r['safe_for_gpu']);self.assertEqual(r['jobs'][0]['candidates']['pds_code'],1)
    def test_unmap_removes_upload(self):
        self.e.insert(3,{'event':'vm_unmap','vm':1,'va':4096,'size':4096})
        self.assertEqual(m.candidates(self.e)['jobs'][0]['candidates'],{})
    def test_overwrite_invalidates_previous_range(self):
        self.e.insert(2,{'event':'upload','kind':'usc_code','gpu_va':4100,'size':16})
        self.assertEqual(m.candidates(self.e)['jobs'][0]['candidates'],{'usc_code':1})
    def test_ambiguous_vm_rejected(self):
        self.e.insert(1,{'event':'vm_map','vm':2,'va':4096,'size':4096})
        with self.assertRaises(ValueError):m.candidates(self.e)
if __name__=='__main__':unittest.main()
