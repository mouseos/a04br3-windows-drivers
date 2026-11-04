#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util
import json
import pathlib
import tempfile
import unittest

root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('trace',root/'tools/gpu/check-render-trace.py')
trace=importlib.util.module_from_spec(spec); spec.loader.exec_module(trace)

class TraceTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup)
        self.path=pathlib.Path(self.tmp.name)/'trace.jsonl'
        self.events=[
            dict(event='vm_create',handle=1),dict(event='bo_create',fd=10,handle=2,size=4096,initial_origin='zeroed_by_shim'),
            dict(event='vm_map',fd=10,vm=1,handle=2,va=4096,offset=0,size=4096),
            dict(event='context_create',handle=3,type=0,vm=1),
            dict(event='freelist_create',handle=4,vm=1,gpu=4096,max=4),
            dict(event='freelist_create',handle=5,vm=1,gpu=4112,max=4),
            dict(event='hwrt_create',handle=6,freelists=[4,5],rt=[[4128,4144,4160],[4180,4192,4208]],
                 merge=[0]*6,tpc=4096,tpc_size=16,vheap=4224,rtc=4240),
            dict(event='submit_begin',count=1),
            dict(event='job',index=0,type=0,context=3,hwrt=6,rt_index=0),
            dict(event='submit_end')]
    def runtrace(self):
        self.path.write_text(''.join(json.dumps(x)+'\n' for x in self.events))
        return trace.check(self.path)
    def test_good_still_not_exportable(self):
        r=self.runtrace(); self.assertFalse(r['fixture_exportable']); self.assertEqual(r['unresolved_address_checks'],0)
    def test_rejects_unverified_initial_content(self):
        self.events[1]['initial_origin']='unknown'
        with self.assertRaises(ValueError): self.runtrace()
    def test_fd_namespaces_and_close_reuse(self):
        self.events.insert(2,dict(event='bo_create',fd=11,handle=2,size=32,initial_origin='zeroed_by_shim'))
        self.events.insert(3,dict(event='bo_close',fd=11,handle=2))
        self.events.insert(4,dict(event='bo_create',fd=11,handle=2,size=32,initial_origin='zeroed_by_shim'))
        self.assertFalse(self.runtrace()['fixture_exportable'])
    def test_upload_liveness_and_vm_ambiguity(self):
        upload=dict(event='upload',kind='pds_code',gpu_va=4096,size=16)
        self.events.insert(3,upload)
        r=self.runtrace();self.assertEqual(r['uploads_seen'],1);self.assertEqual(r['uploads_unmapped_at_observation'],0)
        self.events.insert(3,dict(event='vm_create',handle=12))
        self.events.insert(4,dict(event='vm_map',fd=10,vm=12,handle=2,va=4096,offset=0,size=4096))
        self.assertEqual(self.runtrace()['uploads_ambiguous_vm'],1)
        upload['gpu_va']=999999
        self.assertEqual(self.runtrace()['uploads_unmapped_at_observation'],1)
    def test_missing_hwrt(self):
        self.events[8]['hwrt']=99
        with self.assertRaises(ValueError): self.runtrace()
    def test_unmap_before_submit(self):
        self.events.insert(7,dict(event='vm_unmap',vm=1,va=4096,size=4096))
        self.assertGreater(self.runtrace()['unresolved_address_checks'],0)
    def test_invalid_overlap(self):
        self.events.insert(3,dict(event='vm_map',fd=10,vm=1,handle=2,va=4100,offset=0,size=128))
        with self.assertRaises(ValueError): self.runtrace()
    def test_rejects_truncated_submit(self):
        self.events.pop()
        with self.assertRaises(ValueError): self.runtrace()
    def test_rejects_foreign_vm(self):
        self.events[4]['vm']=123
        with self.assertRaises(ValueError): self.runtrace()
if __name__=='__main__': unittest.main()
