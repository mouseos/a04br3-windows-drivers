"""Mocked transport tests, never device evidence or a finalized fresh capture."""
import hashlib,importlib.util,json,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
repo=Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('runtime_assets',repo/'tools/recovery/collect-gpu-runtime-assets.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)

class Assets(unittest.TestCase):
 def exercise(self,boot='test-boot',missing=False,oversize=False):
  with tempfile.TemporaryDirectory(dir=repo/'build',prefix='runtime-assets-mock-') as tmp:
   root=Path(tmp);manifest=root/'mock-capture.json';h='1'*64;data=b'PE-test-bytes'
   manifest.write_text(json.dumps(dict(status='captured-unmounted',read_only=True,unmounted=True,errors=[],serial='MOCK',recovery_boot_id='test-boot',source_partition='/dev/block/mock',files=[dict(source_path=m.ANCHOR,sha256=h)])))
   original=manifest.read_bytes();streams=[]
   def run(args,**kw):
    a=args[3:]
    if a==['get-state']:v='recovery'
    elif a==['shell','cat','/proc/sys/kernel/random/boot_id']:v=boot
    elif a==['shell','cat','/proc/mounts']:v='/dev/block/mock '+m.MOUNT+' fuseblk ro,relatime 0 0'
    elif a[:3]==['shell','readlink','-f']:v=a[3]
    elif a[:2]==['shell','sha256sum']:v=(h if a[2].endswith('/SYSTEM') else hashlib.sha256(data).hexdigest())+'  fixed'
    elif a[:3]==['shell','sh','-c']:v='ABSENT' if missing else 'PRESENT'
    elif a[:3]==['shell','stat','-c']:v=str(m.MAX_BYTES+1 if oversize else len(data))+':1:2'
    else:raise AssertionError(args)
    return SimpleNamespace(stdout=v.encode())
   def stream(*args):streams.append(args);return data
   error=None
   try:m.collect(root,'MOCK',manifest,root/'out',run,stream)
   except ValueError as e:error=str(e)
   receipt=json.loads((root/'out/gpu-runtime-assets.json').read_text())
   self.assertEqual(manifest.read_bytes(),original)
   return receipt,error,streams
 def test_bound_generation_and_hashes(self):
  r,e,c=self.exercise();self.assertIsNone(e);self.assertTrue(r['complete']);self.assertTrue(r['same_recovery_and_SYSTEM_generation']);self.assertEqual(len(c),2)
 def test_missing_is_explicit_and_not_complete(self):
  r,e,c=self.exercise(missing=True);self.assertIsNone(e);self.assertFalse(r['complete']);self.assertEqual(len(r['missing_files']),2);self.assertFalse(c)
 def test_wrong_generation_or_oversize_never_stream(self):
  for args in [dict(boot='other-boot'),dict(oversize=True)]:
   r,e,c=self.exercise(**args);self.assertIsNotNone(e);self.assertFalse(r['complete']);self.assertFalse(c)
if __name__=='__main__':unittest.main()
