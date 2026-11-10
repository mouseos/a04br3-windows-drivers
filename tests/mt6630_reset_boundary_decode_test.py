#!/usr/bin/env python3
"""In-memory wire decoding only: no driver build, registry or hardware."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('checkpoint_decoder',
    Path(__file__).resolve().parents[1] / 'tools/winpe/decode-mt6630-checkpoint.py')
cp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cp)


def record(events):
    size = cp.HEADER.size + cp.CAPACITY * cp.EVENT.size
    header = cp.HEADER.pack(0x50433441, 1, size, 3, 806, 805, 1, 2,
        len(events), 0, 0, 0, 0, 0, 0, 0, 0, 1030)
    return header + b''.join(cp.EVENT.pack(*event) for event in events) + bytes(
        (cp.CAPACITY - len(events)) * cp.EVENT.size)


class ResetBoundaryDecode(unittest.TestCase):
    def test_failure_is_signed_and_later_boundaries_are_not_invented(self):
        result = cp.decode(record([(128, 2, 3, 1, 0xffffffff, 0x1701)]))
        self.assertEqual((result['generation'], result['parent_generation']), (806, 805))
        self.assertFalse(result['overflow'])
        self.assertEqual(len(result['events']), 1)
        value = result['events'][0]['reset_boundary']
        self.assertEqual(value['backend_return'], -1)
        self.assertEqual(value['boundary'], 'radio_reset')
        self.assertTrue(value['entry_hci_open'])
        self.assertFalse(value['entry_sco_held'])

    def test_three_boundaries_and_unavailable_broker(self):
        result = cp.decode(record([(128, 2, 3, 1, 0, phase | 0x300) for phase in (1, 2, 3)]))
        self.assertEqual([e['reset_boundary']['boundary'] for e in result['events']],
            ['radio_reset', 'default_mib_profile', 'final_idle'])
        self.assertTrue(all(e['reset_boundary']['entry_hci_open'] is None for e in result['events']))

    def test_legacy_record_and_unknown_boundary(self):
        self.assertEqual(cp.decode(record([(101, 2, 1, 1, 0, 0x0e010192)]))['events'][0]['stage'], 'CONNECT_OID')
        for event in ((128, 2, 3, 1, 0, 5), (128, 2, 3, 1, 0, 0x2001), (128, 2, 1, 1, 0, 1)):
            with self.assertRaises(ValueError):
                cp.decode(record([event]))


if __name__ == '__main__':
    unittest.main()
