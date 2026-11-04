#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import hashlib
import importlib.util
import json
import pathlib
import tempfile
import unittest
root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('summary',root/'tools/gpu/summarize-render-csb.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class SummaryTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.p=pathlib.Path(self.tmp.name)
        self.raw=b'CONTROL STREAM DUMP\nPPP_STATE_UPDATE\n  vs_pds_data_base_addr : private_value\n  <host addr> : private_value\n'
        (self.p/'probe.log').write_bytes(self.raw)
        self.events=b''
        (self.p/'events.jsonl').write_bytes(self.events)
        (self.p/'run-manifest.json').write_text(json.dumps({'control_stream_dump':True,'probe_log_sha256':hashlib.sha256(self.raw).hexdigest(),'events_sha256':hashlib.sha256(self.events).hexdigest()}))
    def test_summary_does_not_export_private_bytes(self):
        result=m.inspect(self.p);self.assertEqual(result['dumps'],1);self.assertEqual(result['field_occurrences']['vs_pds_data_base_addr'],1)
        self.assertNotIn('private_value',str(result));self.assertFalse(result['fixture_exportable'])
    def test_digest_failure(self):
        (self.p/'probe.log').write_bytes(self.raw+b'changed')
        with self.assertRaises(ValueError):m.inspect(self.p)
if __name__=='__main__':unittest.main()
