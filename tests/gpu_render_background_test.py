#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import copy,importlib.util,pathlib,struct,unittest
root=pathlib.Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('bg',root/'tools/gpu/check-fragment-background.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
def fixture():
    base=m.BASE;shader=0x1000;texcode=0x1100;texdata=0x1200;event=0x1300;usc=0xe000001000
    rows=[{'event':'vm_map','vm':1,'va':base,'size':0x4000},
      {'event':'vm_map','vm':1,'va':usc,'size':4096},{'event':'context_create','handle':1,'vm':1},
      {'event':'upload','gpu_va':base+shader,'size':32,'kind':'pds_data'},
      {'event':'upload','gpu_va':base+shader+32,'size':4,'kind':'pds_code'},
      {'event':'upload','gpu_va':base+texcode,'size':8,'kind':'pds_code'},
      {'event':'upload','gpu_va':base+texdata,'size':12,'kind':'pds_data'},
      {'event':'upload','gpu_va':base+event,'size':8,'kind':'pds_data'},
      {'event':'upload','gpu_va':usc,'size':40,'kind':'usc_code'}]
    control=0x800000000|(usc&0xfffffffc);h=14695981039346656037
    for b in struct.pack('<Q',control):h=((h^b)*1099511628211)&((1<<64)-1)
    rows.append({'event':'render_pixel_event','data_gpu':base+event,'shader_gpu':usc,'shader_size':128,'data_size':8,'control':control,'fnv64':h})
    command=bytearray(384);bg=(texcode<<32)|shader;table=texdata<<32;info=0x0080000000010000
    for off in (256,280):struct.pack_into('<3Q',command,off,bg,table,info)
    struct.pack_into('<I',command,356,0x201);struct.pack_into('<I',command,364,event)
    for _ in range(2):
        rows += [{'event':'fragment_shader_link'},{'event':'job','type':0,'context':1}]
        for flags in (192,128):rows.append({'event':'job','type':1,'context':1,'flags':flags,'stream_length':384,'stream_hex':command.hex()})
    return rows
class TestBackground(unittest.TestCase):
    def test_fixed(self):
        result=m.analyze(fixture(),True);self.assertEqual(result['fragment_jobs'],4);self.assertEqual(result['pixel_event_shader_jobs'],4)
        self.assertFalse(result['gpu_execution_allowed'])
    def test_overwritten_pds(self):
        rows=fixture();idx=next(i for i,e in enumerate(rows) if e['event']=='fragment_shader_link')
        rows.insert(idx,{'event':'upload','gpu_va':m.BASE+0x1000,'size':4,'kind':'pds_data'})
        with self.assertRaises(ValueError):m.analyze(rows,True)
    def test_bad_event_and_shader_unmap(self):
        rows=fixture();e=next(e for e in rows if e['event']=='render_pixel_event');e['control']^=1<<35
        with self.assertRaises(ValueError):m.analyze(rows,True)
        rows=fixture();idx=next(i for i,e in enumerate(rows) if e['event']=='fragment_shader_link')
        rows.insert(idx,{'event':'vm_unmap','vm':1,'va':0xe000001000})
        with self.assertRaises(ValueError):m.analyze(rows,True)
    def test_truncated_command(self):
        rows=fixture();e=next(e for e in rows if e.get('type')==1);e['stream_hex']=e['stream_hex'][:-2]
        with self.assertRaises(ValueError):m.analyze(rows,True)
    def test_pbe_target_extent_and_unmap(self):
        rows=fixture();addr=0x8000000000
        target={'event':'render_pbe_target','image_gpu':addr,'target_gpu':addr,'image_size':16384,
                'width':64,'height':64,'stride':64,'layout':1,'state0':(addr>>2)&0xffffffff,'state1':addr>>34}
        rows.insert(1,{'event':'vm_map','vm':1,'va':addr,'size':16384});rows.insert(2,target)
        event=next(e for e in rows if e['event']=='render_pixel_event')
        event['pbe_state0']=target['state0'];event['pbe_state1']=target['state1']
        self.assertEqual(m.analyze(rows,True,True)['pbe_target_jobs'],4)
        bad=copy.deepcopy(rows);next(e for e in bad if e['event']=='render_pbe_target')['image_size']=8192
        with self.assertRaises(ValueError):m.analyze(bad,True,True)
        bad=copy.deepcopy(rows);idx=next(i for i,e in enumerate(bad) if e['event']=='fragment_shader_link')
        bad.insert(idx,{'event':'vm_unmap','vm':1,'va':addr})
        with self.assertRaises(ValueError):m.analyze(bad,True,True)
if __name__=='__main__':unittest.main()
