#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util
import pathlib
import unittest

spec=importlib.util.spec_from_file_location('bounded',pathlib.Path(__file__).resolve().parents[1]/'tools/gpu/check-context-bounded-ir.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

def section(name,values,body):
    constants='\n'.join('load_const (0x%08x)'%v for v in values)
    return 'after pco_postprocess_nir:\nshader: MESA_SHADER_COMPUTE\nname: '+name+'\n'+constants+'\n'+body+'\nshader ir before passes:'

def fixture():
    store='@load_shared_reg_alloc_size_pco\n uge \n@dma_st_pco\n ishl \nloop {\n@dma_st_shregs_pco\n bcsel \n uadd64_32 \n}\nelse { load_const (0xffffffff) @dma_st_pco }'
    load='@dma_ld_pco\n uge \n iand \n ishl \nloop {\n@dma_ld_shregs_pco\n umin \n bcsel \n uadd64_32 \n}'
    return section('cs_store_sr_1024_common',{0,1,2,3,4,256,1024,4096,0xfffffc00,0xffffffff},store)+'\n'+section('cs_load_sr_256_common',{0,1,2,3,4,256,1024,0xffffff00},load)

class TestBoundedIR(unittest.TestCase):
    def test_guarded(self):
        self.assertEqual(m.audit(fixture())['store']['guard_max_blocks'],256)
    def test_missing_guard_or_marker(self):
        for old,new in ((' uge ',' ugt '),('load_const (0xffffffff) @dma_st_pco','load_const (0xffffffff)'),(' bcsel ',' iadd ')):
            with self.assertRaises(ValueError):m.audit(fixture().replace(old,new,1))

if __name__=='__main__':unittest.main()
