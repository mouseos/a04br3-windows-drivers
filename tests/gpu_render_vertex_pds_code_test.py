#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import hashlib,importlib.util,json,pathlib,struct,tempfile,unittest
root=pathlib.Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('code',root/'tools/gpu/check-vertex-pds-code.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class TestCode(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.p=pathlib.Path(self.temp.name)
        rows=[];lines=[]
        for idx,(field,words) in enumerate(m.CODE.items()):
            raw=struct.pack('<%dI'%len(words),*words)
            for i in range(2):
                addr=4096+idx*256+i*32;lines.append('%s : 0x%x'%(field,addr))
                rows.append({'gpu_va':addr,'size':len(raw),'fnv64':m.fnv(raw)})
        data=('\n'.join(map(json.dumps,rows))+'\n').encode();log=('\n'.join(lines)+'\n').encode()
        (self.p/'pds-code-hashes.jsonl').write_bytes(data);(self.p/'probe.log').write_bytes(log)
        self.manifest={'mesa_build_manifest':{'mesa_commit':m.PIN},'diagnostic_manifest':{'sipf1_patch':True,'pds_code_hash_metadata_only':True},
         'control_stream_dump':True,'pds_code_hashes_sha256':hashlib.sha256(data).hexdigest(),'probe_log_sha256':hashlib.sha256(log).hexdigest()}
        (self.p/'run-manifest.json').write_text(json.dumps(self.manifest))
    def test_matches_without_addresses(self):
        r=m.check(self.p);self.assertEqual(r['draws'],2);self.assertFalse(r['gpu_execution_allowed']);self.assertNotIn('4096',str(r))
    def test_tamper_fails(self):
        (self.p/'pds-code-hashes.jsonl').write_text('changed')
        with self.assertRaises(ValueError):m.check(self.p)
if __name__=='__main__':unittest.main()
