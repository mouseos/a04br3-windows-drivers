#!/usr/bin/env python3
import importlib.util, pathlib, unittest
p=pathlib.Path(__file__).resolve().parents[1]/'tools/uart/analyze-ge8300-copy.py'
spec=importlib.util.spec_from_file_location('copy_uart',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
GOOD=(b'A04BR3 recovery: EBS\nA04VOL_LISTENER_READY\nA04GPU NATIVE POST ENTER\n'
 b'A04GPU NATIVE POWER=00000000\nA04GPU NATIVE START STATUS=00000000\nA04GPU NATIVE FENCE=00000000\n'
 + b''.join(b'A04GPU CONTEXT INDEX=%08X STATUS=00000000\n'%n for n in range(4))
 +b'A04GPU COPY STATUS=00000000 MATCH=00000400 GUARD=00000000 PHASE=00000004 UFO=00000001 EXPECT=00000001\n'
 +b'A04GPU CONTEXT CLEANUP STATUS=00000000 FAILED=00000004 MASK=0000000F\nA04GPU FORCED IDLE=00000000\nA04GPU NATIVE OFF=00000000\nA04VOL_RELEASED\n')
class CopyTest(unittest.TestCase):
 def test_valid_is_not_dma_proof(self):
  r=m.analyze(GOOD);self.assertEqual(r['判定'],'一致');self.assertTrue(r['解除']['完全受信']);self.assertFalse(r['復元ゲート']['自動resetまたは書込み'])
 def test_copy_guards(self):
  for a,b in [(b'MATCH=00000400',b'MATCH=000003FF'),(b'GUARD=00000000',b'GUARD=00000001'),(b'PHASE=00000004',b'PHASE=00000003'),(b'UFO=00000001',b'UFO=00000000'),(b'EXPECT=00000001',b'EXPECT=00000000')]:
   with self.subTest(field=a):self.assertEqual(m.analyze(GOOD.replace(a,b))['判定'],'失敗')
 def test_cleanup_and_fence(self):
  for a,b in [(b'NATIVE FENCE=00000000',b'NATIVE FENCE=C0000001'),(b'MASK=0000000F',b'MASK=00000007'),(b'FORCED IDLE=00000000',b'FORCED IDLE=C0000001')]:
   self.assertEqual(m.analyze(GOOD.replace(a,b))['判定'],'失敗')
 def test_missing_collision_reboot(self):
  self.assertEqual(m.analyze(GOOD.replace(b'A04GPU NATIVE FENCE=00000000\n',b''))['判定'],'不明')
  self.assertEqual(m.analyze(GOOD.replace(b'MATCH=00000400',b'MATAGPU00400'))['判定'],'失敗')
  self.assertEqual(m.analyze(GOOD+b'BOOT_REASON=1\n')['判定'],'失敗')
  self.assertEqual(m.analyze(GOOD+b'A04GPU NATIVE POST ENTER\n')['判定'],'失敗')
 def test_release_strict(self):
  for v in (GOOD.replace(b'A04VOL_RELEASED\n',b'A04VOL_RELEA'), GOOD.replace(b'A04VOL_LISTENER_READY\n',b''),GOOD+b'A04VOL_RELEASE_UNKNOWN\n'):
   self.assertFalse(m.analyze(v)['解除']['完全受信'])
if __name__=='__main__':unittest.main()
