#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import hashlib
import importlib.util
import json
import pathlib
import struct
import tempfile
import unittest

root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('fragment',root/'tools/gpu/check-render-fragment.py')
fragment=importlib.util.module_from_spec(spec);spec.loader.exec_module(fragment)
spec=importlib.util.spec_from_file_location('generator',root/'tools/gpu/reconstruct-fragment-pds.py')
generator=importlib.util.module_from_spec(spec);spec.loader.exec_module(generator)

class FragmentTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.path=pathlib.Path(self.tmp.name)
        self.events=[dict(event='vm_create',handle=1),dict(event='bo_create',fd=1,handle=2,size=4096,initial_origin='zeroed_by_shim'),
                     dict(event='vm_map',fd=1,handle=2,vm=1,va=4096,offset=0,size=4096),
                     dict(event='bo_create',fd=1,handle=3,size=4096,initial_origin='zeroed_by_shim'),
                     dict(event='vm_map',fd=1,handle=3,vm=1,va=0xe000001000,offset=0,size=4096)]
        for base in (4352,4608):
            shader=0xe000001000 + (base-4096)
            self.events += [dict(event='upload',kind='pds_data',gpu_va=base,size=32),
                            dict(event='upload',kind='pds_code',gpu_va=base+32,size=4),
                            dict(event='upload',kind='usc_code',gpu_va=shader,size=32),
                            dict(event='fragment_shader_link',pds_data_gpu=base,pds_data_size=32,pds_data_written=32,
                                 pds_code_gpu=base+32,pds_code_size=16,pds_code_written=4,
                                 shader_gpu=shader,shader_size=64,entry_gpu=shader,doutu_offset=0,doutu_control=0,
                                 doutu_fnv64=fragment.fnv(struct.pack('<Q',shader & 0xfffffffc)),
                                 code_fnv64=fragment.fnv(struct.pack('<I',0xf4000002)))]
        self.events += [dict(event='submit_begin',count=0),dict(event='submit_end')]
        self.log=' shaderbase : 0x1100\n shaderbase : 0x1200\n'
    def runcheck(self):
        events=''.join(json.dumps(e)+'\n' for e in self.events).encode();log=self.log.encode()
        (self.path/'events.jsonl').write_bytes(events);(self.path/'probe.log').write_bytes(log)
        (self.path/'run-manifest.json').write_text(json.dumps(dict(diagnostic_manifest=dict(pds_construct_metadata_only=True),control_stream_dump=True,
             events_sha256=hashlib.sha256(events).hexdigest(),probe_log_sha256=hashlib.sha256(log).hexdigest())))
        return fragment.check(self.path)
    def test_pair_and_reject_mismatch(self):
        self.assertEqual(self.runcheck()['fragment_ppp_pairs'],2)
        self.log=' shaderbase : 0x1100\n shaderbase : 0x1300\n'
        with self.assertRaises(ValueError):self.runcheck()
    def test_reject_untraced_upload(self):
        self.events[6]['size']=8
        with self.assertRaises(ValueError):self.runcheck()
    def test_generated_padding_is_zero(self):
        self.runcheck()
        link=next(e for e in self.events if e['event']=='fragment_shader_link')
        data=generator.build(link)
        self.assertEqual(data[8:32],bytes(24));self.assertEqual(data[36:48],bytes(12))
        self.assertEqual(struct.unpack_from('<I',data,32)[0],0xf4000002)

if __name__=='__main__':unittest.main()
