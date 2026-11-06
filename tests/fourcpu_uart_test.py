import importlib.util, pathlib, unittest, zlib
p=pathlib.Path(__file__).resolve().parents[1]/'tools/cpu/decode-fourcpu-uart.py';spec=importlib.util.spec_from_file_location('cpu_uart',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def row(seq,cpu,observed,loops,state,elapsed,run=123):
 b=('A04CU1 '+' '.join(f'{v:08X}' for v in (run,seq,cpu,observed,loops,state,elapsed))).encode();return b+b' '+f'{zlib.crc32(b)&0xffffffff:08X}'.encode()+b'\n'
def fixture():
 start=row(0,0xffffffff,0,0,0,0)*3
 cpu=b''.join(row(seq,i,i,seq*10,3 if seq==6 else 2,60000 if seq==6 else seq*10000)*3 for seq in range(1,7) for i in range(4))
 return start+cpu+row(0xffffffff,0xffffffff,15,4,0,60100)*3
class Tests(unittest.TestCase):
 def test_complete(self):self.assertEqual(m.decode(fixture())['runs'][0]['result'],'pass_60s')
 def test_missing_cpu(self):self.assertNotEqual(m.decode(fixture().replace(row(6,2,2,60,3,60000),b''))['runs'][0]['result'],'pass_60s')
 def test_wrong_cpu(self):self.assertNotEqual(m.decode(fixture().replace(row(6,2,2,60,3,60000),row(6,2,1,60,3,60000)))['runs'][0]['result'],'pass_60s')
 def test_checksum_and_missing_repetitions(self):
  d=fixture();line=row(0xffffffff,0xffffffff,15,4,0,60100);d=d.replace(line,line[:-3]+b'00\n',1);self.assertGreater(m.decode(d)['invalid_lines'],0);self.assertNotEqual(m.decode(d)['runs'][0]['result'],'pass_60s')
 def test_conflict(self):
  d=fixture()+row(6,2,2,100,3,60000);self.assertTrue(m.decode(d)['runs'][0]['contradictory_records']);self.assertNotEqual(m.decode(d)['runs'][0]['result'],'pass_60s')
 def test_missing_progress(self):self.assertNotEqual(m.decode(fixture().replace(row(2,1,1,20,2,20000),b''))['runs'][0]['result'],'pass_60s')
 def test_short_duration(self):self.assertNotEqual(m.decode(fixture().replace(row(6,3,3,60,3,60000),row(6,3,3,60,3,59999)))['runs'][0]['result'],'pass_60s')
if __name__=='__main__':unittest.main()
