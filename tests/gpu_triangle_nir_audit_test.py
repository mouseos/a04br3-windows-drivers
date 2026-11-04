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
def scratchless_fixture():
    text=fixture();v='''32 %0 = @load_vertex_id
32 %1 = load_const (0x00000001)
1 %2 = ieq %0, %1
32 %3 = load_const (0xbf400000)
32 %4 = load_const (0x3f400000)
32 %5 = bcsel %2, %4, %3
@store_output (%5, %0) (component=0, io location=VARYING_SLOT_POS)
32 %7 = load_const (0x00000002)
1 %8 = ieq %0, %7
32 %11 = bcsel %8, %4, %3
@store_output (%11, %0) (component=1, io location=VARYING_SLOT_POS)
32 %13 = load_const (0x3f000000)
@store_output (%13, %0) (component=2, io location=VARYING_SLOT_POS)
32 %16 = load_const (0x3f800000)
@store_output (%16, %0) (component=3, io location=VARYING_SLOT_POS)
32 %17 = load_const (0x00000000)
'''
    import re
    text=re.sub(r'(after pco_postprocess_nir:\nshader: MESA_SHADER_VERTEX).*?\n}',lambda match:match.group(1)+'\n'+v+'\n}',text,flags=re.S)
    ops=['mbyp','movc','pck.zero','tst.s32.e','tst.u32.z.phase2end','uvsw.write','uvsw.write.emit.endtask']
    lines=['    %04d: { p0: %s; }'%(i,op) for i,op in enumerate(ops)]
    lines += ['    %04d: { p0: bbyp0bm_imm32 a, %s; }'%(i+7,val) for i,val in enumerate(['-0.750000f','-0.750000f','0.750000f','0.750000f'])]
    lines += ['    0011: { p0: mbyp; }','    0012: { p0: mbyp; }']
    grouped='shader ir after pco_group_instrs:\nname: ""\nstage: MESA_SHADER_VERTEX\n'+'\n'.join(lines)+'\n}\nshader ir after pco_group_instrs:\nname: ""\nstage: MESA_SHADER_FRAGMENT\n}\n'
    return text+grouped*2
class TestNir(unittest.TestCase):
    def test_fixed(self):
        r=m.audit(fixture());self.assertEqual(r['vertex_scratch_bytes'],24);self.assertFalse(r['gpu_execution_allowed'])
    def test_outside_scratch_rejected(self):
        with self.assertRaises(ValueError):m.audit(fixture().replace('0x10))','0x18))',1))
    def test_changed_draw_rejected(self):
        with self.assertRaises(ValueError):m.audit(fixture().replace('index_count : 3','index_count : 4',1))
    def test_unexpected_memory_intrinsic_rejected(self):
        with self.assertRaises(ValueError):m.audit(fixture().replace('@load_vertex_id','@load_vertex_id\n@load_global',1))
    def test_scratchless_triangle(self):
        r=m.audit(scratchless_fixture(),True);self.assertEqual(r['vertex_scratch_bytes'],0)
        with self.assertRaises(ValueError):m.audit(scratchless_fixture().replace('0x00000002','0x00000001',1),True)
        with self.assertRaises(ValueError):m.audit(scratchless_fixture().replace('p0: mbyp','p0: ld',1),True)
if __name__=='__main__':unittest.main()
