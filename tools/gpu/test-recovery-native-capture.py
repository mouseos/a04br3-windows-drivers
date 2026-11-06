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

class CaptureTests(unittest.TestCase):
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
