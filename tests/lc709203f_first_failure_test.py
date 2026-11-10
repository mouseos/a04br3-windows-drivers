"""固定format/世代判定。試験データはメモリ内のみ、hive/実機/buildなし。"""
import importlib.util
from pathlib import Path
import struct
import unittest
s = importlib.util.spec_from_file_location('first_failure', Path(__file__).resolve().parents[1] / 'tools/recovery/decode-lc709203f-first-failure.py')
m = importlib.util.module_from_spec(s)
s.loader.exec_module(m)

def pack(values):
    return struct.pack('<' + 'I' * len(values), *values).hex()

class FirstFailureTest(unittest.TestCase):
    def setUp(self):
        self.epoch = [m.MAGIC, 1, 123, 456]
        self.record = [m.MAGIC, 1, 96, 4, 0xc00000a3, 123, 456, 124, 456,
                       0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 5, 0, 0]
    def test_short_circuit_and_current_failure(self):
        r = m.decode(pack(self.epoch), pack(self.record))
        self.assertTrue(r['failure_observed'])
        self.assertEqual(r['NTSTATUS'], 'C00000A3')
        self.assertEqual(r['admission']['START24'], 1)
        self.assertIsNone(r['admission']['TIMING20'])
    def test_old_failure_and_absence_are_never_success(self):
        self.record[5] -= 1
        r = m.decode(pack(self.epoch), pack(self.record))
        self.assertFalse(r['failure_observed'])
        self.assertFalse(r['current_load'])
        self.assertFalse(m.decode(pack(self.epoch))['hardware_success_claimed'])
    def test_reject_success_unknown_schema_and_wrong_size(self):
        for index, value in ((4, 0), (1, 2), (15, 32), (22, 1)):
            bad = self.record.copy(); bad[index] = value
            with self.assertRaises(ValueError):
                m.decode(pack(self.epoch), pack(bad))
        with self.assertRaises(ValueError):
            m.decode(pack(self.epoch), pack(self.record) + '00')
if __name__ == '__main__':
    unittest.main()
