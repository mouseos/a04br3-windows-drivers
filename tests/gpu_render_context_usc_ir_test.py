#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util
import pathlib
import unittest
root=pathlib.Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('ir',root/'tools/gpu/check-context-usc-ir.py');ir=importlib.util.module_from_spec(s);s.loader.exec_module(ir)
class AuditTest(unittest.TestCase):
    def setUp(self):
        def part(name,values,ops):
            return ('after pco_postprocess_nir:\nshader: MESA_SHADER_COMPUTE\nname: '+name+'\n'+
                    '\n'.join('load_const (0x%08x)'%x for x in values)+'\n'+ops+
                    '\nloop { ishl iadd ieq uadd64_32\nshader ir before passes:\n')
        self.log=part('cs_store_sr_1024_common',[0,1,2,3,4,1024,4096,0xfffffc00],
                      '@load_shared_reg_alloc_size_pco @dma_st_pco @dma_st_shregs_pco')+part(
                      'cs_load_sr_256_common',[0,1,2,3,4,256,1024,0xffffff00],'@dma_ld_pco @dma_ld_shregs_pco')
    def test_expected_and_changed_immediate(self):
        self.assertEqual(ir.audit(self.log)['store']['size_source'],'special register 103')
        with self.assertRaises(ValueError):ir.audit(self.log.replace('0x00001000','0x00001001'))
    def test_missing_memory_operation(self):
        with self.assertRaises(ValueError):ir.audit(self.log.replace('@dma_ld_shregs_pco','unknown'))
if __name__=='__main__':unittest.main()
