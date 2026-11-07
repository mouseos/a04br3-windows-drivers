import importlib.util
from pathlib import Path
import unittest
import zlib

spec = importlib.util.spec_from_file_location('decoder', Path(__file__).parents[1] / 'tools/recovery/decode-ge8300-copy-uart.py')
decoder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(decoder)

def line(seq, status=0, error=0, size=4096, mismatch=0, source=123, dest=123):
    body = ('A04CP1 ' + ' '.join(f'{v:08X}' for v in (42,seq,status,error,size,mismatch,source,dest))).encode()
    return body + f' {zlib.crc32(body):08X}\n'.encode()

class DecoderTest(unittest.TestCase):
    def test_complete_and_duplicates(self):
        data = b''.join(line(i)*3 for i in range(3)) + line(3, error=3, size=0, source=0, dest=0)*3
        self.assertEqual(decoder.decode(data)[0]['result'], 'pass')
        self.assertEqual(decoder.decode(data.replace(line(1), b''))[0]['result'], 'incomplete')
        self.assertEqual(decoder.decode(data + line(1, dest=124))[0]['result'], 'conflict')
    def test_failure(self):
        data = line(0, status=2, error=31, size=0) + line(1, status=2, error=1, size=0, source=0, dest=0)
        self.assertEqual(decoder.decode(data)[0]['result'], 'failure')
    def test_corruption(self):
        self.assertEqual(decoder.decode(line(0).replace(b'A04CP1', b'A04CP2')), [])

if __name__ == '__main__': unittest.main()
