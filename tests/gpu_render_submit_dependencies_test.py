#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util
import pathlib
import unittest

root=pathlib.Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('submit',root/'tools/gpu/check-render-submit-dependencies.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)

def fixture():
    events=[{'event':'vm_map','vm':1,'va':0x1000,'size':0x100000},
            {'event':'context_create','handle':1,'type':0,'vm':1}]
    fields={'vs_pds_data_base_addr':[],'pds_data_addr':[],
            'vs_pds_code_base_addr':[],'pds_code_addr':[]}
    for i in range(2):
        b=0x1000+i*0x20000
        v,d,vc,dc,vs,info1,info2,spill,scratch,fd,fc,fs=(b+x for x in
            (0x100,0x200,0x300,0x400,0x1000,0x2000,0x2100,0x3000,0x5000,0x12000,0x12100,0x13000))
        for key,val in zip(fields,(v,d,vc,dc)):fields[key].append(val)
        events.extend([{'event':'pds_shader','parent_gpu':v,'shader_gpu':vs,'entry_gpu':vs,'size':384},
          {'event':'pds_dependency','parent_gpu':d,'kind':'spill','info_gpu':info1,'buffer_gpu':spill,'buffer_size':8192,'block_size':4},
          {'event':'pds_dependency','parent_gpu':d,'kind':'scratch','info_gpu':info2,'buffer_gpu':scratch,'buffer_size':49152,'block_size':24}])
        for addr,size,kind in ((v,12,'vertex_attributes'),(d,24,'descriptor_mappings'),(vc,12,'pds_code'),
                               (dc,8,'pds_code'),(vs,312,'usc_code'),(fd,32,'pds_data'),
                               (fc,4,'pds_code'),(fs,360,'usc_code')):
            events.append({'event':'pds_construct' if kind in ('vertex_attributes','descriptor_mappings') else 'upload',
                           'gpu_va':addr,'size':size,'kind':kind})
        events.extend([{'event':'fragment_shader_link','pds_data_gpu':fd,'pds_data_size':32,'pds_data_written':32,
                        'pds_code_gpu':fc,'pds_code_size':16,'pds_code_written':4,'shader_gpu':fs,'entry_gpu':fs,'shader_size':448},
                       {'event':'job','type':0,'context':1}])
    text='\n'.join('  %s : 0x%x'%(key,val) for key,values in fields.items() for val in values)
    return events,text

class TestSubmit(unittest.TestCase):
    def test_two_draws(self):
        events,text=fixture();result=m.analyze(events,text)
        self.assertEqual(result['fixed_draws_checked'],2);self.assertFalse(result['gpu_execution_allowed'])
    def test_unmap_remap_cannot_restore_upload(self):
        events,text=fixture();i=next(i for i,e in enumerate(events) if e['event']=='job')
        events[i:i]=[{'event':'vm_unmap','vm':1,'va':0x1000}, {'event':'vm_map','vm':1,'va':0x1000,'size':0x100000}]
        with self.assertRaises(ValueError):m.analyze(events,text)
    def test_overwrite_invalidates_code(self):
        events,text=fixture();i=next(i for i,e in enumerate(events) if e['event']=='job')
        code=next(e for e in events if e.get('kind')=='pds_code')
        events.insert(i,{'event':'upload','kind':'pds_data','gpu_va':code['gpu_va'],'size':4})
        with self.assertRaises(ValueError):m.analyze(events,text)
    def test_missing_or_reordered_job(self):
        events,text=fixture();events=[e for e in events if e['event']!='job']
        with self.assertRaises(ValueError):m.analyze(events,text)
if __name__=='__main__':unittest.main()
