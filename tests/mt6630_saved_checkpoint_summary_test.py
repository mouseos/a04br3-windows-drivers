#!/usr/bin/env python3
"""Saved proof/raw decoding only; no PowerShell/registry/device/VM calls."""
import hashlib,importlib.util,json,tempfile,unittest
from pathlib import Path
repo=Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('cp',repo/'tools/winpe/decode-mt6630-checkpoint.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
def record(channel,gen,parent,events=(),build=1029):
 b=bytearray(m.HEADER.size+64*m.EVENT.size)
 m.HEADER.pack_into(b,0,0x50433441,1,len(b),channel,gen,parent,0,0,len(events),0,0,0,0,0,0,0,0,build)
 for i,event in enumerate(events):m.EVENT.pack_into(b,m.HEADER.size+i*m.EVENT.size,*event)
 return b
class Summary(unittest.TestCase):
 def test_bound_current_trace_and_stale_history_exclusion(self):
  (repo/'build').mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(dir=repo/'build') as tmp:
   d=Path(tmp);r=dict(status='checkpoint-extracted-not-a-connection-diagnosis',errors=[],load_rc=0,unload_rc=0,privilege_restore_error=0,originals_unchanged=True,manifest_unchanged=True,source_manifest_sha256='a'*64,values={},selection=dict(selected_present=True,fields={'InfPath':{'value':'oem42.inf'},'DriverVersion':{'value':'2026.11.11.1029'}}))
   def put(name,b):
    (d/(name+'.bin')).write_bytes(b);r['values'][name]={'present':True,'sha256':hashlib.sha256(b).hexdigest()};(d/'receipt.json').write_text(json.dumps(r))
   put('DriverEntry',record(0,100,0));put('Initialize',record(1,101,100));put('RuntimeFailure',record(3,102,101,[(100,2,4,1,2,0),(110,2,3,1,0x51,0)]))
   result=m.summarize_receipt(d);self.assertTrue(result['selection_1029']);self.assertTrue(result['initialize_parent_bound']);self.assertTrue(result['connection_parent_bound']);self.assertTrue(result['connect_request_context_observed']);self.assertEqual(result['last_observed_connection_event']['stage'],'CONNECT_AUTH_RX');self.assertFalse(result['connection_success_claimed'])
   put('RuntimeFailure',record(3,80,69,[(100,2,4,1,2,0)],build=1011));result=m.summarize_receipt(d);self.assertFalse(result['connection_parent_bound']);self.assertIsNone(result['last_observed_connection_event'])
   put('RuntimeFailure',record(3,102,101,[(100,2,4,1,1,0)]));self.assertFalse(m.summarize_receipt(d)['connect_request_context_observed'])
   (d/'RuntimeFailure.bin').write_bytes(b'changed')
   with self.assertRaises(ValueError):m.summarize_receipt(d)
class KnownWireBuilds(unittest.TestCase):
 def test_identical_wire_size_does_not_admit_unknown_build_semantics(self):
  for build in (1029,1030,1031):
   raw=record(3,102,101,[(100,2,4,1,2,0)],build)
   self.assertEqual(len(raw),1624)
   self.assertEqual(m.decode(raw)['connection_trace'],build in (1029,1030))
  raw=record(3,102,101,build=1030);m.HEADER.pack_into(raw,0,0x50433441,2,len(raw),3,102,101,0,0,0,0,0,0,0,0,0,0,0,1030)
  with self.assertRaises(ValueError):m.decode(raw)
 def test_1030_summary_binds_only_matching_initialize_generation_and_build(self):
  (repo/'build').mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(dir=repo/'build') as tmp:
   d=Path(tmp);r=dict(status='checkpoint-extracted-not-a-connection-diagnosis',errors=[],load_rc=0,unload_rc=0,privilege_restore_error=0,originals_unchanged=True,manifest_unchanged=True,source_manifest_sha256='a'*64,values={},selection=dict(selected_present=True,fields={'DriverVersion':{'value':'2026.11.11.1030'}}))
   def put(name,b):
    (d/(name+'.bin')).write_bytes(b);r['values'][name]={'present':True,'sha256':hashlib.sha256(b).hexdigest()};(d/'receipt.json').write_text(json.dumps(r))
   put('DriverEntry',record(0,100,0,build=1030));put('Initialize',record(1,101,100,build=1030))
   for build,parent,expected in ((1030,101,True),(1029,101,False),(1031,101,False),(1030,99,False)):
    put('RuntimeFailure',record(3,102,parent,[(100,2,4,1,2,0),(117,2,1,1,0,0)],build))
    result=m.summarize_receipt(d);self.assertTrue(result['selection_1030']);self.assertEqual(result['connection_parent_bound'],expected);self.assertEqual(result['connect_request_context_observed'],expected);self.assertFalse(result['connection_success_claimed'])
if __name__=='__main__':unittest.main()
