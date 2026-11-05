#!/usr/bin/env python3
"""Check four pinned ordinary image files; perform no device operations."""
import argparse, hashlib, json
from pathlib import Path
p=argparse.ArgumentParser()
for n in ('test-boot','test-esp','restore-boot','restore-esp'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();m=json.loads(Path(__file__).with_name('ge8300-render-dual-manifest.json').read_text())
def check(path,expected,size=None):
 if not path.is_file() or (size is not None and path.stat().st_size!=size):raise SystemExit('invalid regular file: '+str(path))
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 if h.hexdigest()!=expected:raise SystemExit('hash mismatch: '+str(path))
 return h.hexdigest()
r={'test_boot':check(a.test_boot,m['candidate_boot_sha256']),'test_esp':check(a.test_esp,m['trusted_esp_sha256'],268435456),'restore_boot':check(a.restore_boot,m['restore_boot_sha256']),'restore_esp':check(a.restore_esp,m['restore_esp_sha256'],268435456),'device_touched':False,'dma_reset_proven':False}
print(json.dumps(r,indent=2))
