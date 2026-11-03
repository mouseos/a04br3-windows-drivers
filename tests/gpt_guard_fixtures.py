#!/usr/bin/env python3
"""Independently generate GPT cases with Python's UUID and CRC implementation."""
import pathlib
import struct
import subprocess
import tempfile
import uuid
import zlib

basic = uuid.UUID('ebd0a0a2-b9e5-4433-87c0-68b6b72699c7').bytes_le
esp = uuid.UUID('c12a7328-f81f-11d2-ba4b-00a0c93ec93b').bytes_le


def fixture(*, duplicate=False, overlap=False, duplicate_esp=False, bad_esp_type=False, missing_esp=False):
    entries = bytearray(128 * 128)
    def entry(index, name, start, last, kind=basic):
        off = index * 128
        entries[off:off+16] = kind
        entries[off+16:off+32] = uuid.UUID(int=index + 1).bytes_le
        struct.pack_into('<QQ', entries, off+32, start, last)
        label = name.encode('utf-16le')
        entries[off+56:off+56+len(label)] = label
    entry(0, 'Windows', 512, 2047)
    entry(1, 'Windows' if duplicate else 'firmware', 2048 if not overlap else 2000, 3071)
    if not missing_esp:
        entry(2, 'EFI', 128, 255, basic if bad_esp_type else esp)
    if duplicate_esp:
        entry(3, 'EFI', 256, 383, esp)
    h = bytearray(512)
    struct.pack_into('<8sIIIIQQQQ16sQIII', h, 0, b'EFI PART', 0x10000, 92, 0, 0,
                     1, 4095, 34, 4062, uuid.UUID(int=42).bytes_le,
                     2, 128, 128, zlib.crc32(entries))
    struct.pack_into('<I', h, 16, zlib.crc32(h[:92]))
    return h + entries


with tempfile.TemporaryDirectory() as tmp:
    p = pathlib.Path(tmp) / 'gpt.bin'
    cases = [(fixture(), 0), (fixture(duplicate=True), 1), (fixture(overlap=True), 1),
             (fixture(duplicate_esp=True), 1), (fixture(bad_esp_type=True), 1),
             (fixture(missing_esp=True), 1)]
    bad_header = fixture(); bad_header[24] ^= 1; cases.append((bad_header, 1))
    bad_array = fixture(); bad_array[600] ^= 1; cases.append((bad_array, 1))
    for data, expected in cases:
        p.write_bytes(data)
        actual = subprocess.run(['./build/gpt_guard_test', str(p)], check=False).returncode
        if actual != expected:
            raise SystemExit(f'GPT guard result {actual}, expected {expected}')
