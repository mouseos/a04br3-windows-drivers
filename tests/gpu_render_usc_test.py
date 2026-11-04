#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import copy
import importlib.util
import pathlib
import unittest

root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('usc',root/'tools/gpu/check-render-usc.py')
usc=importlib.util.module_from_spec(spec);spec.loader.exec_module(usc)

class UscTest(unittest.TestCase):
    def setUp(self):
        self.events=[]
        for i,stage in enumerate((0,4,0,4)):
            size=312 if stage==0 else 360;va=4096+i*1024;digest=50+stage
            self.events.extend([dict(event='pco_binary',stage=stage,internal=0,size=size,fnv64=digest),
                dict(event='upload',kind='usc_code',gpu_va=va,size=size),
                dict(event='usc_upload_hash',gpu_va=va,size=size,fnv64=digest),
                dict(event='pds_shader' if stage==0 else 'fragment_shader_link',shader_gpu=va,entry_gpu=va+4)])
    def test_fixed_shader_links(self):
        self.assertEqual(len(usc.links(self.events)),4)
    def test_reject_missing_provenance_and_stale_upload(self):
        e=copy.deepcopy(self.events);e[2]['fnv64']+=1
        with self.assertRaises(ValueError):usc.links(e)
        e=copy.deepcopy(self.events);e.insert(3,dict(event='upload',kind='usc_code',gpu_va=4096,size=4))
        with self.assertRaises(ValueError):usc.links(e)
        e=copy.deepcopy(self.events);e[3]['entry_gpu']=6000
        with self.assertRaises(ValueError):usc.links(e)
    def test_reject_foreign_or_internal_shader(self):
        e=copy.deepcopy(self.events);e[0]['internal']=1
        with self.assertRaises(ValueError):usc.links(e)
        e=copy.deepcopy(self.events);e.insert(2,dict(event='vm_destroy',handle=1))
        with self.assertRaises(ValueError):usc.links(e)
if __name__=='__main__':unittest.main()
