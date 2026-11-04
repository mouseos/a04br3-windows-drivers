#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('audit', ROOT / 'tools/gpu/audit-render-capture.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class CaptureAuditTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.tmp.name)
        self.addCleanup(self.tmp.cleanup)
        self.meta = self.root / 'submit-0.txt'
        self.meta.write_text('job=0 type=0 flags=3 size=24 stream=180000000000000000100000000000000000000000000000\n'
                             'map=0 handle=1 va=1000 offset=0 size=10\n'
                             'map=1 handle=1 va=1000 offset=0 size=10\n')
        (self.root / 'hwrt.txt').write_text('size=64x64 samples=1 layers=1 region=4 merge=0,0,0,0,0,0\n')
        for i in range(2):
            (self.root / f'submit-0-map-{i}.bin').write_bytes(bytes(16))

    def test_detects_duplicate_and_unexportable(self):
        report = audit.inspect(self.root, 0)
        self.assertFalse(report['fixture_exportable'])
        self.assertEqual(report['distinct_maps'], 1)
        self.assertEqual(report['duplicate_maps'], 1)
        self.assertEqual(report['geometry_address_starts_unmapped'], 0)
        self.assertNotIn('stream', str(report))

    def test_rejects_partial_dump(self):
        (self.root / 'submit-0-map-1.bin').write_bytes(bytes(15))
        with self.assertRaises(ValueError): audit.inspect(self.root, 0)

    def test_rejects_conflicting_alias(self):
        (self.root / 'submit-0-map-1.bin').write_bytes(bytes([1]) * 16)
        with self.assertRaises(ValueError): audit.inspect(self.root, 0)

    def test_rejects_gpu_address_wrap_and_duplicate_id(self):
        self.meta.write_text(self.meta.read_text().replace('va=1000 offset=0 size=10', 'va=ffffffffff offset=0 size=10', 1))
        with self.assertRaises(ValueError): audit.inspect(self.root, 0)
        self.meta.write_text(self.meta.read_text().replace('map=1', 'map=0'))
        with self.assertRaises(ValueError): audit.inspect(self.root, 0)

    def test_unmapped_vdm_and_bad_hwrt(self):
        self.meta.write_text(self.meta.read_text().replace('va=1000', 'va=2000'))
        self.assertEqual(audit.inspect(self.root, 0)['geometry_address_starts_unmapped'], 1)
        (self.root / 'hwrt.txt').write_text('size=64x64 samples=1 layers=1 region=4 merge=0,0\n')
        with self.assertRaises(ValueError): audit.inspect(self.root, 0)


if __name__ == '__main__':
    unittest.main()
