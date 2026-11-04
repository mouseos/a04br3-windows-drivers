#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import copy,importlib.util,pathlib,struct,unittest
root=pathlib.Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('pds',root/'tools/gpu/reconstruct-pds-data.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class TestPds(unittest.TestCase):
 def setUp(self):
  v=0xda00001000;d=0xda00002000
  self.text='vs_pds_data_base_addr : 0x%x\nvs_pds_data_base_addr : 0x%x\npds_data_addr : 0x%x\npds_data_addr : 0x%x\n'%(v,v,d,d)
  vertex=struct.pack('<QI',0x5040|(1<<35),0xabc)
  desc=struct.pack('<QQII',0x8000001000,0x8000001010,4,5)
  self.e=[dict(event='pds_construct',kind='vertex_attributes',gpu_va=v,size=12,entries=2,fnv64=m.fnv(vertex)),
          dict(event='pds_entry',kind='vertex_attributes',gpu_va=v,entry_type=2,const_offset=2,subtype=0,value=0xabc),
          dict(event='pds_entry',kind='vertex_attributes',gpu_va=v,entry_type=6,const_offset=0,subtype=0,value=1<<35),
          dict(event='pds_shader',parent_gpu=v,offset=0,shader_gpu=0xe000005000,entry_gpu=0xe000005040,size=4096),
          dict(event='pds_construct',kind='descriptor_mappings',gpu_va=d,size=24,entries=4,fnv64=m.fnv(desc)),
          dict(event='pds_entry',kind='descriptor_mappings',gpu_va=d,entry_type=5,const_offset=0,subtype=12,value=0),
          dict(event='pds_entry',kind='descriptor_mappings',gpu_va=d,entry_type=5,const_offset=2,subtype=13,value=0),
          dict(event='pds_entry',kind='descriptor_mappings',gpu_va=d,entry_type=2,const_offset=4,subtype=0,value=4),
          dict(event='pds_entry',kind='descriptor_mappings',gpu_va=d,entry_type=2,const_offset=5,subtype=0,value=5),
          dict(event='pds_dependency',parent_gpu=d,offset=0,kind='spill',info_gpu=0x8000001000,buffer_gpu=0x8000002000,buffer_size=8192,block_size=4),
          dict(event='pds_dependency',parent_gpu=d,offset=2,kind='scratch',info_gpu=0x8000001010,buffer_gpu=0x8000004000,buffer_size=49152,block_size=24)]
 def test_reconstruct(self):
  f=m.build(self.e,self.text);self.assertEqual(len(f['vertex-pds.bin']),12);self.assertEqual(len(f['descriptor-pds.bin']),24)
  self.assertEqual(struct.unpack('<QI',f['spill-info.bin']),(0x8000002000,4))
 def test_reject_untrusted_pointer_and_changed_literal(self):
  e=copy.deepcopy(self.e);e[9]['buffer_gpu']=0x7fff00112233
  with self.assertRaises(ValueError):m.build(e,self.text)
  e=copy.deepcopy(self.e);e[7]['value']=7
  with self.assertRaises(ValueError):m.build(e,self.text)
 def test_reject_unknown_entry_or_code_control(self):
  e=copy.deepcopy(self.e);e[1]['entry_type']=99
  with self.assertRaises(ValueError):m.build(e,self.text)
  e=copy.deepcopy(self.e);e[2]['value']=4
  with self.assertRaises(ValueError):m.build(e,self.text)
if __name__=='__main__':unittest.main()
