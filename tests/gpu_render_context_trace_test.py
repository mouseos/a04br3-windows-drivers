#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import copy
import importlib.util
import pathlib
import unittest
root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('ctx',root/'tools/gpu/check-render-context.py')
ctx=importlib.util.module_from_spec(spec);spec.loader.exec_module(ctx)

class ContextTraceTest(unittest.TestCase):
    def setUp(self):
        self.events=[dict(event='vm_map',vm=1,va=4096,size=8192)]
        for n,(kind,index) in enumerate(sorted(ctx.EXPECTED)):
            addr=4096+n*128
            upload='usc_code' if kind.endswith('_usc') else 'pds_code' if kind.endswith('_code') else 'pds_data' if kind.endswith('_data') else None
            if upload:self.events.append(dict(event='upload',kind=upload,gpu_va=addr,size=16))
            self.events.append(dict(event='ctx_component',kind=kind,index=index,gpu_va=addr,size=64))
        self.events.extend([dict(event='context_create',type=0,handle=9,vm=1),dict(event='job',type=0,context=9)])
    def test_live_components(self):
        r=ctx.analyze(self.events);self.assertEqual(r['components'],26);self.assertEqual(r['render_jobs'],1)
        self.assertFalse(r['gpu_execution_allowed'])
    def test_missing_upload_and_early_unmap(self):
        e=copy.deepcopy(self.events);n=next(i for i,x in enumerate(e) if x['event']=='upload');e.pop(n)
        with self.assertRaises(ValueError):ctx.analyze(e)
        e=copy.deepcopy(self.events);e.insert(-1,dict(event='vm_unmap',vm=1,va=4096))
        with self.assertRaises(ValueError):ctx.analyze(e)
    def test_overwrite_invalidates_upload(self):
        e=copy.deepcopy(self.events);u=next(x for x in e if x['event']=='upload')
        e.insert(-1,dict(event='upload',kind='pds_data',gpu_va=u['gpu_va'],size=4))
        with self.assertRaises(ValueError):ctx.analyze(e)
if __name__=='__main__':unittest.main()
