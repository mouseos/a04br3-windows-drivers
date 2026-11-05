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
    def test_background_shader_and_code(self):
        rows=fixture();base=m.BASE;shader=0xe000002000
        def fnv(data):
            h=14695981039346656037
            for b in data:h=((h^b)*1099511628211)&((1<<64)-1)
            return h
        idx=next(i for i,e in enumerate(rows) if e['event']=='fragment_shader_link')
        rows[idx:idx]=[{'event':'vm_map','vm':1,'va':shader,'size':4096},
                       {'event':'upload','gpu_va':shader,'size':16,'kind':'usc_code'},
                       {'event':'bg_frag_pds','gpu_va':base+0x1000,'size':32,'shader_gpu':shader,'control':0x2000,
                        'fnv64':fnv(struct.pack('<Q',0x2000)+bytes(24))},
                       {'event':'bg_shader','gpu_va':shader,'size':16,'pds_gpu':base+0x1000,'clear_mask':1,'load_mask':0}]
        hashes=[{'gpu_va':base+0x1100,'size':8,'fnv64':fnv(struct.pack('<II',0xf4020000,0xd3000000))}]
        self.assertEqual(m.analyze(rows,True,False,False,True,hashes)['background_shader_dependencies'],8)
        bad=copy.deepcopy(rows);next(e for e in bad if e['event']=='bg_frag_pds')['control']^=4
        with self.assertRaises(ValueError):m.analyze(bad,True,False,False,True,hashes)
        bad=copy.deepcopy(hashes);bad[0]['fnv64']^=1
        with self.assertRaises(ValueError):m.analyze(rows,True,False,False,True,bad)
    def test_clear_dependencies(self):
        rows=fixture();base=m.BASE;constant=base+0x2000
        def fnv(data):
            h=14695981039346656037
            for b in data:h=((h^b)*1099511628211)&((1<<64)-1)
            return h
        idx=next(i for i,e in enumerate(rows) if e['event']=='fragment_shader_link')
        rows[idx:idx]=[{'event':'bg_constants','gpu_va':constant,'size':4,'clears':1,'textures':0,'tiles':0,'shareds':1,
                        'fnv64':fnv(bytes((0x22,0x44,0x66,0xff)))},
                       {'event':'bg_pds_data','gpu_va':base+0x1200,'size':12,'constants_gpu':constant,'shareds':1,
                        'fnv64':fnv(struct.pack('<QI',constant,0x90000001))}]
        self.assertEqual(m.analyze(rows,True,False,True)['background_clear_dependencies'],8)
        bad=copy.deepcopy(rows);next(e for e in bad if e['event']=='bg_constants')['textures']=1
        with self.assertRaises(ValueError):m.analyze(bad,True,False,True)
        bad=copy.deepcopy(rows);idx=next(i for i,e in enumerate(bad) if e['event']=='fragment_shader_link')
        bad.insert(idx,{'event':'upload','gpu_va':constant,'size':4,'kind':'other'})
        with self.assertRaises(ValueError):m.analyze(bad,True,False,True)
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
