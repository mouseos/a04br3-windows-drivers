#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import copy
import importlib.util
import pathlib
import unittest
root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('vdm',root/'tools/gpu/check-render-vdm-pds.py')
vdm=importlib.util.module_from_spec(spec);spec.loader.exec_module(vdm)

class VdmTest(unittest.TestCase):
    def setUp(self):
        self.events=[dict(event='vm_map',vm=1,va=4096,size=4096)]
        for addr,kind,size in ((0x1100,'vertex_attributes',12),(0x1200,'descriptor_mappings',24),
                               (0x1300,'vertex_attributes',12),(0x1400,'descriptor_mappings',24)):
            self.events.append(dict(event='pds_construct',gpu_va=addr,kind=kind,size=size))
        for addr,size in ((0x1500,12),(0x1600,8),(0x1700,12),(0x1800,8)):
            self.events.append(dict(event='upload',kind='pds_code',gpu_va=addr,size=size))
        self.log='''vs_pds_data_base_addr : 0x1100
vs_pds_data_base_addr : 0x1300
pds_data_addr : 0x1200
pds_data_addr : 0x1400
vs_pds_data_size : 16 bytes
vs_pds_data_size : 16 bytes
pds_data_size : 32 bytes
pds_data_size : 32 bytes
vs_pds_code_base_addr : 0x1500
vs_pds_code_base_addr : 0x1700
pds_code_addr : 0x1600
pds_code_addr : 0x1800
'''
    def test_correlate(self):
        r=vdm.analyze(self.events,self.log)
        self.assertEqual(r['pds_code_upload_sizes'],[12,8,12,8])
        self.assertFalse(r['gpu_execution_allowed'])
    def test_reject_partial_fetch_and_missing_code(self):
        e=copy.deepcopy(self.events);e[0]['size']=0x414
        with self.assertRaises(ValueError):vdm.analyze(e,self.log)
        e=copy.deepcopy(self.events);e.pop()
        with self.assertRaises(ValueError):vdm.analyze(e,self.log)
    def test_reject_changed_fetch_size(self):
        with self.assertRaises(ValueError):vdm.analyze(self.events,self.log.replace('16 bytes','12 bytes'))
if __name__=='__main__':unittest.main()
