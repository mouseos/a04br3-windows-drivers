#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import copy
import importlib.util
import pathlib
import unittest

spec = importlib.util.spec_from_file_location('spm', pathlib.Path(__file__).resolve().parents[1] / 'tools/gpu/check-spm-load.py')
spm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(spm)


def fixture():
    scratch, constants, pds = 0x8100000000, 0x8200000000, 0xda00001000
    rows = [{'event': 'vm_map', 'vm': 1, 'va': addr, 'size': size} for addr, size in
            ((scratch, 16384), (constants, 4096), (pds, 4096))]
    e = {'event': 'spm_load', 'scratch_gpu': scratch, 'scratch_size': 16384,
         'used': 16384, 'constants_gpu': constants, 'constants_size': 32,
         'pds_gpu': pds, 'pds_size': 16, 'width': 64, 'height': 64,
         'samples': 1, 'outputs': 1, 'tiles': 0, 'bg1': 0x1000 << 32}
    return rows + [copy.deepcopy(e) for _ in range(3)]


class TestSpm(unittest.TestCase):
    def test_fixed(self):
        result = spm.analyze(fixture())
        self.assertEqual(result['spm_loads'], 3)
        self.assertFalse(result['gpu_execution_allowed'])

    def test_reject_wrong_size_and_register(self):
        for key, value in (('used', 16385), ('bg1', 0), ('tiles', 1)):
            rows = fixture()
            rows[-1][key] = value
            with self.assertRaises(ValueError):
                spm.analyze(rows)

    def test_reject_unmapped_or_different_vm(self):
        rows = fixture()
        rows.insert(3, {'event': 'vm_unmap', 'vm': 1, 'va': rows[0]['va'], 'size': 16384})
        with self.assertRaises(ValueError):
            spm.analyze(rows)
        rows = fixture()
        rows[1]['vm'] = 2
        with self.assertRaises(ValueError):
            spm.analyze(rows)

    def test_reject_ambiguous_mapping(self):
        rows = fixture()
        rows.insert(3, {'event': 'vm_map', 'vm': 2, 'va': rows[0]['va'], 'size': 16384})
        with self.assertRaises(ValueError):
            spm.analyze(rows)


if __name__ == '__main__':
    unittest.main()
