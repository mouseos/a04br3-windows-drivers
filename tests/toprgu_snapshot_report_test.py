#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import struct
import tempfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('snapshot_report', root / 'tools/recovery/report-toprgu-snapshots.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
with tempfile.TemporaryDirectory(dir=root / 'build/tmp') as tmp:
    path = Path(tmp) / 'hive'
    header = bytearray(4096)
    header[:4] = b'regf'
    struct.pack_into('<II', header, 4, 8, 7)
    struct.pack_into('<II', header, 0x14, 1, 5)
    path.write_bytes(header)
    result = mod.snapshot(path)
    assert result['bytes'] == 4096 and result['regf']['version'] == '1.5'
    assert result['regf']['sequences_equal'] is False
    link = Path(tmp) / 'outside'
    link.symlink_to('/etc/passwd')
    try:
        mod.snapshot(link)
        raise AssertionError('outside input accepted')
    except ValueError:
        pass
print('snapshot report: header and outside-workspace rejection passed')
