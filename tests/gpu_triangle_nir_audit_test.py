#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util,pathlib,unittest
root=pathlib.Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('audit',root/'tools/gpu/audit-triangle-nir.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)

def fixture():
    v='scratch: 24\n32 %0 = @load_vertex_id\n32 %7 = load_const (0x00000003)\n32 %8 = ishl %0, %7\n32x2 %9 = @load_scratch (%8)\n'
    v+='\n'.join('@store_scratch (%%0, %%1 (0x%x))'%n for n in (0,8,16))+'\n@store_output\n'
    v+='\n'.join('32 %%99 = load_const (0x%08x)'%n for n in m.V_CONST if n!=3)+'\n'
    f='\n'.join('@'+n for n in sorted(m.F_INTR))+'\n'+'\n'.join('32 %%99 = load_const (0x%08x)'%n for n in m.F_CONST)+'\n'
    draw='[0028] 3: INDEX_LIST\n index_count : 3\n primitive_topology : TRI_LIST (6)\n'
    draw+='\n'.join(' %s : <not present>'%n for n in ('index_base_addr','index_offset','start_index','start_instance','indirect_base_addr','instance_count'))+'\n <raw>'
    return ''.join('after pco_postprocess_nir:\nshader: '+stage+'\n'+body+'\n}\n' for stage,body in [('MESA_SHADER_VERTEX',v),('MESA_SHADER_FRAGMENT',f)]*2)+draw+'\n'+draw
class TestNir(unittest.TestCase):
    def test_fixed(self):
        r=m.audit(fixture());self.assertEqual(r['vertex_scratch_bytes'],24);self.assertFalse(r['gpu_execution_allowed'])
    def test_outside_scratch_rejected(self):
        with self.assertRaises(ValueError):m.audit(fixture().replace('0x10))','0x18))',1))
    def test_changed_draw_rejected(self):
        with self.assertRaises(ValueError):m.audit(fixture().replace('index_count : 3','index_count : 4',1))
    def test_unexpected_memory_intrinsic_rejected(self):
        with self.assertRaises(ValueError):m.audit(fixture().replace('@load_vertex_id','@load_vertex_id\n@load_global',1))
if __name__=='__main__':unittest.main()
