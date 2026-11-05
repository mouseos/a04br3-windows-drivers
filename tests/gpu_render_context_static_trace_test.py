#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import copy
import importlib.util
import pathlib
import unittest
spec=importlib.util.spec_from_file_location('static',pathlib.Path(__file__).resolve().parents[1]/'tools/gpu/check-context-static.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

def fixture():
    rows=[{'event':'ctx_component','kind':'vdm_state','index':0,'gpu_va':0x8000000000},
          {'event':'ctx_component','kind':'geom_state','index':0,'gpu_va':0x8000000100}]
    for i in range(2):
        pt=0xda00001000+i*0x200;sr=pt+128
        for kind,addr in (('pt_store_data',pt),('pt_store_code',pt+32),('pt_resume_data',pt+64),('pt_resume_code',pt+96),
                          ('sr_store_data',sr),('sr_store_code',sr+64),('sr_load_data',sr+80),('sr_load_code',sr+144)):
            rows.append({'event':'ctx_component','kind':kind,'index':i,'gpu_va':addr})
        rows.append({'event':'ctx_static','index':i,'vdm':0x8000000000,'geom':0x8000000100,
          'store0':(((sr&0xffffffff)|5)<<32)|0x22001004,'store1':(sr+64)&0xffffffff,
          'store2':((pt&0xffffffff)<<32)|0x420,'resume0':((((sr+80)&0xffffffff)|5)<<32)|0x20001004,
          'resume1':(sr+144)&0xffffffff,'resume2':(((pt+64)&0xffffffff)<<32)|0x420})
    return rows

class TestStaticTrace(unittest.TestCase):
    def test_fixed(self):self.assertEqual(m.analyze(fixture())['context_slots'],2)
    def test_bad_task_and_contiguity(self):
        rows=fixture();next(e for e in rows if e['event']=='ctx_static')['store0']^=1
        with self.assertRaises(ValueError):m.analyze(rows)
        rows=fixture();next(e for e in rows if e.get('kind')=='pt_store_code')['gpu_va']+=16
        with self.assertRaises(ValueError):m.analyze(rows)
if __name__=='__main__':unittest.main()
