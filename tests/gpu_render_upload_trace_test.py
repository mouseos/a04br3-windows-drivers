#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import hashlib
import importlib.util
import json
import pathlib
import tempfile
import unittest
root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('uploads',root/'tools/gpu/check-render-uploads.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class UploadTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=pathlib.Path(self.tmp.name)
        self.rows=[dict(kind=k,gpu_va=4096,size=16) for k in m.KINDS]
        self.maps=[dict(event='vm_map',va=4096,size=4096)]
    def runcheck(self):
        u=b''.join((json.dumps(r)+'\n').encode() for r in self.rows)
        e=b''.join((json.dumps(r)+'\n').encode() for r in self.maps)
        (self.root/'uploads.jsonl').write_bytes(u);(self.root/'events.jsonl').write_bytes(e)
        (self.root/'run-manifest.json').write_text(json.dumps(dict(diagnostic_manifest={'upload_metadata_only':True},uploads_sha256=hashlib.sha256(u).hexdigest(),events_sha256=hashlib.sha256(e).hexdigest())))
        return m.inspect(self.root)
    def test_maps_at_some_time_but_remains_unsafe(self):
        r=self.runcheck();self.assertFalse(r['safe_for_gpu']);self.assertEqual(sum(r['unmatched_any_time'].values()),0)
    def test_outside_or_partial(self):
        self.rows[0]['gpu_va']=8190;self.rows[0]['size']=16
        self.assertEqual(self.runcheck()['unmatched_any_time']['pds_data'],1)
    def test_overflow_and_unknown_type(self):
        self.rows[0]['gpu_va']=m.LIMIT-8
        with self.assertRaises(ValueError):self.runcheck()
        self.rows[0]['gpu_va']=4096;self.rows[0]['kind']='unknown'
        with self.assertRaises(ValueError):self.runcheck()
    def test_corrupt_digest(self):
        self.runcheck();(self.root/'uploads.jsonl').write_bytes(b'bad')
        with self.assertRaises(ValueError):m.inspect(self.root)
if __name__=='__main__':unittest.main()
