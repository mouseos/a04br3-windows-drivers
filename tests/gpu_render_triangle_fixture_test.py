#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util
import pathlib
import unittest
spec=importlib.util.spec_from_file_location('triangle',pathlib.Path(__file__).resolve().parents[1]/'tools/gpu/generate-triangle-fixture.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

def fixture():
    vertex='shader ir after pco_group_instrs:\nname: test\nstage: MESA_SHADER_VERTEX\n{\n}\n'
    ops=sorted(m.FRAGMENT_OPS)+['mbyp']*(27-len(m.FRAGMENT_OPS))
    body='\n'.join('  %d: { s0 = sh0, s1 = sr53 p0: %s; }'%(i,op) for i,op in enumerate(ops))
    fragment='shader ir after pco_group_instrs:\nname: test\nstage: MESA_SHADER_FRAGMENT\n{\n'+body+'\n}\n'
    return (vertex+fragment)*2
class TestTriangleFixture(unittest.TestCase):
    def test_grouped(self):m.check_fragment(fixture())
    def test_reject_new_source_or_opcode(self):
        for old,new in (('sh0','sh1'),('sr53','sr54'),('p0: mbyp','p0: ld')):
            with self.assertRaises(ValueError):m.check_fragment(fixture().replace(old,new,1))
if __name__=='__main__':unittest.main()
