#!/usr/bin/env python3
"""TLV parserのCPU-only試験。zero-filled fixtureはGPU packetではなく送信もしない。"""
import importlib.util
import struct
import unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location("capture_parser",Path(__file__).with_name("read-recovery-native-capture.py"))
parser=importlib.util.module_from_spec(spec);spec.loader.exec_module(parser)

def event(kind,seq,items):
    return struct.pack("<8sIIIiiI",b"A04RGCAP",1,kind,seq,0,0,len(items))+b"".join(
        struct.pack("<II",tag,len(data))+data for tag,data in items)

def format_only_pairs(seq=1):
    # The parser checks framing, not GPU validity. These contain no valid task.
    return event(1,seq,[(1,bytes(32)),(2,bytes(0x160)),(3,bytes(0x88)),(4,b""),(5,bytes(0x150))])+event(2,seq,[(6,bytes(12))])

def resource(status=0,data=b"\x11\x22\x33\x44",seq=1):
    meta=struct.pack("<II7Q4IQQII",1,status,1,0x1000,0xa800010000,0,max(4096,len(data)),max(4096,len(data)),
        len(data),len(data),1,1,0,1,2,10,10)
    return event(3,seq,[(7,meta),(8,data)])

def sparse_fixture(index=7,offset=7*4096,parent_offset=0,cpu_offset=0):
    inp=struct.pack("<QQIIQ",1024*1024,4096,1,256,0x12340000)+bytes(24)
    out=struct.pack("<QI",0x31,0)
    journal=event(4,0,[(9,struct.pack("<6I",6,9,10,56,12,0)+inp+out)])
    witness=event(6,0,[(11,struct.pack("<IIQQQQIIQ",1,10,1,0x31,1024*1024,4096,1,256,0x12340000)+struct.pack("<I",index))])
    meta=struct.pack("<II7Q4IQQII",2,8,0x31,0x100000+offset-cpu_offset,
        0x65c01db000+offset-parent_offset,offset,1024*1024,1024*1024,4096,4096,1,1,0,1,2,10,10)
    meta+=struct.pack("<7Q",1,0x65c01db000,parent_offset,1024*1024,4096,0x100000,cpu_offset)
    return journal+witness,event(3,1,[(7,meta),(8,bytes(4096))])

class CaptureTests(unittest.TestCase):
    def test_sparse_nonzero_index_and_parent_offset(self):
        witness,data=sparse_fixture(parent_offset=4096,cpu_offset=2*4096)
        result=parser.parse(witness+data+format_only_pairs())
        r=result["events"][2]
        self.assertEqual(r["pmr_offset"],7*4096)
        self.assertEqual(r["gpu_va"],hex(0x65c01db000+6*4096))
        self.assertEqual(r["reason"],"backed-interval")
        self.assertTrue(r["coverage"].endswith("unknown"))
    def test_sparse_hole_and_missing_witness_are_rejected(self):
        witness,data=sparse_fixture(index=3)
        with self.assertRaises(ValueError):parser.parse(witness+data+format_only_pairs())
        with self.assertRaises(ValueError):parser.parse(data+format_only_pairs())
    def test_sparse_outside_vma_and_parent_are_rejected(self):
        witness,data=sparse_fixture(cpu_offset=8*4096)
        with self.assertRaises(ValueError):parser.parse(witness+data+format_only_pairs())
        witness,data=sparse_fixture(parent_offset=8*4096)
        with self.assertRaises(ValueError):parser.parse(witness+data+format_only_pairs())
    def test_sparse_topology_requires_successful_matching_allocation(self):
        witness,data=sparse_fixture()
        broken=bytearray(witness);struct.pack_into("<I",broken,40+24+56+8,1)
        with self.assertRaises(ValueError):parser.parse(bytes(broken)+data+format_only_pairs())
    def test_connect_packed17_and_partial_is_not_capture_success(self):
        inp=struct.pack("<4I",0,0x2010,0x1000b,5425693)
        out=struct.pack("<B4I",2,0,1,2,0)
        raw=event(4,0,[(9,struct.pack("<6I",1,0,10,16,17,0)+inp+out)])
        with self.assertRaises(ValueError):parser.parse(raw)
        result=parser.parse(raw,allow_incomplete=True)
        self.assertFalse(result["complete_capture"])
        self.assertEqual(result["events"][0]["client"]["ddk_version"],"1.11")
        self.assertEqual(result["events"][0]["client"]["ddk_build"],5425693)
        self.assertEqual(result["events"][0]["bridge_error"],0)
        self.assertTrue(result["gpu_success"].startswith("unknown"))
    def test_resource_is_not_gpu_success(self):
        result=parser.parse(resource()+format_only_pairs())
        self.assertEqual(result["resource_bytes"],4)
        self.assertTrue(result["gpu_success"].startswith("unknown"))
        self.assertTrue(result["resource_reference_coverage"].startswith("unknown"))
        self.assertEqual(result["events"][0]["gpu_va"],"0xa800010000")
    def test_unread_status_cannot_carry_data(self):
        with self.assertRaises(ValueError):parser.parse(resource(2)+format_only_pairs())
    def test_resource_needs_matching_kick(self):
        with self.assertRaises(ValueError):parser.parse(resource())
    def test_oversized_resource_is_rejected(self):
        with self.assertRaises(ValueError):parser.parse(resource(data=bytes(0x40004))+format_only_pairs())
    def test_kick_read_limit(self):
        with self.assertRaises(ValueError):parser.parse(resource(data=bytes(0x40000))*65+format_only_pairs())
    def test_process_read_limit(self):
        raw=b"".join(resource(data=bytes(0x40000),seq=seq)*16+format_only_pairs(seq) for seq in range(1,5))
        with self.assertRaises(ValueError):parser.parse(raw+resource(data=bytes(0x40000),seq=5)+format_only_pairs(5))
    def test_memory_summary_checks_actual_reads(self):
        summary=event(5,1,[(10,struct.pack("<8I",1,0,8,8,1,1,1,1))])
        with self.assertRaises(ValueError):parser.parse(resource()+summary+format_only_pairs())
    def test_incomplete_post_is_rejected(self):
        with self.assertRaises(ValueError):parser.parse(format_only_pairs()[:-1])
    def test_bridge_journal_has_bounded_source_buffers(self):
        journal=event(4,0,[(9,struct.pack("<6I",6,9,10,65,0,0)+bytes(65))])
        with self.assertRaises(ValueError):parser.parse(journal+format_only_pairs())

if __name__=="__main__":unittest.main()
